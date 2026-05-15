
using System;
using System.Threading;
using AvaloniaMedia.FFmpeg.Model;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Model;

public sealed unsafe class Frame : IDisposable
{
    public AVFrame* FramePointer { get; private set; } = null;
    public AVSubtitle* SubtitlePointer { get; private set; } = null;

    public bool IsNull => FramePointer == null;
    public long Pts => FramePointer->pts;
    public int Format => FramePointer->format;
    public int Width => FramePointer->width;
    public int Height => FramePointer->height;
    public int SampleRate => FramePointer->sample_rate;
    public int SampleNum => FramePointer->nb_samples;
    public int ChannelCount => FramePointer->ch_layout.nb_channels;
    public AVRational SampleAspectRatio => FramePointer->sample_aspect_ratio;
    public int BytePerSample => ffmpeg.av_get_bytes_per_sample((AVSampleFormat)FramePointer->format);
    public byte* DataPointer(uint plane) => FramePointer->data[plane];
    public int LineSize(uint plane) => FramePointer->linesize[plane];

    public double Time { get; set; } = double.NaN; // 以秒为单位
    public double Duration { get; set; } = double.NaN;
    public int Serial { get; set; } = -1;
    public bool Uploaded { get; set; } = false;
    public double Speed { get; set; } = 1.0;
    public string? Subtitle { get; set; }


    /// <summary>
    /// 创建一个初始化的Frame对象，并分配内存；初始状态下，各计算属性无效，不应访问
    /// </summary>
    /// <returns></returns>
    /// <exception cref="FFmpegException">资源创建失败异常</exception>
    public static Frame CreateFrame()
    {
        var frame = ffmpeg.av_frame_alloc();
        if (frame == null)
            throw new FFmpegException("Failed to allocate frame");

        return new Frame { FramePointer = frame };
    }

    public static Frame MoveFrame(Frame other)
    {
        var framePtr = ffmpeg.av_frame_alloc();
        if (framePtr == null)
            throw new FFmpegException("Failed to allocate frame");

        ffmpeg.av_frame_move_ref(framePtr, other.FramePointer);
        var frame = new Frame
        {
            FramePointer = framePtr,
            Time = other.Time,
            Duration = other.Duration,
            Serial = other.Serial,
            Uploaded = other.Uploaded,
            Speed = other.Speed,
            Subtitle = other.Subtitle
        };
        // other.FramePointer = null; // error: 'other' is a 'ref struct' and cannot be used as a 'variable'
        other.Time = double.NaN;
        other.Duration = double.NaN;
        other.Serial = -1;
        other.Uploaded = false;
        other.Speed = 1.0;
        other.Subtitle = null;
        return frame;
    }

    public void Unref()
    {
        ffmpeg.av_frame_unref(FramePointer);
    }

    /// <summary>
    /// 释放Frame中的资源，将其变为空状态，Frame.IsNull为true
    /// 该函数不是线程安全的
    /// </summary>
    public void Release()
    {
        if (SubtitlePointer != null)
        {
            ffmpeg.avsubtitle_free(SubtitlePointer);
            SubtitlePointer = null;
        }

        if (FramePointer != null)
        {
            var ptr = FramePointer;
            ffmpeg.av_frame_free(&ptr);
            FramePointer = null;
        }
    }

    private bool disposed = false;

    /// <summary>
    /// 回收Frame的所有资源，该函数是线程安全的
    /// </summary>
    public void Dispose()
    {
        if (Interlocked.Exchange(ref disposed, true) == true)
            return;

        Release();

        GC.SuppressFinalize(this);
    }

    ~Frame() => Dispose();
}