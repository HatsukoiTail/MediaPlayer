


using System;
using System.Diagnostics;
using System.Threading;
using AvaloniaMedia.FFmpeg.Filter;
using AvaloniaMedia.FFmpeg.Model;
using AvaloniaMedia.FFmpeg.Queue;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Decode;

public unsafe class AudioDecoder : Decoder
{

    protected enum DecodeResult { Success, Error, Eof, Flush, Continue }

    public override FrameQueue FrameQueue { get; } = new();
    public override bool IsEof { get; protected set; } = false;
    public override int FinishSerial { get; protected set; }

    private readonly AudioFilter audioFilter = new();

    public int SampleRate => audioFilter.IsValid ? audioFilter.SampleRate : CodecContextPointer->sample_rate;
    public int ChannelCount => audioFilter.IsValid ? audioFilter.ChannelCount : CodecContextPointer->ch_layout.nb_channels;
    public AVSampleFormat SampleFormat => audioFilter.IsValid ? audioFilter.SampleFormat : CodecContextPointer->sample_fmt;

    public double Speed { get; set; } = 1.0;

    private int lastPacketSerial = -1;
    private int lastFilterSerial = -1;
    private double lastFilterSpeed = 1.0;

    protected override void Decode(CancellationToken token)
    {
        using var frame = Frame.CreateFrame();
 
        while (token.IsCancellationRequested == false)
        {
            var result = DecodeFrame(frame, token);

            if (result == DecodeResult.Error)
            {
                break;
            }
            if (result == DecodeResult.Eof)
            {
                // 当解码完成后，继续进入DecodeFrame，由于PacketQueue为空，因此会阻塞在Dequeue阶段
                continue;
            }

            var handleResult = HandleFrame(frame, token);
            if (handleResult == false)
            {
                break;
            }
        }
    }

    private DecodeResult DecodeFrame(Frame frame, CancellationToken token)
    {
        Debug.Assert(PacketQueue != null);
        while (token.IsCancellationRequested == false)
        {
            // 进入循环时，如果发现上次Packet的Serial与当前的主Serial不同，那么不能先接收帧，而是首先刷新解码器
            if (lastPacketSerial != PacketQueue.Serial && lastPacketSerial >= 0)
            {
                // 刷新解码器
                ffmpeg.avcodec_flush_buffers(CodecContextPointer);
                FinishSerial = 0;
            }

            // 进入循环先尝试接收帧，因为某些情况下送入一个Packet，能够接收到多个帧
            // 如果之前刚刷新过解码器，此处的返回值应该为AVERROR(EAGAIN)
            int result = ffmpeg.avcodec_receive_frame(CodecContextPointer, frame.FramePointer);
            if (result >= 0)
            {
                return DecodeResult.Success;
            }
            else if (result == ffmpeg.AVERROR_EOF)
            {
                FinishSerial = lastPacketSerial;
                ffmpeg.avcodec_flush_buffers(CodecContextPointer);
                return DecodeResult.Eof;
            }
            else if (result == ffmpeg.AVERROR(ffmpeg.EAGAIN))
            {
                // 说明没有可用的帧，需要向解码器发送Packet
            }
            else
            {
                // 其他错误，抛出异常
                throw new FFmpegException(result, "Failed to receive frame from decoder.");
            }

            // 重复循环直到取出最新的Packet
            Packet? packet = null;
            while (token.IsCancellationRequested == false)
            {
                // 取出新的Packet
                if (PacketQueue.IsEmpty)
                {
                    PacketQueue.Awake();
                }

                packet = PacketQueue.Dequeue();

                if (packet == null)
                {
                    // 阻塞模式下取出的包如果为空，说明PacketQueue已Abort
                    return DecodeResult.Error;
                }

                if (lastPacketSerial != packet.Serial && lastPacketSerial > 0)
                {
                    // 包的序列号与当前的上次送入解码器的包的序列号不同
                    ffmpeg.avcodec_flush_buffers(CodecContextPointer);
                    FinishSerial = -1;
                }

                lastPacketSerial = packet.Serial;

                if (packet.Serial == PacketQueue.Serial)
                {
                    break;
                }
                else
                {
                    packet.Dispose(); // 销毁过期的Packet
                }
            }

            Debug.Assert(packet != null);

            result = ffmpeg.avcodec_send_packet(CodecContextPointer, packet.PacketPointer);
            packet.Dispose();
            if (result < 0 && result != ffmpeg.AVERROR(ffmpeg.EAGAIN) && result != ffmpeg.AVERROR_EOF)
            {
                throw new FFmpegException(result, "Failed to send packet to decoder.");
            }
        }

        return DecodeResult.Error;
    }


    private bool HandleFrame(Frame frame, CancellationToken token)
    {
        FormatFrameTime(frame);

        // 检查是否需要配置滤镜
        if (FilterOutdated(frame))
        {
            var filterArgs = BuildAtempoFilterString(Speed);
            var options = new AudioFilter.Options
            {
                ThreadCount = 4,
                SwrOpts = string.Empty,
                ChannelLayout = &frame.FramePointer->ch_layout,
                SampleFormat = (AVSampleFormat)frame.Format,
                SampleRate = frame.SampleRate,
                FilterArgs = filterArgs
            };
            audioFilter.Build(options);
            lastFilterSerial = lastPacketSerial;
            lastFilterSpeed = Speed;
        }

        return FilterFrame(frame, token);
    }


