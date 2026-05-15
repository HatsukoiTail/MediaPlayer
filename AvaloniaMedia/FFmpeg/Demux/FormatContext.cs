using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using AvaloniaMedia.FFmpeg.Model;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Demux;

public sealed unsafe class FormatContext : IDisposable
{
    #region AVFormatContext
    public AVFormatContext* FormatContextPointer { get; private set; }
    #endregion

    #region FFmpeg回调的函数委托
    private GCHandle? ioContextHandle;
    private avio_alloc_context_read_packet? readDelegate;
    private avio_alloc_context_seek? seekDelegate;
    private bool ownsAVIO;
    #endregion

    #region Public Methods
    public bool IsOpen => FormatContextPointer != null;
    public double Duration => (double)FormatContextPointer->duration / ffmpeg.AV_TIME_BASE;
    public int StreamCount => (int)FormatContextPointer->nb_streams;
    public AVStream* GetStream(int index) => FormatContextPointer->streams[index];
    public string? Title => GetTitle();
    #endregion

    public string? GetTitle()
    {
        Debug.Assert(IsOpen);

        AVDictionaryEntry* tag = ffmpeg.av_dict_get(FormatContextPointer->metadata, "title", null, 0);
        if (tag == null)
            return null;

        return Marshal.PtrToStringAnsi((IntPtr)tag->value);
    }

    /// <summary>
    /// 打开一个文件作为输入
    /// </summary>
    /// <param name="filename"></param>
    /// <returns></returns>
    /// <exception cref="FFmpegException"></exception>
    public void OpenAsReader(string filename, Func<bool>? interruptCB = null)
    {
        Debug.Assert(IsOpen == false);

        AVFormatContext* formatCtx = ffmpeg.avformat_alloc_context();
        if (formatCtx == null)
        {
            throw new FFmpegException("Failed to allocate format context");
        }

        if (interruptCB != null)
        {
            formatCtx->interrupt_callback = InterruptCallback.Create(interruptCB);
        }

        int result = ffmpeg.avformat_open_input(&formatCtx, filename, null, null);
        if (result < 0)
        {
            ffmpeg.avformat_free_context(formatCtx);
            throw new FFmpegException(result, $"Failed to open input file {filename}");
        }

        result = ffmpeg.avformat_find_stream_info(formatCtx, null);
        if (result < 0)
        {
            ffmpeg.avformat_close_input(&formatCtx);
            throw new FFmpegException(result, $"Failed to find stream info");
        }

        FormatContextPointer = formatCtx;
    }

    public void OpenAsReader(Stream stream)
    {
        Debug.Assert(IsOpen == false);

        // 创建格式上下文
        AVFormatContext* formatCtx = ffmpeg.avformat_alloc_context();
        if (formatCtx == null)
        {
            throw new FFmpegException("Failed to allocate format context");
        }

        // 创建buffer
        const int ioBufferSize = 512 * 1024;
        byte* buffer = (byte*)ffmpeg.av_malloc(ioBufferSize);
        if (buffer == null)
        {
            ffmpeg.avformat_free_context(formatCtx);
            throw new FFmpegException("Failed to allocate AVIO buffer");
        }

        // 创建自定义IO并绑定句柄
        var ioContext = new IOContext(stream);
        var handle = GCHandle.Alloc(ioContext, GCHandleType.Normal);

        if (handle.IsAllocated == false)
        {
            ffmpeg.av_free(buffer);
            ffmpeg.avformat_free_context(formatCtx);
            throw new Exception("Failed to allocate GCHandle for IOContext");
        }

        void* opaque = GCHandle.ToIntPtr(handle).ToPointer();
        var readDelegate = new avio_alloc_context_read_packet(IOContext.FFmpegRead);
        var seekDelegate = new avio_alloc_context_seek(IOContext.FFmpegSeek);

        // 创建AVIO上下文
        AVIOContext* ioCtx = ffmpeg.avio_alloc_context(buffer, ioBufferSize, 0, opaque, readDelegate, null, seekDelegate);
        if (ioCtx == null)
        {
            handle.Free();
            ffmpeg.av_free(buffer);
            ffmpeg.avformat_free_context(formatCtx);
            throw new FFmpegException("Failed to allocate AVIO context");
        }

        // 绑定avio到formatCtx
        formatCtx->pb = ioCtx;

        int result = ffmpeg.avformat_open_input(&formatCtx, null, null, null);
        if (result < 0)
        {
            ffmpeg.avformat_free_context(formatCtx);
            ffmpeg.avio_context_free(&ioCtx);
            handle.Free();
            throw new FFmpegException(result, "Failed to open input file");
        }

        result = ffmpeg.avformat_find_stream_info(formatCtx, null);
        if (result < 0)
        {
            ffmpeg.avformat_close_input(&formatCtx);
            ffmpeg.avio_context_free(&ioCtx);
            handle.Free();
            throw new FFmpegException(result, "Failed to find stream info");
        }

        FormatContextPointer = formatCtx;
        ioContextHandle = handle;
        this.readDelegate = readDelegate;
        this.seekDelegate = seekDelegate;
        ownsAVIO = true;
    }

    public void OpenAsWritter(string fileName, string formatName)
    {
        AVFormatContext* formatCtx = null;
        int result = ffmpeg.avformat_alloc_output_context2(&formatCtx, null, formatName, fileName);
        if (result < 0)
        {
            throw new FFmpegException(result, "Fail to alloc output format context.");
        }
    }

    ~FormatContext() => Dispose();

    public void Close()
    {
        if (FormatContextPointer != null)
        {
            var ctx = FormatContextPointer;
            var interrupt_callback = ctx->interrupt_callback;
            var pb = ctx->pb;
            ffmpeg.avformat_close_input(&ctx);
            FormatContextPointer = null;
            if (ownsAVIO)
                ffmpeg.avio_context_free(&pb);
            InterruptCallback.Release(ref interrupt_callback);
        }
        if (ioContextHandle != null)
        {
            if (ioContextHandle.Value.IsAllocated)
            {
                ioContextHandle.Value.Free();
            }
            ioContextHandle = null;
        }

        readDelegate = null;
        seekDelegate = null;
    }

    private bool disposed = false;
    public void Dispose()
    {
        if (disposed == true)
            return;

        Close();

        GC.SuppressFinalize(this);

        disposed = true;
    }
}