using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Media.Imaging;
using AvaloniaMedia.FFmpeg.Model;
using AvaloniaMedia.FFmpeg.Thumbnail;
using AvaloniaMedia.Helper;
using CommunityToolkit.Mvvm.ComponentModel;

namespace AvaloniaMedia.Models;

public partial class MediaModel : ObservableObject
{
    public string FilePath { get; init; } = string.Empty;
    public string FileName => Path.GetFileName(FilePath);
    public string DirectoryPath => Path.GetDirectoryName(FilePath) ?? string.Empty;
    public string Extension => Path.GetExtension(FilePath).ToLowerInvariant();

    public long FileSize { get; init; }
    public string FileSizeText => FormatFileSize(FileSize);

    public double Duration { get; init; }
    public string DurationText => FormatDuration(Duration);

    public int Width { get; init; }
    public int Height { get; init; }
    public string ResolutionText => Width > 0 ? $"{Width} × {Height}" : string.Empty;

    public DateTime LastWriteTime { get; init; }

    [ObservableProperty]
    public partial Bitmap? Thumbnail { get; set; }

    [ObservableProperty]
    public partial bool ThumbnailLoaded { get; set; }

    [ObservableProperty]
    public partial bool HasError { get; set; }

    public bool IsVideo => MediaHelper.IsVideo(FilePath);

    public bool IsAudio => MediaHelper.IsAudio(FilePath);

    public bool IsImage => MediaHelper.IsImage(FilePath);

    public string MediaTypeIcon => IsVideo ? "🎬" : IsAudio ? "🎵" : IsImage ? "🖼" : "📄";

    public string SubtitleText => IsImage ? $"{Width} × {Height}" : DurationText;

    private readonly Lock _thumbnailLock = new();

    public async Task LoadThumbnailAsync(CancellationToken token = default)
    {
        if (ThumbnailLoaded || HasError) return;

        try
        {
            Bitmap? result = null;
            if (MediaHelper.IsVideo(FilePath))
            {
                var option = new FrameLoader.Option()
                {
                    Width = 200,
                    Height = 0,
                    Position = 0.5,
                };
                result = await FrameLoader.LoadFrameAsync(FilePath, option, token);
            }
            else if (MediaHelper.IsImage(FilePath))
            {
                result = await MediaHelper.LoadImageAsync(FilePath, 200, 0);
            }

            if (result is null) return;

            lock (_thumbnailLock)
            {
                if (ThumbnailLoaded || HasError) return;
                Thumbnail = result;
                ThumbnailLoaded = true;
                Console.WriteLine($"Media loaded, {FileName}");
            }
        }
        catch (FFmpegException ex)
        {
            Console.WriteLine($"Fial to load {FileName}, {ex.Message}");
            HasError = true;
        }
    }

    public void UnloadThumbnail()
    {
        Bitmap? old;
        lock (_thumbnailLock)
        {
            if (Thumbnail != null)
            {
                Console.WriteLine($"Media unloaded, {FileName}");
            }
            old = Thumbnail;
            Thumbnail = null;
            ThumbnailLoaded = false;
        }
        old?.Dispose();
    }

    private static string FormatFileSize(long bytes)
    {
        string[] sizes = ["B", "KB", "MB", "GB", "TB"];
        double d = bytes;
        int idx = 0;
        while (d >= 1024 && idx < sizes.Length - 1) { d /= 1024; idx++; }
        return $"{d:F1} {sizes[idx]}";
    }

    private static string FormatDuration(double seconds)
    {
        if (seconds <= 0) return "--:--";
        var ts = TimeSpan.FromSeconds(seconds);
        return ts.TotalHours >= 1
            ? ts.ToString(@"h\:mm\:ss")
            : ts.ToString(@"mm\:ss");
    }
}
