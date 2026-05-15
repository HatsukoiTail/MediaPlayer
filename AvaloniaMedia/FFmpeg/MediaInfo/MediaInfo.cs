


using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using Avalonia.Media.Imaging;
using AvaloniaMedia.FFmpeg.Demux;
using FFmpeg.AutoGen;
using HarfBuzzSharp;

namespace AvaloniaMedia.FFmpeg.MediaInfo;

public unsafe class MediaInfo : IDisposable
{
    private FormatContext FormatContext { get; } = new();

    public string? FilePath { get; private set; }
    public string? FileName => Path.GetFileName(FilePath);
    public Stream? FileStream { get; private set; }

    #region AVFormatContext
    public double Duration => FormatContext.IsOpen ? FormatContext.Duration : 0;
    public long BitRate => FormatContext.IsOpen ? FormatContext.FormatContextPointer->bit_rate : 0;
    public long Size => GetMediaSize();
    public string FormatName => GetFormatName();
    public int StreamCount => (int)(FormatContext.IsOpen ? FormatContext.FormatContextPointer->nb_streams : 0);
    #endregion

    #region Video Stream
    private AVStream* videoStream = null;
    public string VideoCodec => GetVideoCodecName();
    public int Width => GetWidth();
    public int Height => GetHeight();
    public string PixelFormat => GetPixelFormat();
    public double FrameRate => GetFrameRate();
    public double VideoStartTime => GetVideoStartTime();
    public double VideoDuration => GetVideoDuration();
    public long VideoFrameCount => GetVideoFrameCount();
    public long VideoBitRate => GetVideoBitRate();
    public WriteableBitmap? Cover
    {
        get
        {
            if (field != null)
            {
                return field;
            }
            return GetCover();
        }
    }
    #endregion

    #region Audio Stream
    private AVStream* audioStream = null;
    public string AudioCodec => GetAudioCodecName();
    public int SampleRate => GetSampleRate();
    public int ChannelCount => GetChannelCount();
    public string ChannelLayout => GetChannelLayout();
    public string SampleFormat => GetSampleFormat();
    public int BitsPerSample => GetBitsPerSample();
    public long AudioBitRate => GetAudioBitRate();
    #endregion

    #region Meta Data
    public Dictionary<string, string>? MetaData
    {
        get
        {
            if (field != null)
            {
                return field;
            }
            return GetMetaData();
        }
    }
    public Chapter[]? Chapters
    {
        get
        {
            if (field != null)
            {
                return field;
            }
            return GetChapter();
        }
    }
    #endregion

    public MediaInfo(string path)
    {
        FormatContext.OpenAsReader(path);
        FilePath = path;
    }

    private long GetMediaSize()
    {
        if (FormatContext.IsOpen == false)
            return 0;

        var size = ffmpeg.avio_size(FormatContext.FormatContextPointer->pb);
        if (size > 0)
            return size;

        if (FilePath != null)
        {
            return new FileInfo(FilePath).Length;
        }
        else if (FileStream != null)
        {
            return FileStream.Length;
        }
        else
        {
            throw new Exception("FilePath and FileStream are alse null");
        }
    }

    private string GetFormatName()
    {
        if (FormatContext.IsOpen == false)
            return string.Empty;

        byte* ptr = FormatContext.FormatContextPointer->iformat->name;
        var name = Marshal.PtrToStringAnsi((IntPtr)ptr);
        return string.IsNullOrEmpty(name) ? string.Empty : name;
    }

    private AVStream* GetStream(AVMediaType type)
    {
        if (FormatContext.IsOpen == false)
            return null;

        var index = ffmpeg.av_find_best_stream(FormatContext.FormatContextPointer, type, -1, -1, null, 0);
        if (index < 0)
        {
            return null;
        }
        return FormatContext.FormatContextPointer->streams[index];
    }

    private string GetVideoCodecName()
    {
        if (videoStream == null)
        {
            videoStream = GetStream(AVMediaType.AVMEDIA_TYPE_VIDEO);
        }

        if (videoStream == null)
        {
            return string.Empty;
        }

        return ffmpeg.avcodec_get_name(videoStream->codecpar->codec_id);
    }

    private int GetWidth()
    {
        if (videoStream == null)
        {
            videoStream = GetStream(AVMediaType.AVMEDIA_TYPE_VIDEO);
        }

        if (videoStream == null)
        {
            return 0;
        }

        return videoStream->codecpar->width;
    }

    private int GetHeight()
    {
        if (videoStream == null)
        {
            videoStream = GetStream(AVMediaType.AVMEDIA_TYPE_VIDEO);
        }

        if (videoStream == null)
        {
            return 0;
        }

        return videoStream->codecpar->height;
    }

    private string GetPixelFormat()
    {
        if (videoStream == null)
        {
            videoStream = GetStream(AVMediaType.AVMEDIA_TYPE_VIDEO);
        }
        if (videoStream == null)
        {
            return string.Empty;
        }
        var format = videoStream->codecpar->format;
        return ffmpeg.av_get_pix_fmt_name((AVPixelFormat)format);
    }

