


// using System;
// using System.Diagnostics;
// using System.IO;
// using System.Threading;
// using AvaloniaMedia.FFmpeg.Demux;
// using AvaloniaMedia.FFmpeg.Model;
// using FFmpeg.AutoGen;

// namespace AvaloniaMedia.FFmpeg.MediaInfo;

// public class ThumbnailLoader : IDisposable
// {
//     private readonly FormatContext formatContext = new();
//     private string? filePath;
//     private Stream? fileStream;

//     public bool IsOpen => formatContext.IsOpen;

//     public void Open(string path)
//     {
//         if (filePath != null && filePath != path)
//         {
//             formatContext.Close();
//         }

//         formatContext.OpenAsReader(path);
//         filePath = path;
//     }

//     public Frame Load(CancellationToken token)
//     {
//         if (formatContext.IsOpen)
//         {
//             throw new InvalidOperationException("File has not been opened.");
//         }

//     }

//     private unsafe void Seek(double seconds)
//     {
//         Debug.Assert(formatContext.IsOpen);
//         seconds = Math.Max(0, Math.Min(formatContext.Duration, seconds));
//         long pts = (long)(seconds * ffmpeg.AV_TIME_BASE);
//         int result = ffmpeg.avformat_seek_file(FormatContext.FormatContextPointer, -1, long.MinValue, pts, long.MaxValue, 0);
//         if (result < 0)
//         {
//             throw new FFmpegException(result, "Failed to seek");
//         }
//     }
    
//     private Frame GetFrame()
//     {
        
//     }

//     public void Dispose()
//     {
        
//     }
// }