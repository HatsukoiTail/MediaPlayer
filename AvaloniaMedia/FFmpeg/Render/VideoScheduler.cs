


using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using AvaloniaMedia.FFmpeg.Clock;
using AvaloniaMedia.FFmpeg.Model;
using AvaloniaMedia.FFmpeg.Queue;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Render;

public class VideoScheduler
{
    public IVideoRender? Video { get; set; }
    public VideoView? VideoView { get; set; }
    public required ClockManager Clock { get; set; }
    private FrameQueue? videoQueue;
    private FrameQueue? subtitleQueue;
    private Thread? thread;
    private readonly Stopwatch stopwatch = new();

    private Frame? lastFrame;
    private double frameTime = double.NaN;

    private string currentSubtitleText = string.Empty;
    private double subtitleEndTime = double.NaN;

    public int Serial { get; set; } = -1;

    #region Scheduler State
    private CancellationTokenSource? cancelToken;
    public bool IsRunning => thread != null && thread.IsAlive;
    public bool IsPaused { get; set; } = false;
    public bool IsStep { get; set; }
    public event Action<double>? StepCompleted;
    #endregion


    private const double MinRefreshInterval = 0.01;
    private const double MinSyncThreshold = 0.04;
    private const double MaxSyncThreshold = 0.1;


    public void Start(FrameQueue videoQueue, FrameQueue? subtitleQueue = null)
    {
        this.videoQueue = videoQueue;
        this.subtitleQueue = subtitleQueue;
        stopwatch.Restart();
        frameTime = GetReleativeTime();
        cancelToken ??= new CancellationTokenSource();
        thread = new Thread(() => RefreshLoop(cancelToken.Token));
        thread.Start();
    }

    public void Stop()
    {
        cancelToken?.Cancel();
        thread?.Join();
        cancelToken?.Dispose();
        cancelToken = null;
        thread = null;
    }

    public Task StopAsync() => Task.Run(Stop);

    private void RefreshLoop(CancellationToken token)
    {
        while (token.IsCancellationRequested == false)
        {
            double remainTime = Refresh();
            if (remainTime > 0)
            {
                int sleepTime = Math.Max(1, (int)(remainTime * 1000));
                // Console.WriteLine($"Scheduler sleeping for {remainTime:F3}s ({sleepTime}ms)");
                Thread.Sleep(sleepTime);
            }
        }
        Console.WriteLine("VideoScheduler loop exited.");
    }

    private double Refresh()
    {
        Debug.Assert(videoQueue != null);

        // 首先处理音频频谱显示，如果需要

        // 渲染视频帧
        if (videoQueue.Count == 0)
        {
            return MinRefreshInterval;
        }

        if (lastFrame == null)
        {
            lastFrame = videoQueue.Dequeue();
            if (lastFrame == null)
            {
                return MinRefreshInterval; // 稍后再次尝试
            }
            // 否则直接渲染该帧
            RenderFrame(lastFrame);
            if (IsStep)
            {
                IsStep = false;
                IsPaused = true;
                StepCompleted?.Invoke(lastFrame.Time);
            }
            return MinRefreshInterval;
        }

        var frame = videoQueue.Peek();
        if (frame == null)
        {
            return MinRefreshInterval;
        }
        if (frame.Serial != Serial)
        {
            videoQueue.Dequeue()?.Dispose();
            return 0;
        }
        if (IsPaused && !IsStep)
        {
            return MinRefreshInterval;
        }
        if (lastFrame.Serial != frame.Serial)
        {
            frameTime = GetReleativeTime();
            lastFrame.Dispose();
            lastFrame = null;
            return MinRefreshInterval;
        }

        // 计算名义上的延时，即前后两帧的时间差
        var duration = GetFrameDuration(frame, lastFrame);
        var delay = GetTargetDelay(duration);

        var currentTime = GetReleativeTime();

        if (currentTime < frameTime + delay)
        {
            // 尚未到显示时间
            double remainTime = frameTime + delay - currentTime;
            // Console.WriteLine($"Not time: delay = {delay}, diff = {currentTime - frameTime}, remain = {remainTime}");
            return Math.Max(MinRefreshInterval, remainTime);
        }

        // Console.WriteLine($"clock delay = {delay}, frame diff = {currentTime - frameTime}, current = {currentTime}, last = {frameTime}, duration = {duration}");

        frameTime += delay;
        if (delay > 0 && currentTime - frameTime > MaxSyncThreshold)
        {
            frameTime = currentTime;
        }

        // 更新视频时钟
        Clock.SetClock(ClockType.Video, frame.Time, frame.Serial);
        Clock.SyncClock(ClockType.Video);

        // Console.WriteLine($"Update video clock, {frame.Time}");

        // 丢帧
        if (!IsStep && videoQueue.Count > 1)
        {
            var nextFrame = videoQueue.PeekNext();
            if (nextFrame != null && nextFrame.Serial == Serial)
            {
                var currentFrameDuration = GetFrameDuration(nextFrame, frame);
                if (IsStep == false && Clock.SyncMode != ClockType.Video && currentTime > frameTime + currentFrameDuration)
                {
                    var dropFrame = videoQueue.Dequeue();
                    Console.WriteLine($"Drop video frame in scheduler, pts = {dropFrame?.Pts}");
                    dropFrame?.Dispose();
                    return 0;
                }
            }
        }

        videoQueue.Dequeue();
        RenderFrame(frame);
        lastFrame?.Dispose();
        lastFrame = frame;

        if (IsStep)
        {
            IsStep = false;
            IsPaused = true;
            StepCompleted?.Invoke(frame.Time);
        }

        return MinRefreshInterval;
    }