    private double GetFrameRate()
    {
        if (videoStream == null)
        {
            videoStream = GetStream(AVMediaType.AVMEDIA_TYPE_VIDEO);
        }
        if (videoStream == null)
        {
            return 0;
        }
        var rate = (double)videoStream->codecpar->framerate.num / videoStream->codecpar->framerate.den;
        return rate;
    }

    private double GetVideoStartTime()
    {
        if (videoStream == null)
        {
            videoStream = GetStream(AVMediaType.AVMEDIA_TYPE_VIDEO);
        }
        if (videoStream == null)
        {
            return 0;
        }
        if (videoStream->start_time == ffmpeg.AV_NOPTS_VALUE)
        {
            return 0;
        }
        var startTime = (double)videoStream->start_time / ffmpeg.AV_TIME_BASE;
        return startTime;
    }

    private double GetVideoDuration()
    {
        if (videoStream == null)
        {
            videoStream = GetStream(AVMediaType.AVMEDIA_TYPE_VIDEO);
        }
        if (videoStream == null)
        {
            return 0;
        }
        return (double)videoStream->duration * videoStream->time_base.num / videoStream->time_base.den;
    }

    private long GetVideoFrameCount()
    {
        if (videoStream == null)
        {
            videoStream = GetStream(AVMediaType.AVMEDIA_TYPE_VIDEO);
        }
        if (videoStream == null)
        {
            return 0;
        }
        return videoStream->nb_frames;
    }

    private long GetVideoBitRate()
    {
        if (videoStream == null)
        {
            videoStream = GetStream(AVMediaType.AVMEDIA_TYPE_VIDEO);
        }
        if (videoStream == null)
        {
            return 0;
        }
        return videoStream->codecpar->bit_rate;
    }

