using System;
using System.Diagnostics;
using AvaloniaMedia.FFmpeg.Clock;
using AvaloniaMedia.FFmpeg.Model;
using AvaloniaMedia.FFmpeg.Queue;
using NAudio.Wave;

namespace AvaloniaMedia.FFmpeg.Render;

public class AudioProvider : IWaveProvider, IDisposable
{
    public FrameQueue? FrameQueue { get; private set; }
    public WaveFormat? WaveFormat { get; private set; }
    private ClockManager? clock;

    private Frame? bufferFrame;
    private int bufferIndex;

    public double HardwareLatency { get; set; } = 0.1;

    public bool IsOpen => FrameQueue != null;

    public void Open(FrameQueue frameQueue, int sampleRate, int channels, ClockManager clock)
    {
        FrameQueue = frameQueue;
        WaveFormat = new WaveFormat(sampleRate, 16, channels);
        this.clock = clock;
    }

    public unsafe int Read(byte[] buffer, int offset, int count)
    {
        Debug.Assert(FrameQueue != null && WaveFormat != null);

        int bytesRead = 0;

        // Snapshot the frame and position for clock update,
        // so we still have the data even after the frame is fully consumed.
        Frame? clockFrame = null;
        int clockBytePosition = 0;

        fixed (byte* bufferPtr = buffer)
        {
            while (bytesRead < count)
            {
                if (bufferFrame == null)
                {
                    if (!FrameQueue.TryDequeue(out bufferFrame))
                        break;

                    bufferIndex = 0;
                }

                Debug.Assert(bufferFrame != null);

                int totalSize = bufferFrame.SampleNum * bufferFrame.BytePerSample * bufferFrame.ChannelCount;
                int remaining = totalSize - bufferIndex;
                int copySize = Math.Min(count - bytesRead, remaining);

                Buffer.MemoryCopy(bufferFrame.DataPointer(0) + bufferIndex, bufferPtr + offset + bytesRead, count - bytesRead, copySize);

                bufferIndex += copySize;
                bytesRead += copySize;

                if (bufferIndex >= totalSize)
                {
                    clockFrame = bufferFrame;
                    clockBytePosition = totalSize;
                    bufferFrame.Dispose();
                    bufferFrame = null;
                    bufferIndex = 0;
                }
            }

            if (bytesRead < count)
            {
                Console.WriteLine($"数据不足， {count - bytesRead}");
                Array.Clear(buffer, offset + bytesRead, count - bytesRead);
            }

            // Update audio clock — use the snapshot from the last fully-consumed
            // frame, or the current frame if it still has data remaining.
            if (clock != null && WaveFormat != null)
            {
                var frameForClock = clockFrame ?? bufferFrame;
                var posForClock = clockFrame != null ? clockBytePosition : bufferIndex;

                if (frameForClock != null)
                {
                    double bytesPerSecond = WaveFormat.BitsPerSample / 8.0 * WaveFormat.Channels * WaveFormat.SampleRate;
                    double audioClock = Math.Max(0, frameForClock.Time + posForClock / bytesPerSecond * frameForClock.Speed - HardwareLatency);
                    clock.SetClock(ClockType.Audio, audioClock, frameForClock.Serial);
                    clock.SyncClock(ClockType.Audio);
                    // Console.WriteLine($"Update audio clock, {audioClock}, frame time = {frameForClock.Time}, bytes per second = {bytesPerSecond}");
                }
            }

            return count;
        }
    }

    public void Dispose()
    {
        bufferFrame?.Dispose();
        bufferFrame = null;
        GC.SuppressFinalize(this);
    }
}
