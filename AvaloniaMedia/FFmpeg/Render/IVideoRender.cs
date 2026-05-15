

using AvaloniaMedia.FFmpeg.Model;

namespace AvaloniaMedia.FFmpeg.Render;

public interface IVideoRender
{
    public void Draw(Frame frame);
}