    private WriteableBitmap? GetCover()
    {
        if (videoStream == null)
        {
            videoStream = GetStream(AVMediaType.AVMEDIA_TYPE_VIDEO);
        }
        if (videoStream == null)
        {
            return null;
        }
        if ((videoStream->disposition & ffmpeg.AV_DISPOSITION_ATTACHED_PIC) != 1)
        {
            return null;
        }
        var packet = &videoStream->attached_pic;

        AVCodecContext* ctx = null;
        AVFrame* frame = null;
        AVFrame* swsFrame = null;

        try
        {
            // 打开解码器
            var codec = ffmpeg.avcodec_find_decoder(videoStream->codecpar->codec_id);
            if (codec == null)
                return null;

            ctx = ffmpeg.avcodec_alloc_context3(codec);
            if (ctx == null)
                return null;

            if (ffmpeg.avcodec_open2(ctx, codec, null) < 0)
            {
                return null;
            }

            if (ffmpeg.avcodec_send_packet(ctx, packet) < 0)
            {
                return null;
            }

            frame = ffmpeg.av_frame_alloc();
            if (frame == null)
                return null;

            if (ffmpeg.avcodec_receive_frame(ctx, frame) < 0)
            {
                return null;
            }

            if ((AVPixelFormat)frame->format != AVPixelFormat.AV_PIX_FMT_RGBA && (AVPixelFormat)frame->format != AVPixelFormat.AV_PIX_FMT_BGRA)
            {
                swsFrame = ffmpeg.av_frame_alloc();
                swsFrame->width = frame->width;
                swsFrame->height = frame->height;
                swsFrame->format = (int)AVPixelFormat.AV_PIX_FMT_RGBA;
                if (swsFrame == null)
                {
                    return null;
                }
                SwsContext* sws_ctx = ffmpeg.sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format,
                                                            swsFrame->width, swsFrame->height, (AVPixelFormat)swsFrame->format,
                                                            (int)SwsFlags.SWS_BILINEAR, null, null, null);
                if (sws_ctx == null)
                {
                    return null;
                }
                int result = ffmpeg.sws_scale_frame(sws_ctx, swsFrame, frame);
                if (result < 0)
                {
                    return null;
                }
            }
            else
            {
                swsFrame = frame;
                frame = null;
            }

            var width = swsFrame->width;
            var height = swsFrame->height;
            var data = swsFrame->data[0];
            var stride = swsFrame->linesize[0];

            var bitmap = new WriteableBitmap(
                new Avalonia.PixelSize(swsFrame->width, swsFrame->height),
                new Avalonia.Vector(96, 96),
                Avalonia.Platform.PixelFormat.Rgba8888,
                Avalonia.Platform.AlphaFormat.Opaque
            );

            using var buffer = bitmap.Lock();

            if (buffer.RowBytes != stride)
            {
                for (int i = 0; i < height; i++)
                {
                    var srcAddr = new IntPtr(data + i * stride);
                    var dstAddr = new IntPtr(buffer.Address.ToInt64() + i * buffer.RowBytes);
                    System.Buffer.MemoryCopy(srcAddr.ToPointer(), dstAddr.ToPointer(), stride, stride);
                }
            }
            else
            {
                var length = width * height * 4;
                System.Buffer.MemoryCopy(data, buffer.Address.ToPointer(), length, length);
            }

            return bitmap;
        }
        finally
        {
            ffmpeg.avcodec_free_context(&ctx);
            ffmpeg.av_frame_free(&frame);
            ffmpeg.av_frame_free(&swsFrame);
        }
    }

    private string GetAudioCodecName()
    {
        if (audioStream == null)
        {
            audioStream = GetStream(AVMediaType.AVMEDIA_TYPE_AUDIO);
        }

        if (audioStream == null)
        {
            return string.Empty;
        }

        return ffmpeg.avcodec_get_name(audioStream->codecpar->codec_id);
    }

    private int GetSampleRate()
    {
        if (audioStream == null)
        {
            audioStream = GetStream(AVMediaType.AVMEDIA_TYPE_AUDIO);
        }

        if (audioStream == null)
        {
            return 0;
        }

        return audioStream->codecpar->sample_rate;
    }

    private int GetChannelCount()
    {
        if (audioStream == null)
        {
            audioStream = GetStream(AVMediaType.AVMEDIA_TYPE_AUDIO);
        }

        if (audioStream == null)
        {
            return 0;
        }

        return audioStream->codecpar->ch_layout.nb_channels;
    }

    private string GetChannelLayout()
    {
        if (audioStream == null)
        {
            audioStream = GetStream(AVMediaType.AVMEDIA_TYPE_AUDIO);
        }

        if (audioStream == null)
        {
            return string.Empty;
        }
        byte* description = stackalloc byte[64];
        int result = ffmpeg.av_channel_layout_describe(&audioStream->codecpar->ch_layout, description, 64);
        if (result < 0)
        {
            return string.Empty;
        }
        return Marshal.PtrToStringAnsi((IntPtr)description)!;
    }

    private string GetSampleFormat()
    {
        if (audioStream == null)
        {
            audioStream = GetStream(AVMediaType.AVMEDIA_TYPE_AUDIO);
        }

        if (audioStream == null)
        {
            return string.Empty;
        }

        return ffmpeg.av_get_sample_fmt_name((AVSampleFormat)audioStream->codecpar->format);
    }

    private int GetBitsPerSample()
    {
        if (audioStream == null)
        {
            audioStream = GetStream(AVMediaType.AVMEDIA_TYPE_AUDIO);
        }

        if (audioStream == null)
        {
            return 0;
        }

        return ffmpeg.av_get_bytes_per_sample((AVSampleFormat)audioStream->codecpar->format);
    }

    private long GetAudioBitRate()
    {
        if (audioStream == null)
        {
            audioStream = GetStream(AVMediaType.AVMEDIA_TYPE_AUDIO);
        }
        if (audioStream == null)
        {
            return 0;
        }
        return audioStream->codecpar->bit_rate;
    }

    public Dictionary<string, string> GetMetaData()
    {
        if (FormatContext.IsOpen == false)
        {
            return [];
        }
        var metadata = FormatContext.FormatContextPointer->metadata;
        if (metadata == null)
        {
            return [];
        }
        var result = new Dictionary<string, string>();
        AVDictionaryEntry* tag = null;
        while ((tag = ffmpeg.av_dict_get(metadata, "", tag, ffmpeg.AV_DICT_IGNORE_SUFFIX)) != null)
        {
            var key = Marshal.PtrToStringAnsi((IntPtr)tag->key);
            if (key == null)
                continue;
            var value = Marshal.PtrToStringAnsi((IntPtr)tag->value);
            if (value == null)
                continue;

            result.Add(key, value);
        }
        result.TrimExcess();
        return result;
    }

    private string GetMetaData(AVDictionary* dict, string key)
    {
        var tag = ffmpeg.av_dict_get(dict, key, null, 0);
        if (tag == null)
        {   
            return string.Empty;
        }
        var value = Marshal.PtrToStringAnsi((nint)tag->value);
        if (string.IsNullOrEmpty(value))
        {
            return string.Empty;
        }
        return value;
    }

    private Chapter[] GetChapter()
    {
        if (FormatContext.IsOpen == false)
            return [];

        AVChapter** chapterPtr = FormatContext.FormatContextPointer->chapters;
        var chapterCount = FormatContext.FormatContextPointer->nb_chapters;

        if (chapterPtr == null || chapterCount == 0)
            return [];

        var chapters = new Chapter[chapterCount];

        for (uint i = 0; i < chapterCount; ++i)
        {
            var chapter = chapterPtr[i];
            if (chapter == null)
            {
                continue;
            }
            var title = GetMetaData(chapter->metadata, "title");
            var startTime = (double)chapter->start * chapter->time_base.num / chapter->time_base.den;
            var endTime = (double)chapter->end * chapter->time_base.num / chapter->time_base.den;
            var record = new Chapter
            {
                Id = chapter->id,
                Title = title,
                StartTime = startTime,
                EndTime = endTime
            };
        }

        return chapters;
    }

    public void Dispose()
    {

    }
}