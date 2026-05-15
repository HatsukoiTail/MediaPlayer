using System;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using AvaloniaMedia.FFmpeg.Demux;
using AvaloniaMedia.FFmpeg.Model;
using AvaloniaMedia.FFmpeg.Queue;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Decode;

public abstract unsafe class Decoder : IDisposable
{
    #region FFmpeg结构体
    public FormatContext? FormatContext { get; private set; }
    public AVStream* Stream { get; private set; }
    public AVCodecContext* CodecContextPointer { get; private set; }
    public AVBufferRef* HardwareDeviceContext { get; protected set; }
    #endregion

    #region 解码线程
    private Thread? decodeThread;
    private CancellationTokenSource? decodeThreadCancelTokenSource;
    #endregion

    #region 解码参数与队列
    protected PacketQueue? PacketQueue { get; private set; }
    public abstract FrameQueue FrameQueue { get; }
    public AVRational TimeBase => Stream->time_base;
    #endregion

    #region 解码器状态
    public bool IsRunning => decodeThread != null && decodeThread.IsAlive;
    public bool IsOpen { get; protected set; } = false;
    public abstract bool IsEof { get; protected set; }
    public abstract int FinishSerial { get; protected set; }
    #endregion

    public virtual Task OpenAsync(FormatContext formatContext, int streamIndex, PacketQueue packetQueue)
    {
        return Task.Run(() => Open(formatContext, formatContext.GetStream(streamIndex), packetQueue));
    }

    public virtual void Open(FormatContext formatContext, AVStream* stream, PacketQueue packetQueue)
    {
        Debug.Assert(IsOpen == false);

        AVCodec* decoder = ffmpeg.avcodec_find_decoder(stream->codecpar->codec_id);
        if (decoder == null)
        {
            throw new FFmpegException($"Failed to find decoder for codec {stream->codecpar->codec_id}.");
        }

        AVCodecContext* codecContext = ffmpeg.avcodec_alloc_context3(decoder);
        if (codecContext == null)
        {
            throw new FFmpegException("Failed to allocate codec context.");
        }

        if (ffmpeg.avcodec_parameters_to_context(codecContext, stream->codecpar) < 0)
        {
            ffmpeg.avcodec_free_context(&codecContext);
            throw new FFmpegException("Failed to copy codec parameters to codec context.");
        }

        if (stream->codecpar->codec_type == AVMediaType.AVMEDIA_TYPE_VIDEO)
        {
            var hwDevice = OpenHardwareDevice(decoder);
            if (hwDevice != null)
            {
                codecContext->hw_device_ctx = ffmpeg.av_buffer_ref(hwDevice);
                HardwareDeviceContext = codecContext->hw_device_ctx;
            }
        }

        if (ffmpeg.avcodec_open2(codecContext, decoder, null) < 0)
        {
            ffmpeg.avcodec_free_context(&codecContext);
            throw new FFmpegException("Failed to open codec context.");
        }

        FormatContext = formatContext;
        Stream = stream;
        CodecContextPointer = codecContext;
        PacketQueue = packetQueue;
        IsEof = false;
        FinishSerial = -1;
        IsOpen = true;
    }

    public Task CloseAsync() => Task.Run(Close);

    public void Close()
    {
        if (!IsOpen)
            return;

        Stop();

        FrameQueue.Clear();

        var ptr = CodecContextPointer;
        ffmpeg.avcodec_free_context(&ptr);
        CodecContextPointer = null;

        if (HardwareDeviceContext != null)
        {
            var hwCtx = HardwareDeviceContext;
            ffmpeg.av_buffer_unref(&hwCtx);
            HardwareDeviceContext = null;
        }

        FormatContext = null;
        Stream = null;
        PacketQueue = null;
        IsEof = false;
        FinishSerial = -1;

        IsOpen = false;
    }

    public void Start()
    {
        Debug.Assert(IsOpen == true);
        Debug.Assert(IsRunning == false);

        FrameQueue.Start();

        decodeThreadCancelTokenSource ??= new CancellationTokenSource();
        decodeThread = new Thread(() => Decode(decodeThreadCancelTokenSource.Token));
        decodeThread.Start();
    }

    public Task StopAsync() => Task.Run(Stop);

    public void Stop()
    {
        decodeThreadCancelTokenSource?.Cancel();
        // 注意，解码线程仍有可能阻塞在PacketQueue.Dequeue或FrameQueue.Enqueue
        PacketQueue?.Abort();
        FrameQueue.Abort();
        decodeThread?.Join();
        decodeThreadCancelTokenSource?.Dispose();
        decodeThreadCancelTokenSource = null;
        decodeThread = null;
    }

    protected abstract void Decode(CancellationToken token);

    private static AVBufferRef* OpenHardwareDevice(AVCodec* codec)
    {
        AVHWDeviceType hardwareDeviceType = AVHWDeviceType.AV_HWDEVICE_TYPE_NONE;

        const int AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX = 1;
        for (int i = 0; ; i++)
        {
            AVCodecHWConfig* config = ffmpeg.avcodec_get_hw_config(codec, i);
            if (config == null)
                break;

            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0)
            {
                hardwareDeviceType = config->device_type;
                break;
            }
        }

        if (hardwareDeviceType == AVHWDeviceType.AV_HWDEVICE_TYPE_NONE)
            return null;

        AVBufferRef* hwDeviceCtx = null;
        int ret = ffmpeg.av_hwdevice_ctx_create(&hwDeviceCtx, hardwareDeviceType, null, null, 0);
        if (ret < 0)
        {
            hardwareDeviceType = AVHWDeviceType.AV_HWDEVICE_TYPE_NONE;
            return null;
        }

        return hwDeviceCtx;
    }

    private bool disposed;
    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }
    protected virtual void Dispose(bool disposing)
    {
        if (disposed)
            return;
        Close();
        FrameQueue.Dispose();
        disposed = true;
    }
    ~Decoder() => Dispose(false);
}
