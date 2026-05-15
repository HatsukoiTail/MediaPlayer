using System;
using System.Diagnostics;
using System.Threading.Tasks;
using Avalonia.Controls;
using AvaloniaMedia.FFmpeg.Clock;
using AvaloniaMedia.FFmpeg.Decode;
using AvaloniaMedia.FFmpeg.Demux;
using AvaloniaMedia.FFmpeg.Model;
using AvaloniaMedia.FFmpeg.Render;

namespace AvaloniaMedia.FFmpeg;

public class MediaContext : IDisposable
{
    #region Private Fileds
    private readonly ClockManager clock;
    private readonly Demuxer demuxer;
    private readonly VideoDecoder videoDecoder;
    private readonly AudioDecoder audioDecoder;
    private readonly SubtitleDecoder subtitleDecoder;
    private readonly VideoScheduler videoScheduler;
    private readonly AudioScheduler audioScheduler;
    #endregion

    #region Public Properties

    public bool IsOpen { get; private set; } = false;
    public bool IsPlaying { get; private set; } = false;
    public bool IsPaused => IsOpen != true || !IsPlaying;
    public double Duration => demuxer.IsOpen == true ? demuxer.FormatContext.Duration : double.NaN;
    public string? Title => demuxer.IsOpen == true ? demuxer.FormatContext.Title : null;
    public double Position
    {
        get
        {
            var time = clock.GetMasterClock();
            Console.WriteLine($"Position = {time}");
            return time;
        }
        set => Seek(value);
    }
    public float Volume
    {
        get => audioScheduler.Volume;
        set => audioScheduler.Volume = value;
    }
    public double Speed
    {
        get => _speed;
        set => SetSpeed(value);
    }
    public Control? VideoControl
    {
        get => videoScheduler.VideoView as Control;
        set
        {
            if (value is IVideoRender render)
                videoScheduler.Video = render;
            videoScheduler.VideoView = value as VideoView;
        }
    }
    public event Action<double>? SeekFinished;

    #endregion


    #region 辅助字段
    private TaskCompletionSource? seekTCS;
    #endregion

    public MediaContext()
    {
        // 实例化主要结构
        clock = new ClockManager();
        demuxer = new Demuxer();
        videoDecoder = new VideoDecoder() { Clock = clock };
        audioDecoder = new AudioDecoder();
        subtitleDecoder = new SubtitleDecoder();
        videoScheduler = new VideoScheduler() { Clock = clock };
        audioScheduler = new AudioScheduler();

        // 绑定事件
        demuxer.DemuxEof += () =>
        {

        };
        demuxer.SeekDone += OnDemuxerSeekDone;
        videoScheduler.StepCompleted += _ =>
        {
            clock.SetPaused(true);
            audioScheduler.Pause();
            IsPlaying = false;
        };
    }

    private void Seek(double seconds)
    {
        demuxer.Seek(seconds);
    }

    public async Task SeekAsync(double seconds)
    {
        seekTCS ??= new TaskCompletionSource();
        await demuxer.SeekAsync(seconds);
        await seekTCS.Task;
        seekTCS = null;
    }

    private double _speed = 1.0;

    private void SetSpeed(double speed)
    {
        if (Math.Abs(_speed - speed) < 0.001)
            return;

        _speed = speed;
        clock.SetSpeed(ClockType.Video, speed);
        clock.SetSpeed(ClockType.External, speed);
        audioDecoder.Speed = speed;
        audioDecoder.InvalidateFilter();
    }

    public void StepNext()
    {
        if (!IsOpen)
            return;

        if (IsPaused)
        {
            clock.SetPaused(false);
            videoScheduler.IsPaused = false;
            audioScheduler.Play();
            IsPlaying = true;
        }

        videoScheduler.IsStep = true;
    }

    public void Open(string filename)
    {
        if (IsOpen)
            throw new InvalidOperationException("MediaContext is already open.");

        demuxer.Open(filename);
        OpenDecoder();
        SetSyncMode();
        IsOpen = true;
    }

    public async Task OpenAsync(string filename)
    {
        if (IsOpen)
            throw new InvalidOperationException("MediaContext is already open.");

        await demuxer.OpenAsync(filename);
        await OpenDecoderAsync();
        SetSyncMode();

        IsOpen = true;
    }

    public void Start()
    {
        // 启动时钟
        clock.Init();

        demuxer.Start();

        if (videoDecoder.IsOpen)
            videoDecoder.Start();
        if (audioDecoder.IsOpen)
            audioDecoder.Start();
        if (subtitleDecoder.IsOpen)
            subtitleDecoder.Start();

        if (audioDecoder.IsOpen)
        {
            audioScheduler.Start(audioDecoder.FrameQueue, audioDecoder.SampleRate, audioDecoder.ChannelCount, clock);
        }

        if (videoDecoder.IsOpen)
        {
            // videoScheduler的Serial始终于VideoPacketQueue的Serial保持一致
            videoScheduler.Serial = demuxer.VideoPacketQueue.Serial;
            videoScheduler.Start(videoDecoder.FrameQueue, subtitleDecoder.IsOpen ? subtitleDecoder.FrameQueue : null);
        }

        IsPlaying = true;
    }

