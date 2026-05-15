using System;
using System.Diagnostics;

namespace AvaloniaMedia.FFmpeg.Clock;

public class ClockManager
{
    public ClockType SyncMode { get; set; }
    public double Speed => audioClock.Speed;
    public int Serial { get; set; }

    private readonly Clock videoClock = new();
    private readonly Clock audioClock = new();
    private readonly Clock externalClock = new();

    private const double AV_NOSYNC_THRESHOLD = 10.0;

    public void Init()
    {
        InitClock(videoClock);
        InitClock(audioClock);
        InitClock(externalClock);
    }

    public double GetClock(ClockType type)
    {
        return type switch
        {
            ClockType.Video => GetClock(videoClock),
            ClockType.Audio => GetClock(audioClock),
            ClockType.External => GetClock(externalClock),
            _ => throw new ArgumentException("Unknown clock type.")
        };
    }

    public double GetMasterClock()
    {
        var clock = SyncMode switch
        {
            ClockType.Video => videoClock,
            ClockType.Audio => audioClock,
            ClockType.External => externalClock,
            _ => throw new ArgumentException("Unknown sync mode.")
        };
        return GetClock(clock);
    }

    public void Reset(double time, int serial)
    {
        Serial = serial;
        SetClock(videoClock, time, serial);
        SetClock(audioClock, time, serial);
        SetClock(externalClock, time, serial);
    }

    public void SetClock(ClockType type, double time, int serial)
    {
        switch (type)
        {
            case ClockType.Video:
                SetClock(videoClock, time, serial);
                break;
            case ClockType.Audio:
                SetClock(audioClock, time, serial);
                break;
            case ClockType.External:
                SetClock(externalClock, time, serial);
                break;
            default:
                throw new ArgumentException("Unknown clock type.");
        }
    }

    public void SetSpeed(ClockType type, double speed)
    {
        var clock = type switch
        {
            ClockType.Video => videoClock,
            ClockType.Audio => audioClock,
            ClockType.External => externalClock,
            _ => throw new ArgumentException("Unknown clock type.")
        };
        var clockTime = GetClock(clock);
        SetClock(clock, clockTime, clock.Serial);
        clock.Speed = speed;
    }

    public void SetSpeed(double speed)
    {
        SetSpeed(ClockType.Video, speed);
        SetSpeed(ClockType.Audio, speed);
        SetSpeed(ClockType.External, speed);
    }

    public void SetPaused(bool paused)
    {
        if (paused == videoClock.IsPaused)
            return;

        var now = (double)Stopwatch.GetTimestamp() / Stopwatch.Frequency;

        if (paused)
        {
            videoClock.Time = GetClock(videoClock);
            videoClock.LastUpdateTime = now;
            audioClock.Time = GetClock(audioClock);
            audioClock.LastUpdateTime = now;
            externalClock.Time = GetClock(externalClock);
            externalClock.LastUpdateTime = now;
        }
        else
        {
            videoClock.LastUpdateTime = now;
            audioClock.LastUpdateTime = now;
            externalClock.LastUpdateTime = now;
        }

        videoClock.IsPaused = paused;
        audioClock.IsPaused = paused;
        externalClock.IsPaused = paused;
    }

    public void SyncClock(ClockType type)
    {
        var slaveClock = type switch
        {
            ClockType.Video => videoClock,
            ClockType.Audio => audioClock,
            _ => throw new ArgumentException("Unknown clock type.")
        };
        var externalTime = GetClock(externalClock);
        var slaveClockTime = GetClock(slaveClock);
        if (!double.IsNaN(slaveClockTime) && (double.IsNaN(externalTime) || Math.Abs(externalTime - slaveClockTime) > AV_NOSYNC_THRESHOLD))
        {
            SetClock(externalClock, slaveClockTime, slaveClock.Serial);
        }
    }

    private void InitClock(Clock clock)
    {
        clock.Speed = 1.0;
        clock.IsPaused = false;
        clock.SourceSerial = -1;
        SetClock(clock, double.NaN, -1);
    }

    private double GetClock(Clock clock)
    {
        if (clock.Serial != Serial)
        {
            return double.NaN;
        }
        if (clock.IsPaused)
        {
            return clock.Time;
        }
        else
        {
            var relativeTime = (double)Stopwatch.GetTimestamp() / Stopwatch.Frequency;
            var time = clock.Time + (relativeTime - clock.LastUpdateTime) * clock.Speed;
            return time;
        }
    }

    private void SetClock(Clock clock, double time, int serial)
    {
        var relativeTime = (double)Stopwatch.GetTimestamp() / Stopwatch.Frequency;
        clock.Time = time;
        clock.LastUpdateTime = relativeTime;
        clock.Serial = serial;
        clock.Drift = time - relativeTime;
    }
}