    public void Flush()
    {
        frameTime = GetReleativeTime();
    }

    private void RenderFrame(Frame frame)
    {
        if (subtitleQueue != null)
        {
            RenderSubtitle(frame.Time);
        }
        frame.Subtitle = currentSubtitleText.Length > 0 ? currentSubtitleText : null;
        Video?.Draw(frame);
    }

    private unsafe void RenderSubtitle(double videoTime)
    {
        if (subtitleQueue == null)
            return;

        // Check if current subtitle has expired
        if (!string.IsNullOrEmpty(currentSubtitleText) && videoTime >= subtitleEndTime)
        {
            currentSubtitleText = string.Empty;
            subtitleEndTime = double.NaN;
        }

        // Try to dequeue new subtitles that should be displayed
        while (subtitleQueue.Count > 0)
        {
            var subtitle = subtitleQueue.Peek();
            if (subtitle == null)
                break;

            // If subtitle starts in the future, stop
            if (subtitle.Time > videoTime)
                break;

            subtitleQueue.Dequeue();

            // If subtitle is still valid, extract text
            double endTime = subtitle.Time + subtitle.Duration;
            if (endTime > videoTime)
            {
                AVSubtitle* sub = subtitle.SubtitlePointer;
                if (sub != null && sub->num_rects > 0)
                {
                    for (uint i = 0; i < sub->num_rects; i++)
                    {
                        AVSubtitleRect* rect = sub->rects[i];
                        if (rect->type == AVSubtitleType.SUBTITLE_TEXT || rect->type == AVSubtitleType.SUBTITLE_ASS)
                        {
                            string? text = Marshal.PtrToStringAnsi((IntPtr)rect->text);
                            if (!string.IsNullOrEmpty(text))
                            {
                                // For ASS subtitles, skip dialogue prefix
                                if (rect->type == AVSubtitleType.SUBTITLE_ASS && text.StartsWith("Dialogue:"))
                                {
                                    // Extract text after the 9th comma (the Text field)
                                    int commaCount = 0;
                                    int textStart = 0;
                                    for (int j = 0; j < text.Length; j++)
                                    {
                                        if (text[j] == ',')
                                        {
                                            commaCount++;
                                            if (commaCount == 9)
                                            {
                                                textStart = j + 1;
                                                break;
                                            }
                                        }
                                    }
                                    if (textStart > 0)
                                        text = text.Substring(textStart);
                                }
                                currentSubtitleText = text;
                            }
                        }
                    }
                }
                subtitleEndTime = endTime;
            }

            subtitle.Dispose();
        }
    }

    private static double GetFrameDuration(Frame cur, Frame? last)
    {
        if (last == null || last.Serial != cur.Serial)
        {
            return cur.Duration > 0 ? cur.Duration : 0.04; // 默认FPS=25
        }
        double diff = cur.Time - last.Time;
        if (double.IsNaN(diff) || diff <= 0 || diff > 10.0)
        {
            return cur.Duration > 0 ? cur.Duration : 0.04;
        }
        return diff;
    }

    private double GetTargetDelay(double duration)
    {
        if (Clock.SyncMode == ClockType.Video)
            return duration;

        var masterClock = Clock.GetMasterClock();
        var videoClock = Clock.GetClock(ClockType.Video);

        double diff = videoClock - masterClock;

        double syncThreshold = Math.Max(MinSyncThreshold, Math.Min(MaxSyncThreshold, duration));
        if (!double.IsNaN(diff) && Math.Abs(diff) < 10.0)
        {
            if (diff < -syncThreshold)
            {
                // 视频太慢，需要加快处理
                return Math.Max(0, duration + diff);
            }
            if (diff > syncThreshold)
            {
                if (duration > MaxSyncThreshold)
                    return duration + diff;
                else
                    return 2 * duration;
            }
        }

        return duration;
    }

    private double GetReleativeTime()
    {
        return stopwatch.Elapsed.TotalSeconds;
    }
}