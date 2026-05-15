


using System;
using System.Diagnostics;
using System.Threading;
using AvaloniaMedia.FFmpeg.Clock;
using AvaloniaMedia.FFmpeg.Filter;
using AvaloniaMedia.FFmpeg.Model;
using AvaloniaMedia.FFmpeg.Queue;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Decode;

public unsafe class VideoDecoder : Decoder
{

    protected enum DecodeResult { Success, Error, Eof, More }

    public override FrameQueue FrameQueue { get; } = new();
    public override bool IsEof { get; protected set; } = false;
    public override int FinishSerial { get; protected set; } = -1;
    public bool IsStep { get; set; } = false;
    public double Speed { get; set; } = 1.0;

    private readonly VideoFilter videoFilter = new();
    public required ClockManager Clock { get; set; }

    private int lastPacketSerial = -1;
    // 调用av_buffersink_get_frame_flags从滤镜拉取一帧所需要的时间
    // 可以这么理解，该值为一个乐观值，
    // 即使不考虑需要重新配置滤镜以及向滤镜推送一帧以及滤镜中数据不足需要更多帧的情况
    // 即该值为可参考的最快应用滤镜获取一帧所需的时间
    // 如果视频提前于主时钟的时间间隔小于该值，那么最乐观的情况，这一帧也将超时而无法即使播放，从而需要丢弃
    private double lastHandleInterval = 0;

    protected override void Decode(CancellationToken token)
    {
        using var frame = Frame.CreateFrame();

        while (token.IsCancellationRequested == false)
        {
            var result = DecodeFrame(frame, token);

            // Console.WriteLine($"Decode Frame, result = {result}");

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

            // Console.WriteLine($"Handle frame, result = {handleResult}");

            if (handleResult == false)
            {
                break;
            }
        }

        Console.WriteLine("Exit video decode loop.");
    }

