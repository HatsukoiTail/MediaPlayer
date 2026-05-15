using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Threading.Tasks;
using Avalonia.Threading;
using AvaloniaMedia.FFmpeg;
using AvaloniaMedia.FFmpeg.MediaInfo;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace AvaloniaMedia.ViewModels;

public partial class MediaPlayerViewModel : ViewModelBase
{
    public MediaContext MediaContext { get; } = new();

    private DispatcherTimer? progressTimer;

    [ObservableProperty]
    public partial string Title { get; set; } = "这是一个空标题";

    [ObservableProperty]
    public partial bool IsPlaying { get; set; }

    [ObservableProperty]
    public partial bool IsPaused { get; set; } = true;

    [ObservableProperty]
    public partial double Position { get; set; }

    [ObservableProperty]
    public partial double Duration { get; set; } = 0;

    public ObservableCollection<string> PlayList { get; set; } = [ "sadasdaisda", "sdasdasda sa ", "asdausgduagsd", "ashdiashdiahsdiha" ];

    [ObservableProperty]
    public partial double Volume { get; set; } = 80;

    [ObservableProperty]
    public partial double Speed { get; set; } = 1.0;

    [ObservableProperty]
    public partial string CurrentTimeText { get; set; } = "00:00";

    [ObservableProperty]
    public partial string DurationText { get; set; } = "00:00";

    public bool IsSeeking { get; set; }

    partial void OnVolumeChanged(double value)
    {
        MediaContext.Volume = (float)(value / 100.0);
    }

    partial void OnSpeedChanged(double value)
    {
        MediaContext.Speed = value;
    }

    [RelayCommand]
    public async Task OpenFile(string file)
    {
        // Console.WriteLine(MediaContext.v)

        // var path = @"E:\Media\Movies\[AKT] 爱莉希雅 V1 爱莉希雅.mp4";
        // var path = @"E:\Media\Collection\[SFcongee] 李素裳 舰长与合欢门弟子李素裳的床上大战.mp4";
        var path = @"F:\Work\MediaPlayer\Resource\new world.mp4";

        // var info = new MediaInfo(path);
        // Console.WriteLine($"Media Info: {info}");
        // var metadata = info.MetaData;

        await MediaContext.OpenAsync(path);
        Title = MediaContext.Title ?? Path.GetFileName(path);

        Duration = MediaContext.Duration;
        DurationText = FormatTime(Duration);
        Position = 0;
        CurrentTimeText = "00:00";

        MediaContext.Start();

        IsPlaying = true;
        IsPaused = false;

        progressTimer ??= new DispatcherTimer(
            TimeSpan.FromMilliseconds(200),
            DispatcherPriority.Background,
            OnProgressTick);
        progressTimer.Start();
    }

    [RelayCommand]
    public async Task TogglePlayPause()
    {
        if (!MediaContext.IsOpen)
        {
            await OpenFile(@"E:\Media\Movies\[AKT] 爱莉希雅 V1 爱莉希雅.mp4");
            return;
        }

        if (IsPlaying)
        {
            MediaContext.Pause();
            IsPlaying = false;
            IsPaused = true;
        }
        else
        {
            MediaContext.Resume();
            IsPlaying = true;
            IsPaused = false;
        }
    }

    [RelayCommand]
    public async Task Seek()
    {
        if (!MediaContext.IsOpen)
            return;

        IsSeeking = false;
        await MediaContext.SeekAsync(Position);
    }

    [RelayCommand]
    public void StepNext()
    {
        MediaContext.StepNext();
    }

    [RelayCommand]
    public void SetVolume(double value)
    {
        Volume = value;
    }

    [RelayCommand]
    public void SetSpeed(double value)
    {
        Speed = value;
    }

    [RelayCommand]
    public void ToggleFullscreen()
    {
        // Handled in code-behind
    }

    public void Close()
    {
        progressTimer?.Stop();
        MediaContext.Close();
    }

    [RelayCommand]
    public async Task CloseAsync()
    {
        progressTimer?.Stop();
        await MediaContext.CloseAsync();
    }

    [RelayCommand]
    public void SwitchMedia(string mediaName)
    {
        Console.WriteLine($"Request to play {mediaName}");
    }

    private void OnProgressTick(object? sender, EventArgs e)
    {
        if (!MediaContext.IsOpen || IsSeeking)
            return;

        Position = MediaContext.Position;
        // Console.WriteLine(Position);
        CurrentTimeText = FormatTime(Position);

        // Update play state from MediaContext
        if (IsPlaying != MediaContext.IsPlaying)
        {
            IsPlaying = MediaContext.IsPlaying;
            IsPaused = MediaContext.IsPaused;
        }

        // Detect end of playback
        if (Duration > 0 && Position >= Duration - 0.5)
        {
            IsPlaying = false;
            IsPaused = false;
        }
    }

    private static string FormatTime(double seconds)
    {
        if (double.IsNaN(seconds) || seconds < 0)
            return "00:00";

        var ts = TimeSpan.FromSeconds(seconds);
        if (ts.TotalHours >= 1)
            return ts.ToString(@"h\:mm\:ss");
        return ts.ToString(@"mm\:ss");
    }
}