    private bool FilterOutdated(Frame frame)
    {
        if (audioFilter.ChannelCount == 1 && frame.ChannelCount == 1)
        {
            if (ffmpeg.av_get_packed_sample_fmt(audioFilter.SampleFormat) != ffmpeg.av_get_packed_sample_fmt((AVSampleFormat)frame.Format))
            {
                return true;
            }
        }
        else
        {
            if (audioFilter.ChannelCount != frame.ChannelCount || audioFilter.SampleFormat != (AVSampleFormat)frame.Format)
            {
                return true;
            }
        }
        if (ffmpeg.av_channel_layout_compare(audioFilter.ChannelLayout, &frame.FramePointer->ch_layout) != 0)
        {
            return true;
        }
        if (audioFilter.SampleRate != frame.SampleRate)
        {
            return true;
        }
        if (lastPacketSerial != lastFilterSerial)
        {
            return true;
        }
        if (Math.Abs(lastFilterSpeed - Speed) > 0.001)
        {
            return true;
        }
        return false;
    }

    /// <summary>
    /// 接管一个<see cref="Frame"/>, 对其进行滤镜处理, 并将结果存入<see cref="FrameQueue"/>. 调用该函数后, Frame会被释放
    /// </summary>
    /// <returns>
    /// <see cref="DecodeResult.Success"/>: 成功处理一帧; 
    /// <see cref="DecodeResult.Error"/>: 发生错误，token被取消; 
    /// </returns>
    /// <exception cref="FFmpegException"></exception>
    private bool FilterFrame(Frame frame, CancellationToken token)
    {
        
        int result = ffmpeg.av_buffersrc_add_frame(audioFilter.InputContext, frame.FramePointer);
        if (result < 0)
        {
            throw new FFmpegException(result, "Failed to add frame to filter.");
        }
        while (token.IsCancellationRequested == false)
        {
            result = ffmpeg.av_buffersink_get_frame_flags(audioFilter.OutputContext, frame.FramePointer, 0);

            if (result == ffmpeg.AVERROR_EOF)
            {
                FinishSerial = lastPacketSerial;
                return true;
            }
            else if (result == ffmpeg.AVERROR(ffmpeg.EAGAIN))
            {
                return true;
            }
            else if (result < 0)
            {
                throw new FFmpegException(result, "Failed to get frame from filter.");
            }

            var timeBase = ffmpeg.av_buffersink_get_time_base(audioFilter.OutputContext);

            frame.Time = frame.Pts == ffmpeg.AV_NOPTS_VALUE ? double.NaN : frame.Pts * ffmpeg.av_q2d(timeBase);
            // frame.Speed = Speed;
            frame.Serial = lastPacketSerial;
            frame.Duration = (double)frame.SampleNum / frame.SampleRate;

            var enqueueResult = FrameQueue.Enqueue(frame);
            if (enqueueResult == false)
            {
                return false;
            }

            if (PacketQueue!.Serial != lastPacketSerial)
            {
                // 退出循环，同时返回true表面未发生错误，成功消费了Frame
                return true;
            }
        }

        return false;
    }

    public void InvalidateFilter()
    {
        lastFilterSerial = -1;
    }

    private static string BuildAtempoFilterString(double speed)
    {
        if (Math.Abs(speed - 1.0) < 0.001)
            return string.Empty;

        // atempo parameter range is [0.5, 2.0]; chain multiple for wider range
        var parts = new System.Collections.Generic.List<string>();
        double remaining = speed;
        while (remaining > 2.0)
        {
            parts.Add("atempo=2.0");
            remaining /= 2.0;
        }
        while (remaining < 0.5)
        {
            parts.Add("atempo=0.5");
            remaining /= 0.5;
        }
        if (Math.Abs(remaining - 1.0) > 0.001)
            parts.Add($"atempo={remaining:F3}");

        return string.Join(",", parts);
    }

    private long nextPts = ffmpeg.AV_NOPTS_VALUE;
    private AVRational nextPtsTimeBase = new() { num = 0, den = 0 };

    private void FormatFrameTime(Frame frame)
    {
        AVRational timeBase;
        timeBase.den = frame.SampleRate;
        timeBase.num = 1;
        if (frame.Pts != ffmpeg.AV_NOPTS_VALUE)
        {
            frame.FramePointer->pts = ffmpeg.av_rescale_q(frame.Pts, TimeBase, timeBase);
        }
        else if (nextPts != ffmpeg.AV_NOPTS_VALUE)
        {
            frame.FramePointer->pts = ffmpeg.av_rescale_q(nextPts, nextPtsTimeBase, timeBase);
        }
        else
        {
            // do nothing
        }

        if (frame.Pts != ffmpeg.AV_NOPTS_VALUE)
        {
            nextPts = frame.Pts + frame.SampleNum;
            nextPtsTimeBase = timeBase;
        }
    }

    protected override void Dispose(bool disposing)
    {
        base.Dispose(disposing);

        FrameQueue.Dispose();
        audioFilter.Dispose();

        Console.WriteLine("Audio decoder exited");
    }
}