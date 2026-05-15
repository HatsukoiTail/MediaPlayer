

namespace AvaloniaMedia.FFmpeg.MediaInfo;

public record Chapter
{
    public long Id;
    public string Title = string.Empty;
    public double StartTime;
    public double EndTime;
}