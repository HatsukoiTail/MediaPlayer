


using System;
using System.Diagnostics;
using System.Threading;
using AvaloniaMedia.FFmpeg.Model;
using AvaloniaMedia.FFmpeg.Queue;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Decode;

public unsafe class SubtitleDecoder : Decoder
{
    protected enum DecodeResult { Success, Error, Eof }

    public override FrameQueue FrameQueue { get; } = new();
    public override bool IsEof { get; protected set; } = false;
    public override int FinishSerial { get; protected set; }

    protected override void Decode(CancellationToken token)
    {
        var frame = Frame.CreateFrame();

        try
        {
            while (token.IsCancellationRequested == false)
            {
                var result = DecodeFrame(ref frame, token);

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
                if (handleResult == DecodeResult.Error)
                {
                    break;
                }
                // 否者一帧被正确处理，继续循环
                frame = Frame.CreateFrame();
            }
        }
        finally
        {
            frame.Dispose();
        }
    }

    private bool flushing = false;

    private DecodeResult DecodeFrame(ref Frame frame, CancellationToken token)
    {
        Debug.Assert(PacketQueue != null);

        // 由于该函数退出时
        Packet? packet = flushing ? Packet.CreatePacket() : null;

        while (token.IsCancellationRequested == false)
        {
            // 如果没有暂存的包，并且PacketQueue为空，说明需要新的包，因此唤醒生产者线程
            if (!flushing && PacketQueue.IsEmpty)
            {
                if (PacketQueue.IsAborted)
                    return DecodeResult.Error;
                PacketQueue.Awake();
                Thread.Yield();
                continue;
            }
            else if (flushing)
            {
                // 先将packet标记为已使用
                flushing = false;
            }
            else
            {
                // 没有暂存的包，并且队列不为空，因此取出一个包
                packet = PacketQueue.Dequeue();
                // 如果取出的包是null (队列被Abort)或NullPacket，则返回
                if (packet == null || packet.IsNull)
                {
                    return DecodeResult.Error;
                }
            }

            // 此处packet一定不为NullPacket，并且packetUnused一定为false
            Debug.Assert(packet != null && flushing == false);

            // 此处应比较取出包的序列号与上次取出包的序列号(或主序列号)是否相同
            // 若不同，说明这段时间内PacketQueue发生了Flush，即外部执行了跳转，因此需要刷新解码器
            if (packet.Serial != PacketQueue.Serial)
            {
                ffmpeg.avcodec_flush_buffers(CodecContextPointer);
                IsEof = false;
                FinishSerial = 0;
                packet.Dispose();
                continue;
            }

            // 解码字幕
            int gotSubtitle = 0;
            int result = ffmpeg.avcodec_decode_subtitle2(CodecContextPointer, frame.SubtitlePointer, &gotSubtitle, packet.PacketPointer);
            bool isFlushPacket = packet.PacketPointer->data == null;
            packet.Dispose();

            if (result < 0)
            {
                // 如果结果小于0，则可能是AVERROR(EAGAIN)或者发生了错误(此处推测avcodec_decode_subtitle2并不会返回AVERROR_EOF)
                // 如果为AVERROR(EAGAIN)，那么取出更多包即可解码字幕
                // 如果发生了错误，则跳过当前的字幕，解码下一段字幕
                continue;
            }

            // 成功解码字幕
            if (gotSubtitle != 0)
            {
                // 如果avcodec_decode_subtitle2显示成功获取Subtitle，但是Packet->data为null，
                // 说明这个包是解复用线程放入的FlushPacket，
                // 因此将packetUnused标记为true，同时释放Packet，这样下次调用该函数将继续使用FlushPacket以取出剩余的Subtitle
                if (isFlushPacket)
                {
                    flushing = true;
                    frame.Serial = packet.Serial;
                    return DecodeResult.Success;
                }
                // 否则解码成功，但是Packet不是FlushPacket，
                // 这是正常路径，继续取出包并解码即可
                else
                {
                    // 成功解码字幕，返回
                    // 同时保持packetUnused为false
                    frame.Serial = packet.Serial;
                    return DecodeResult.Success;
                }
            }
            else
            {
                // 如果解码未成功(gotSubtitle == 0)，并且当前Packet是FlushPacket
                // 说明已经解码完成，
                if (isFlushPacket)
                {
                    // 标记当前解码器为已完成状态，
                    // 刷新解码器
                    ffmpeg.avcodec_flush_buffers(CodecContextPointer);
                    IsEof = true;
                    FinishSerial = packet.Serial;
                    return DecodeResult.Eof;
                }
                // 否则，解码未成功，并且当前Packet不是FlushPacket，
                // 说明解码器需要更多数据，进入下一次循环继续读取Packet并解码
                else
                {
                    continue;
                }
            }
        }

        // 正常路径不会经过此处返回，仅在CancellationToken被取消时才会在此返回
        return DecodeResult.Error;
    }

    private DecodeResult HandleFrame(Frame frame, CancellationToken token)
    {
        if (frame.SubtitlePointer == null)
            return DecodeResult.Success;

        if (frame.SubtitlePointer->format != 0)
        {
            return DecodeResult.Success;
        }

        frame.FramePointer->width = CodecContextPointer->width;
        frame.FramePointer->height = CodecContextPointer->height;
        frame.Uploaded = false;

        long subtitlePts = frame.SubtitlePointer->pts;
        if (subtitlePts != ffmpeg.AV_NOPTS_VALUE)
        {
            frame.Time = subtitlePts * ffmpeg.av_q2d(TimeBase);
        }

        uint startDisplay = frame.SubtitlePointer->start_display_time;
        uint endDisplay = frame.SubtitlePointer->end_display_time;
        frame.Duration = (endDisplay - startDisplay) / 1000.0;

        var enqueueResult = FrameQueue.Enqueue(frame);
        if (enqueueResult == false)
            return DecodeResult.Error;

        return DecodeResult.Success;
    }

    protected override void Dispose(bool disposing)
    {
        base.Dispose(disposing);
        Console.WriteLine("Subtitle decoder exited");
    }
}