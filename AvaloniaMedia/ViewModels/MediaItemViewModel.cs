using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Media.Imaging;
using AvaloniaMedia.Helper;
using AvaloniaMedia.FFmpeg.Model;
using AvaloniaMedia.FFmpeg.Thumbnail;
using AvaloniaMedia.Services;
using CommunityToolkit.Mvvm.ComponentModel;

namespace AvaloniaMedia.ViewModels;

public partial class MediaItemViewModel : ObservableObject
{
    private readonly ThumbnailCache _cache;
    private CancellationTokenSource? _loadCts;

    public string FilePath { get; }
    public string FileName => Path.GetFileName(FilePath);
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

    [ObservableProperty]
    public partial MediaViewMode ViewMode { get; set; }

    public bool IsLargeView => ViewMode == MediaViewMode.LargeIcons;
    public bool IsMediumView => ViewMode == MediaViewMode.MediumIcons;
    public bool IsListView => ViewMode == MediaViewMode.List;

    partial void OnViewModeChanged(MediaViewMode value)
    {
        OnPropertyChanged(nameof(IsLargeView));
        OnPropertyChanged(nameof(IsMediumView));
        OnPropertyChanged(nameof(IsListView));
    }

    public bool IsVideo => MediaHelper.IsVideo(FilePath);
    public bool IsAudio => MediaHelper.IsAudio(FilePath);
    public bool IsImage => MediaHelper.IsImage(FilePath);

    public string MediaTypeIcon => IsVideo ? "🎬" : IsAudio ? "🎵" : IsImage ? "🖼" : "📄";
    public string SubtitleText => IsImage ? $"{Width} × {Height}" : DurationText;

    public MediaItemViewModel(string filePath, ThumbnailCache cache)
    {
        FilePath = filePath;
        _cache = cache;
    }

    public async Task LoadAsync()
    {
        if (ThumbnailLoaded || HasError) return;

        // Try cache first
        var cached = _cache.Acquire(FilePath);
        if (cached is not null)
        {
            Thumbnail = cached;
            ThumbnailLoaded = true;
            return;
        }

        _loadCts?.Cancel();
        _loadCts = new CancellationTokenSource();
        var token = _loadCts.Token;

        try
        {
            Bitmap? result = null;
            if (MediaHelper.IsVideo(FilePath))
            {
                var option = new FrameLoader.Option
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

            Thumbnail = result;
            ThumbnailLoaded = true;
            _cache.Put(FilePath, result);
        }
        catch (FFmpegException)
        {
            HasError = true;
        }
        catch (OperationCanceledException) { }
    }

    public void Unload()
    {
        _loadCts?.Cancel();
        if (ThumbnailLoaded && Thumbnail is not null)
        {
            _cache.Release(FilePath);
            Thumbnail = null;
            ThumbnailLoaded = false;
        }
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
