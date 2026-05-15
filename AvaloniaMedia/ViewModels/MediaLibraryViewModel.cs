using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using AvaloniaMedia.Helper;
using AvaloniaMedia.Message;
using AvaloniaMedia.Services;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.DependencyInjection;
using CommunityToolkit.Mvvm.Input;
using CommunityToolkit.Mvvm.Messaging;

namespace AvaloniaMedia.ViewModels;

public enum MediaViewMode { LargeIcons, MediumIcons, List }

public enum MediaSortMode { Name, Date, Size, Duration }

public partial class MediaLibraryViewModel : ViewModelBase
{
    private readonly ThumbnailCache _thumbnailCache;

    [ObservableProperty]
    public partial string FolderPath { get; set; } = string.Empty;

    [ObservableProperty]
    public partial string SearchText { get; set; } = string.Empty;

    [ObservableProperty]
    public partial MediaViewMode ViewMode { get; set; } = MediaViewMode.LargeIcons;

    [ObservableProperty]
    public partial MediaSortMode SortMode { get; set; } = MediaSortMode.Name;

    [ObservableProperty]
    public partial bool IsLoading { get; set; }

    [ObservableProperty]
    public partial double LoadProgress { get; set; }

    [ObservableProperty]
    public partial string StatusText { get; set; } = string.Empty;

    [ObservableProperty]
    public partial int ItemCount { get; set; }

    [ObservableProperty]
    public partial int TotalCount { get; set; }

    public ObservableCollection<MediaItemViewModel> Items { get; } = [];

    private readonly List<MediaItemViewModel> _allItems = [];
    private CancellationTokenSource? _scanCts;

    public MediaSortMode[] SortModes { get; } = [MediaSortMode.Name, MediaSortMode.Date, MediaSortMode.Size, MediaSortMode.Duration];

    public MediaLibraryViewModel(ThumbnailCache thumbnailCache)
    {
        _thumbnailCache = thumbnailCache;
    }

    [RelayCommand]
    public async Task SelectFolder()
    {
        var storage = Ioc.Default.GetRequiredService<IStorageProvider>();
        var folders = await storage.OpenFolderPickerAsync(new FolderPickerOpenOptions
        {
            Title = "Select media folder",
            AllowMultiple = false
        });
        if (folders.Count == 0) return;

        FolderPath = folders[0].Path.LocalPath;
        await ScanMediaAsync(FolderPath);
    }

    [RelayCommand]
    public void SetLargeIcons() => ViewMode = MediaViewMode.LargeIcons;

    [RelayCommand]
    public void SetMediumIcons() => ViewMode = MediaViewMode.MediumIcons;

    [RelayCommand]
    public void SetListView() => ViewMode = MediaViewMode.List;

    [RelayCommand]
    public void ShowMedia(string path) => WeakReferenceMessenger.Default.Send(new MediaShownMessage(path));

    private async Task ScanMediaAsync(string path)
    {
        _scanCts?.Cancel();
        _scanCts = new CancellationTokenSource();
        var token = _scanCts.Token;

        IsLoading = true;
        LoadProgress = 0;
        StatusText = "Scanning folder...";

        // Clear old data
        _thumbnailCache.Clear();
        Items.Clear();
        _allItems.Clear();

        int scanned = 0;
        try
        {
            await foreach (var file in MediaHelper.EnumerateMediaFilesAsync(path, token))
            {
                token.ThrowIfCancellationRequested();

                var info = new FileInfo(file);
                var itemVm = new MediaItemViewModel(file, _thumbnailCache)
                {
                    FileSize = info.Length,
                    LastWriteTime = info.LastWriteTime,
                    ViewMode = ViewMode
                };

                _allItems.Add(itemVm);
                scanned++;
                LoadProgress = Math.Min(99, (double)scanned / Math.Max(scanned, 1) * 100);
                StatusText = $"Scanned {scanned} files...";

                // Batch add to UI every 20 items to keep responsive
                if (scanned % 20 == 0)
                {
                    var batch = _allItems.Skip(Items.Count).ToList();
                    await Dispatcher.UIThread.InvokeAsync(() =>
                    {
                        foreach (var item in batch)
                            Items.Add(item);
                        ItemCount = Items.Count;
                    });
                }
            }
        }
        catch (OperationCanceledException) { }

        // Add any remaining items
        var remaining = _allItems.Skip(Items.Count).ToList();
        if (remaining.Count > 0)
        {
            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                foreach (var item in remaining)
                    Items.Add(item);
                ItemCount = Items.Count;
            });
        }

        ApplyFilters();

        IsLoading = false;
        TotalCount = _allItems.Count;
        LoadProgress = 100;
        StatusText = $"{_allItems.Count} items";
    }

    partial void OnViewModeChanged(MediaViewMode value)
    {
        foreach (var item in Items)
            item.ViewMode = value;
    }

    partial void OnSearchTextChanged(string value) => ApplyFilters();
    partial void OnSortModeChanged(MediaSortMode value) => ApplyFilters();

    private void ApplyFilters()
    {
        var filtered = _allItems.AsEnumerable();

        if (!string.IsNullOrWhiteSpace(SearchText))
        {
            var search = SearchText.Trim();
            filtered = filtered.Where(f => f.FileName.Contains(search, StringComparison.OrdinalIgnoreCase));
        }

        filtered = SortMode switch
        {
            MediaSortMode.Name => filtered.OrderBy(f => f.FileName),
            MediaSortMode.Date => filtered.OrderByDescending(f => f.LastWriteTime),
            MediaSortMode.Size => filtered.OrderByDescending(f => f.FileSize),
            MediaSortMode.Duration => filtered.OrderByDescending(f => f.Duration),
            _ => filtered
        };

        var list = filtered.ToList();

        Dispatcher.UIThread.Post(() =>
        {
            Items.Clear();
            foreach (var item in list)
                Items.Add(item);
            ItemCount = Items.Count;
        });
    }
}
