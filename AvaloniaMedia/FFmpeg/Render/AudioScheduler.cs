using System;
using System.Diagnostics;
using System.Threading.Tasks;
using AvaloniaMedia.FFmpeg.Clock;
using AvaloniaMedia.FFmpeg.Queue;
using NAudio.Wave;

namespace AvaloniaMedia.FFmpeg.Render;

public class AudioScheduler : IDisposable
{
    private WaveOutEvent? wavePlayer;
    private AudioProvider? waveProvider;

    public bool IsOpen => waveProvider != null && waveProvider.IsOpen;
    public bool IsRunning => wavePlayer != null && wavePlayer.PlaybackState == PlaybackState.Playing;

    public float Volume
    {
        get => wavePlayer?.Volume ?? 1.0f;
        set
        {
            wavePlayer?.Volume = Math.Clamp(value, 0f, 1f);
        }
    }

    public void Start(FrameQueue queue, int sampleRate, int channels, ClockManager clock)
    {
        Debug.Assert(IsOpen == false);

        waveProvider = new AudioProvider();
        waveProvider.Open(queue, sampleRate, channels, clock);

        wavePlayer = new WaveOutEvent();
        wavePlayer.Init(waveProvider);

        // Pass NAudio's desired latency to the provider for audio clock compensation
        waveProvider.HardwareLatency = wavePlayer.DesiredLatency / 1000.0;

        wavePlayer.Play();
    }

    public void Pause()
    {
        wavePlayer?.Pause();
    }

    public void Play()
    {
        wavePlayer?.Play();
    }

    public void Stop()
    {
        if (wavePlayer != null)
        {
            wavePlayer.Stop();
            wavePlayer.Dispose();
            wavePlayer = null;
        }
        waveProvider?.Dispose();
        waveProvider = null;
    }

    public Task StopAsync() => Task.Run(Stop);

    public void Dispose()
    {
        Stop();
        GC.SuppressFinalize(this);
    }
}
