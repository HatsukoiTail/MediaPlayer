


namespace AvaloniaMedia.FFmpeg.Clock;

public enum ClockType { Video, Audio, External }

public class Clock
{
    public double Time { get; set; }
    public double Drift { get; set; }
    public double LastUpdateTime { get; set; }
    public double Speed { get; set; }
    public int Serial { get; set; }
    public int SourceSerial { get; set; } = -1;
    public bool IsPaused { get; set; }
}