

using System;
using AvaloniaMedia.FFmpeg.Model;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Decode;

public unsafe class CodecContext : IDisposable
{
    public AVCodecContext* CodecContextPointer { get; private set; }
    public bool IsOpen => CodecContextPointer != null;

    public void Open(AVStream* stream)
    {
        AVCodecContext* codecContext = null;
        try
        {
            AVCodec* decoder = ffmpeg.avcodec_find_decoder(stream->codecpar->codec_id);
            if (decoder == null)
            {
                throw new FFmpegException($"Failed to find decoder for codec {stream->codecpar->codec_id}.");
            }

            codecContext = ffmpeg.avcodec_alloc_context3(decoder);
            if (codecContext == null)
            {
                throw new FFmpegException("Failed to allocate codec context.");
            }

            if (ffmpeg.avcodec_parameters_to_context(codecContext, stream->codecpar) < 0)
            {
                throw new FFmpegException("Failed to copy codec parameters to codec context.");
            }

            if (ffmpeg.avcodec_open2(codecContext, decoder, null) < 0)
            {
                throw new FFmpegException("Failed to open codec context.");
            }

            CodecContextPointer = codecContext;
            codecContext = null;
        }
        finally
        {
            if (codecContext != null)
                ffmpeg.avcodec_free_context(&codecContext);
        }
    }

    public void Close()
    {
        if (CodecContextPointer != null)
        {
            var ptr = CodecContextPointer;
            ffmpeg.avcodec_free_context(&ptr);
            CodecContextPointer = null;
        }
    }

    public void Dispose()
    {
        Close();
        GC.SuppressFinalize(this);
    }
}