    private DecodeResult DecodeFrame(Frame frame, CancellationToken token)
    {
        // Decode Frame流程
        // 1. 检查上次读取包的序列号与当前的主序列号，如果不同，则说明PacketQueue发生了Flush，因此要刷新解码器
        // 2. 调用avcodec_receive_frame接收一帧，如果成功，则返回
        //    如果返回AVERROR(EAGAIN)，则说明没有可用的帧，需要向解码器发送Packet，进入步骤3
        //    如果返回AVERROR_EOF，说明解码完成，刷新解码器以取出剩余的帧，当前先返回
        //    (如果解码器已经刷新过，下次调用avcodec_receive_frame时，会返回AVERROR(EAGAIN)，需要重新发送Packet)
        // 3. 检查PacketQueue是否为空，如果为空，则通知Demuxer继续工作
        // 4. 从PacketQueue中取出一个Packet，发送给解码器

        Debug.Assert(PacketQueue != null);
        while (token.IsCancellationRequested == false)
        {
            // 进入循环时，如果发现上次Packet的Serial与当前的主Serial不同，那么不能先接收帧，而是首先刷新解码器
            if (lastPacketSerial != PacketQueue.Serial && lastPacketSerial >= 0)
            {
                // 刷新解码器
                ffmpeg.avcodec_flush_buffers(CodecContextPointer);
                FinishSerial = -1;
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
                Console.WriteLine($"Decode finish, serial = {lastPacketSerial}");
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
                    // 返回Error，告知调用方停止
                    return DecodeResult.Error;
                }

                if (lastPacketSerial != packet.Serial)
                {
                    // 包的序列号与上次送入解码器的包的序列号不同
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
                    packet.Dispose();
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


    /// <summary>
    /// 接管一帧视频帧，对其进行处理一帧视频帧，包括丢帧策略、滤镜处理等; 处理完成后，将结果存入<see cref="FrameQueue"/>；调用该函数后，Frame会被释放
    /// </summary>
    /// <returns>
    /// 
    /// </returns>
    private bool HandleFrame(Frame frame, CancellationToken token)
    {
        frame.FramePointer->pts = frame.FramePointer->best_effort_timestamp;

        double displayTime = double.NaN;
        if (frame.FramePointer->pts != ffmpeg.AV_NOPTS_VALUE)
            displayTime = frame.FramePointer->pts * ffmpeg.av_q2d(TimeBase);

        Debug.Assert(FormatContext != null && Stream != null && PacketQueue != null);

        frame.FramePointer->sample_aspect_ratio = ffmpeg.av_guess_sample_aspect_ratio(FormatContext.FormatContextPointer, Stream, frame.FramePointer);

        // 默认采用丢帧策略
        if (!IsStep && Clock.SyncMode != ClockType.Video && frame.FramePointer->pts != ffmpeg.AV_NOPTS_VALUE)
        {
            var masterClock = Clock.GetMasterClock();
            var diff = displayTime - masterClock;

            // 10秒，启动时钟同步的阈值，如果超过该阈值，说明处于不稳定状态，同步还未建立，此时不进行丢帧操作
            const double SYNC_THRESHOLD = 10.0;
            if (!double.IsNaN(diff) &&                      // 主时钟有效；主时钟无效意味着刚开始，此时不丢帧
                Math.Abs(diff) < SYNC_THRESHOLD &&          // 处于稳定同步状态；不稳定状态可能是发生了Seek后，此时音视频不同步，不丢帧
                PacketQueue.Count > 0 &&                    // 队列中还有未处理的包，说明处于播放状态; 如果队列为空，则可能接近视频结尾或解复用速度慢，此时帧较为宝贵，不丢帧
                lastPacketSerial == Clock.Serial &&         // 序列号相同; 如果序列号不同，则可能刚发生Seek，此时处于不稳定状态，不丢帧
                (diff - lastHandleInterval) < 0)            // 视频帧超前的时间-Handle一帧(实际上是滤镜图的耗时)的时间，如果小于0，则推断该帧将落后于主时钟，因此丢弃
            {
                Console.WriteLine($"Drop video frame in decoder, pts = {frame.Pts}, frame serial = {lastPacketSerial}, diff = {diff}, last handle time = {lastHandleInterval}");
                frame.Unref();
                return true;
            }
        }

        if (IsHardwareFrame(frame.FramePointer))
        {
            AVFrame* swFrame = ffmpeg.av_frame_alloc();
            if (swFrame == null)
                throw new FFmpegException("Failed to allocate frame for hardware transfer.");

            TransferHardwareFrame(swFrame, frame.FramePointer);

            ffmpeg.av_frame_unref(frame.FramePointer);
            ffmpeg.av_frame_move_ref(frame.FramePointer, swFrame);
            ffmpeg.av_frame_free(&swFrame);
        }

        return FilterFrame(frame, token);
    }


    #region 视频滤镜相关

    private int lastFilterSerial = -1;
    private AVRational frameRate = new() { num = 0, den = 0 };


    /// <summary>
    /// 接管一个<see cref="Frame"/>, 对其进行滤镜处理, 并将结果存入<see cref="FrameQueue"/>. 调用该函数后, Frame会被释放
    /// </summary>
    /// <returns>
    /// <see cref="true"/>: 成功处理一帧; 
    /// <see cref="false"/>: 发生错误，token被取消，或队列已停止; 
    /// </returns>
    /// <exception cref="FFmpegException"></exception>
    private bool FilterFrame(Frame frame, CancellationToken token)
    {
        // 检测是否需要重新配置滤镜
        if (videoFilter.Width != frame.FramePointer->width ||
            videoFilter.Height != frame.FramePointer->height ||
            videoFilter.PixelFormat != (AVPixelFormat)frame.FramePointer->format ||
            lastFilterSerial != lastPacketSerial)
        {
            var guessFrameRate = ffmpeg.av_guess_frame_rate(FormatContext!.FormatContextPointer, Stream, null);
            var options = new VideoFilter.Options
            {
                FilterThreadCount = 4,
                Width = frame.Width,
                Height = frame.Height,
                PixelFormat = (AVPixelFormat)frame.Format,
                TimeBase = Stream->time_base,
                AspectRatio = Stream->codecpar->sample_aspect_ratio,
                ColorSpace = frame.FramePointer->colorspace,
                ColorRange = frame.FramePointer->color_range,
                FrameRate = guessFrameRate,
                HardwareContext = frame.FramePointer->hw_frames_ctx,
                PixelFormatAllowList = [AVPixelFormat.AV_PIX_FMT_RGBA],
            };
            videoFilter.Build(options);

            lastFilterSerial = lastPacketSerial;
            frameRate = ffmpeg.av_buffersink_get_frame_rate(videoFilter.OutputContextPointer);
        }

        int result = ffmpeg.av_buffersrc_add_frame(videoFilter.InputContextPointer, frame.FramePointer);
        if (result < 0)
        {
            throw new FFmpegException(result, "Failed to add frame to filter graph.");
        }
        while (token.IsCancellationRequested == false)
        {
            // 首先尝试获取帧，注意此处直接使用frame作为接收新帧的结构体，因为av_buffersink_get_frame_flags会检查frame->data，并减少其引用计数
            var relativeTimeStart = (double)Stopwatch.GetTimestamp() / Stopwatch.Frequency;
            result = ffmpeg.av_buffersink_get_frame_flags(videoFilter.OutputContextPointer, frame.FramePointer, 0);
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
                throw new FFmpegException(result, "Failed to get frame from filter graph.");
            }
            else
            {
                var relativeTimeEnd = (double)Stopwatch.GetTimestamp() / Stopwatch.Frequency;
                lastHandleInterval = relativeTimeEnd - relativeTimeStart;
                if (Math.Abs(lastHandleInterval) > 1.0)
                    lastHandleInterval = 0;

                // 成功获取帧
                // 存入队列

                var timeBase = ffmpeg.av_buffersink_get_time_base(videoFilter.OutputContextPointer);
                var duration = (frameRate.den != 0) ? ffmpeg.av_q2d(new AVRational { num = frameRate.den, den = frameRate.num }) : 0;
                var frameTime = (frame.FramePointer->pts != ffmpeg.AV_NOPTS_VALUE) ? frame.FramePointer->pts * ffmpeg.av_q2d(timeBase) : -1;

                frame.Uploaded = false;
                frame.Time = frameTime;
                frame.Duration = duration;
                frame.Serial = lastPacketSerial;

                Debug.Assert(frame.IsNull == false && frame.FramePointer->data.Length != 0);

                // Console.WriteLine($"Enqueue a frame, pts = {frame.Time}, queue size = {FrameQueue.Count}");

                var enqueueResult = FrameQueue.Enqueue(frame);

                if (enqueueResult == false)
                {
                    // 队列已被停止，意味着需要停止解码
                    return false;
                }
            }
        }

        return false;
    }
    #endregion

    private static bool IsHardwareFrame(AVFrame* frame)
    {
        if (frame->hw_frames_ctx != null)
            return true;

        var desc = ffmpeg.av_pix_fmt_desc_get((AVPixelFormat)frame->format);
        if (desc != null && (desc->flags & ffmpeg.AV_PIX_FMT_FLAG_HWACCEL) != 0)
            return true;

        return false;
    }

    private static void TransferHardwareFrame(AVFrame* dst, AVFrame* src)
    {
        int ret = ffmpeg.av_hwframe_transfer_data(dst, src, 0);
        if (ret < 0)
            throw new FFmpegException(ret, "Failed to transfer hardware frame to software frame.");

        dst->pts = src->pts;
        dst->best_effort_timestamp = src->best_effort_timestamp;
        dst->sample_aspect_ratio = src->sample_aspect_ratio;
        dst->colorspace = src->colorspace;
        dst->color_range = src->color_range;
    }

    protected override void Dispose(bool disposing)
    {
        videoFilter.Dispose();
        FrameQueue.Dispose();

        base.Dispose(disposing);

        Console.WriteLine("Video decoder exited");
    }
}