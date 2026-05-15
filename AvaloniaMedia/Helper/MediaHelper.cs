

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using AvaloniaMedia.FFmpeg.Model;

namespace AvaloniaMedia.Helper;

public static class MediaHelper
{
    public static readonly string[] VideoExtensions =
        [".mp4", ".mkv", ".mov", ".avi", ".webm", ".flv", ".m4v", ".wmv", ".mts"];
    public static readonly string[] AudioExtensions =
        [".mp3", ".flac", ".aac", ".wav", ".ogg", ".wma", ".m4a", ".opus", ".ape", ".aiff"];
    public static readonly string[] ImageExtensions =
        [".jpg", ".jpeg", ".png", ".bmp", ".gif", ".webp", ".tiff", ".tif", ".ico"];

    public static async IAsyncEnumerable<string> EnumerateMediaFilesAsync(string path, [EnumeratorCancellation] CancellationToken token = default)
    {
        if (!Directory.Exists(path))
            throw new ArgumentException($"Path {path} does not exist");

        var dirs = new Stack<string>();
        dirs.Push(path);
        while (dirs.Count > 0)
        {
            token.ThrowIfCancellationRequested();
            string currentDir = dirs.Pop();
            string[] files;
            try
            {
                foreach (string subDir in Directory.EnumerateDirectories(currentDir))
                    dirs.Push(subDir);
                files = Directory.GetFiles(currentDir);
            }
            catch (Exception ex) when (ex is UnauthorizedAccessException or PathTooLongException)
            {
                continue;
            }

            foreach (string file in files)
            {
                if (IsMedia(file))
                    yield return file;
            }

            await Task.Yield();
        }
    }

    public static Task<List<string>> LoadMediaFileAsync(string path, CancellationToken token = default)
    {
        if (!Directory.Exists(path))
        {
            throw new ArgumentException($"Path {path} does not exist");
        }
        return Task.Run(() =>
        {
            var result = new List<string>();
            var dirs = new Stack<string>();
            dirs.Push(path);
            while (dirs.Count > 0)
            {
                token.ThrowIfCancellationRequested();
                string currentDir = dirs.Pop();
                try
                {
                    // 1. 先把当前目录下的所有子目录压入栈（为下一轮做准备）
                    foreach (string subDir in Directory.EnumerateDirectories(currentDir))
                    {
                        dirs.Push(subDir);
                    }

                    // 2. 再安全地遍历当前目录下的文件
                    foreach (string file in Directory.EnumerateFiles(currentDir))
                    {
                        if (IsMedia(file))
                            result.Add(file);
                    }
                }
                catch (Exception ex) when (ex is UnauthorizedAccessException || ex is PathTooLongException)
                {
                    Console.WriteLine($"无权限路径: {currentDir}");
                }
            }
            return result;
        }, token);
    }

    public static bool IsMedia(string filename)
    {
        var ext = Path.GetExtension(filename).ToLowerInvariant();
        return VideoExtensions.Contains(ext) || AudioExtensions.Contains(ext)
            || ImageExtensions.Contains(ext);
    }

    public static bool IsImage(string filename)
    {
        var ext = Path.GetExtension(filename).ToLowerInvariant();
        return ImageExtensions.Contains(ext);
    }

    public static bool IsVideo(string filename)
    {
        var ext = Path.GetExtension(filename).ToLowerInvariant();
        return VideoExtensions.Contains(ext);
    }

    public static bool IsAudio(string filename)
    {
        var ext = Path.GetExtension(filename).ToLowerInvariant();
        return AudioExtensions.Contains(ext);
    }

    public static unsafe WriteableBitmap LoadImage(byte* data, int width, int height, int stride)
    {
        var bitmap = new WriteableBitmap(new PixelSize(width, height), new Vector(96, 96), PixelFormat.Rgba8888, AlphaFormat.Opaque);
        using var buffer = bitmap.Lock();

        if (buffer.RowBytes != stride)
        {
            for (int i = 0; i < height; i++)
            {
                var srcAddr = new IntPtr(data + i * stride);
                var dstAddr = new IntPtr(buffer.Address.ToInt64() + i * buffer.RowBytes);
                Buffer.MemoryCopy(srcAddr.ToPointer(), dstAddr.ToPointer(), buffer.RowBytes, buffer.RowBytes);
            }
        }
        else
        {
            long length = stride * height;
            Buffer.MemoryCopy(data, buffer.Address.ToPointer(), length, length);
        }
        return bitmap;
    }

    public static Task<Bitmap> LoadImageAsync(string filePath, double width = 0, double height = 0)
    {
        return Task.Run(() =>
        {
            // 从本地文件流加载原始位图
            using var fileStream = File.OpenRead(filePath);
            var originalBitmap = new Bitmap(fileStream);

            if (width == 0 && height == 0)
            {
                return originalBitmap;
            }
            if (width == 0 && height != 0)
            {
                // 固定高度等比缩放
                var targetWidth = originalBitmap.Size.Width / originalBitmap.Size.Height * height;
                var scaledBitmap = originalBitmap.CreateScaledBitmap(new PixelSize((int)targetWidth, (int)height), BitmapInterpolationMode.HighQuality);
                return scaledBitmap;
            }
            else if (width != 0 && height == 0)
            {
                // 固定宽度等比缩放
                var targetHeight = originalBitmap.Size.Height / originalBitmap.Size.Width * width;
                var scaledBitmap = originalBitmap.CreateScaledBitmap(new PixelSize((int)width, (int)targetHeight), BitmapInterpolationMode.HighQuality);
                return scaledBitmap;
            }
            else
            {
                var scaledBitmap = originalBitmap.CreateScaledBitmap(new PixelSize((int)width, (int)height), BitmapInterpolationMode.HighQuality);
                return scaledBitmap;
            }
        });

    }
}