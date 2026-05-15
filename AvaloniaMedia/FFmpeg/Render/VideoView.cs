using System;
using System.Threading;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using AvaloniaMedia.FFmpeg.Model;

namespace AvaloniaMedia.FFmpeg.Render;

public class VideoView : Control, IVideoRender
{
    private WriteableBitmap? bitmap;
    private WriteableBitmap? renderBitmap;
    private string subtitleText = string.Empty;

    public string? Subtitle
    {
        get => subtitleText;
        set
        {
            subtitleText = value ?? string.Empty;
            Avalonia.Threading.Dispatcher.UIThread.Post(InvalidateVisual, Avalonia.Threading.DispatcherPriority.Render);
        }
    }

    public void Draw(Frame frame)
    {
        unsafe
        {
            Draw(frame.DataPointer(0), frame.Width, frame.Height, frame.LineSize(0));
        }
    }

    public unsafe void Draw(byte* data, int width, int height, int stride, string? subtitle = null)
    {
        if (bitmap is null || bitmap.PixelSize.Width != width || bitmap.PixelSize.Height != height)
        {
            bitmap = new WriteableBitmap(new PixelSize(width, height), new Vector(96, 96), PixelFormat.Rgba8888, AlphaFormat.Opaque);
        }

        using var buffer = bitmap.Lock();

        if (buffer.RowBytes != stride)
        {
            for (int i = 0; i < height; i++)
            {
                var srcAddr = new IntPtr(data + i * stride);
                var dstAddr = new IntPtr(buffer.Address.ToInt64() + i * buffer.RowBytes);
                Buffer.MemoryCopy(srcAddr.ToPointer(), dstAddr.ToPointer(), stride, stride);
            }
        }
        else
        {
            var length = width * height * 4;
            Buffer.MemoryCopy(data, buffer.Address.ToPointer(), length, length);
        }

        var oldBitmap = Interlocked.Exchange(ref renderBitmap, bitmap);

        if (!string.IsNullOrEmpty(subtitle))
            subtitleText = subtitle;

        Avalonia.Threading.Dispatcher.UIThread.Post(InvalidateVisual, Avalonia.Threading.DispatcherPriority.Render);
    }

    public override void Render(DrawingContext context)
    {
        context.FillRectangle(Brushes.Black, new Rect(Bounds.Size));

        if (renderBitmap is null)
            return;

        var controlSize = Bounds.Size;
        var imageSize = new Size(renderBitmap.PixelSize.Width, renderBitmap.PixelSize.Height);
        double scale = Math.Min(controlSize.Width / imageSize.Width, controlSize.Height / imageSize.Height);
        var drawSize = new Size(imageSize.Width * scale, imageSize.Height * scale);
        var offset = new Point((controlSize.Width - drawSize.Width) / 2, (controlSize.Height - drawSize.Height) / 2);
        var destRect = new Rect(offset, drawSize);

        context.DrawImage(renderBitmap, destRect);

        // Draw subtitle text
        if (!string.IsNullOrEmpty(subtitleText))
        {
            var text = new FormattedText(
                subtitleText,
                System.Globalization.CultureInfo.CurrentCulture,
                FlowDirection.LeftToRight,
                new Typeface("Microsoft YaHei"),
                18,
                Brushes.White);

            double textX = offset.X + (drawSize.Width - text.Width) / 2;
            double textY = offset.Y + drawSize.Height - text.Height - 20;

            // Draw text shadow
            var shadowText = new FormattedText(
                subtitleText,
                System.Globalization.CultureInfo.CurrentCulture,
                FlowDirection.LeftToRight,
                new Typeface("Microsoft YaHei"),
                18,
                Brushes.Black);
            context.DrawText(shadowText, new Point(textX + 1, textY + 1));
            context.DrawText(text, new Point(textX, textY));
        }
    }
}
