

using System;
using System.Runtime.InteropServices;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Model;


public class FFmpegException : Exception
{
    public int ErrorCode { get; } = int.MinValue;

    public string FFmpegMessage => GetErrorMessage(ErrorCode);

    public FFmpegException(string message)
        : base(message)
    { }

    public FFmpegException(int errorCode, string? message = null)
        : base(message ?? GetErrorMessage(errorCode))
    {
        ErrorCode = errorCode;
    }

    public static void ThrowIfError(int errorCode, string? message = null)
    {
        if (errorCode < 0)
            throw new FFmpegException(errorCode, message);
    }

    public static void ThrowIfNull(IntPtr pointer, string? message = null)
    {
        if (pointer == IntPtr.Zero)
            throw new FFmpegException(message ?? "Pointer is null");
    }

    public static unsafe string GetErrorMessage(int ErrorCode)
    {
        if (ErrorCode >= 0)
            return "Success";
        const int bufferSize = ffmpeg.AV_ERROR_MAX_STRING_SIZE;
        byte* buffer = stackalloc byte[bufferSize];
        ffmpeg.av_strerror(ErrorCode, buffer, bufferSize);
        return Marshal.PtrToStringAnsi((IntPtr)buffer) ?? "Unknown error";
    }
}