    public void Resume()
    {
        if (!IsOpen || !IsPaused)
            return;

        clock.SetPaused(false);
        videoScheduler.IsPaused = false;
        audioScheduler.Play();

        IsPlaying = true;
    }

    public void Pause()
    {
        Console.WriteLine($"Request pause, {IsOpen}, {IsPaused}");
        if (!IsOpen || IsPaused)
            return;

        audioScheduler.Pause();
        clock.SetPaused(true);
        videoScheduler.IsPaused = true;

        IsPlaying = false;
    }

    private void Stop()
    {
        IsPlaying = false;

        // demuxer.VideoPacketQueue.Abort();
        // demuxer.AudioPacketQueue.Abort();
        // demuxer.SubtitlePacketQueue.Abort();
        // videoDecoder.FrameQueue.Abort();
        // audioDecoder.FrameQueue.Abort();
        // subtitleDecoder.FrameQueue.Abort();

        videoScheduler.Stop();
        audioScheduler.Stop();
        videoDecoder.Stop();
        audioDecoder.Stop();
        subtitleDecoder.Stop();
        demuxer.Stop();
    }

    private async Task StopAsync()
    {
        if (videoScheduler.IsRunning)
            await videoScheduler.StopAsync();

        if (audioScheduler.IsRunning)
            await audioScheduler.StopAsync();

        if (videoDecoder.IsRunning)
            await videoDecoder.StopAsync();

        if (audioDecoder.IsRunning)
            await audioDecoder.StopAsync();

        if (subtitleDecoder.IsRunning)
            await subtitleDecoder.StopAsync();

        if (demuxer.IsRunning)
            await demuxer.StopAsync();

        IsPlaying = true;
    }

    public void Close()
    {
        Console.WriteLine("Waiting for MediaContext closing");
        if (!IsOpen)
            return;

        videoScheduler.Stop();
        audioScheduler.Stop();

        videoDecoder.Close();
        audioDecoder.Close();
        subtitleDecoder.Close();

        demuxer.Close();

        IsOpen = false;

        Console.WriteLine("MediaContext closed");
    }

    public async Task CloseAsync()
    {
        if (IsOpen == false)
            return;

        await StopAsync();

        await videoDecoder.CloseAsync();
        await audioDecoder.CloseAsync();
        await subtitleDecoder.CloseAsync();

        IsOpen = false;
    }

    public void Dispose()
    {
        Close();
        demuxer.Dispose();
        videoDecoder.Dispose();
        audioDecoder.Dispose();
        subtitleDecoder.Dispose();
        GC.SuppressFinalize(this);
    }

    private void OnDemuxerSeekDone(double target)
    {
        if (IsPaused)
        {
            StepNext();
        }

        var mainSerial = demuxer.Serial;

        videoScheduler.Serial = mainSerial;
        clock.Reset(target, mainSerial);

        seekTCS?.TrySetResult();

        SeekFinished?.Invoke(target);
    }

    private async Task OpenDecoderAsync()
    {
        if (demuxer.HasVideo)
        {
            await videoDecoder.OpenAsync(demuxer.FormatContext, demuxer.VideoStreamIndex, demuxer.VideoPacketQueue);
        }
        if (demuxer.HasAudio)
        {
            await audioDecoder.OpenAsync(demuxer.FormatContext, demuxer.AudioStreamIndex, demuxer.AudioPacketQueue);
        }
        if (demuxer.HasSubtitle)
        {
            await subtitleDecoder.OpenAsync(demuxer.FormatContext, demuxer.SubtitleStreamIndex, demuxer.SubtitlePacketQueue);
        }
    }

    private unsafe void OpenDecoder()
    {
        if (demuxer.HasVideo)
            videoDecoder.Open(demuxer.FormatContext, demuxer.GetStream(MediaStreamType.Video), demuxer.VideoPacketQueue);
        if (demuxer.HasAudio)
            audioDecoder.Open(demuxer.FormatContext, demuxer.GetStream(MediaStreamType.Audio), demuxer.AudioPacketQueue);
        if (demuxer.HasSubtitle)
            subtitleDecoder.Open(demuxer.FormatContext, demuxer.GetStream(MediaStreamType.Subtitle), demuxer.SubtitlePacketQueue);
    }

    private void SetSyncMode()
    {
        if (demuxer.HasAudio)
        {
            clock.SyncMode = ClockType.Audio;
        }
        else
        {
            clock.SyncMode = ClockType.External;
        }
    }
}