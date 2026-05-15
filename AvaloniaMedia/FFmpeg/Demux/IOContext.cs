
using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using AvaloniaMedia.FFmpeg.Model;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Demux;

public sealed unsafe class IOContext(Stream stream)
{
    private readonly Stream stream = stream;

    private int Read(byte* buffer, int bufferSize)
    {
        Debug.Assert(stream != null, "Stream should not be null");
        Debug.Assert(stream.CanRead, "Stream should be readable");
        var bytesRead = stream.Read(new Span<byte>(buffer, bufferSize));
        if (bytesRead == 0)
            return ffmpeg.AVERROR_EOF;
        return bytesRead;
    }

    private long Seek(long offset, int whence)
    {
        Debug.Assert(stream != null, "Stream should not be null");
        Debug.Assert(stream.CanSeek, "Stream should be seekable");

        if (whence == ffmpeg.AVSEEK_SIZE)
            return stream.Length;

        const int SEEK_SET = 0;
        const int SEEK_CUR = 1;
        const int SEEK_END = 2;

        SeekOrigin origin = whence switch
        {
            SEEK_SET => SeekOrigin.Begin,
            SEEK_CUR => SeekOrigin.Current,
            SEEK_END => SeekOrigin.End,
            _ => throw new FFmpegException($"Invalid seek whence, {whence}")
        };

        return stream.Seek(offset, origin);
    }

    public static int FFmpegRead(void* opaque, byte* buffer, int bufferSize)
    {
        GCHandle handle = GCHandle.FromIntPtr((IntPtr)opaque);
        var ioContext = (IOContext)handle.Target!;
        return ioContext.Read(buffer, bufferSize);
    }
    public static long FFmpegSeek(void* opaque, long offset, int whence)
    {
        GCHandle handle = GCHandle.FromIntPtr((IntPtr)opaque);
        var ioContext = (IOContext)handle.Target!;
        return ioContext.Seek(offset, whence);
    }
}