using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Rendering.SceneGraph;
using Avalonia.Threading;
using AvaloniaMedia.FFmpeg.Model;
using SkiaSharp;

namespace AvaloniaMedia.FFmpeg.Render;

public class HwRender : ICustomDrawOperation
{
    public Rect Bounds => throw new NotImplementedException();

    public void Dispose()
    {
        throw new NotImplementedException();
    }

    public bool Equals(ICustomDrawOperation? other)
    {
        throw new NotImplementedException();
    }

    public bool HitTest(Point p)
    {
        throw new NotImplementedException();
    }

    public void Render(ImmediateDrawingContext context)
    {
        // var texture = new SKBackendTexture(
        // SKImage.FromTexture()
        throw new NotImplementedException();
    }

}

public class SkiaVideoRender : Control, IVideoRender
{
    private SKBitmap? currentFrame;
    private WriteableBitmap? renderTarget;
    private readonly object frameLock = new();
    private string subtitleText = string.Empty;

    public unsafe void Draw(Frame frame)
    {
        // Only accept RGBA frames; other formats indicate a pipeline bug
        if (frame.Format != (int)global::FFmpeg.AutoGen.AVPixelFormat.AV_PIX_FMT_RGBA)
        {
            Console.WriteLine($"SkiaVideoRender: unexpected pixel format {frame.Format}, skipping frame.");
            return;
        }

        int width = frame.Width;
        int height = frame.Height;
        if (width <= 0 || height <= 0)
            return;

        var info = new SKImageInfo(width, height, SKColorType.Rgba8888, SKAlphaType.Opaque);
        var bitmap = new SKBitmap(info);
        var dstPtr = (byte*)bitmap.GetPixels();
        var srcPtr = frame.DataPointer(0);
        var srcStride = frame.LineSize(0);
        var dstRowBytes = info.RowBytes;
        var copyRowBytes = Math.Min(dstRowBytes, srcStride);

        for (int y = 0; y < height; y++)
        {
            Buffer.MemoryCopy(
                srcPtr + y * (long)srcStride,
                dstPtr + y * (long)dstRowBytes,
                copyRowBytes, copyRowBytes);
        }

        var sub = frame.Subtitle;

        lock (frameLock)
        {
            var old = currentFrame;
            currentFrame = bitmap;
            old?.Dispose();
            subtitleText = sub ?? string.Empty;
        }

        Dispatcher.UIThread.Post(InvalidateVisual, DispatcherPriority.Render);
    }

    public override void Render(DrawingContext context)
    {
        context.FillRectangle(Brushes.Black, new Rect(Bounds.Size));

        SKBitmap? frame;
        string sub;
        lock (frameLock)
        {
            frame = currentFrame;
            sub = subtitleText;
        }

        if (frame == null || frame.Width <= 0 || frame.Height <= 0)
            return;

        var controlW = Math.Max(1, (int)Bounds.Width);
        var controlH = Math.Max(1, (int)Bounds.Height);

        if (renderTarget is null
            || renderTarget.PixelSize.Width != controlW
            || renderTarget.PixelSize.Height != controlH)
        {
            renderTarget?.Dispose();
            renderTarget = new WriteableBitmap(
                new PixelSize(controlW, controlH),
                new Vector(96, 96),
                PixelFormat.Rgba8888,
                AlphaFormat.Opaque);
        }

        using var buffer = renderTarget.Lock();
        var skInfo = new SKImageInfo(controlW, controlH, SKColorType.Rgba8888, SKAlphaType.Opaque);
        using var surface = SKSurface.Create(skInfo, buffer.Address, buffer.RowBytes);
        if (surface == null) return;

        var canvas = surface.Canvas;
        canvas.Clear(SKColors.Black);

        double scaleW = (double)controlW / frame.Width;
        double scaleH = (double)controlH / frame.Height;
        double scale = Math.Min(scaleW, scaleH);
        var drawW = (float)(frame.Width * scale);
        var drawH = (float)(frame.Height * scale);
        var drawX = (controlW - drawW) / 2f;
        var drawY = (controlH - drawH) / 2f;
        var destRect = new SKRect(drawX, drawY, drawX + drawW, drawY + drawH);

#pragma warning disable CS0618
        using var paint = new SKPaint { FilterQuality = SKFilterQuality.High, IsAntialias = true };
#pragma warning restore CS0618

        try
        {
            canvas.DrawBitmap(frame, destRect, paint);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"SkiaVideoRender: DrawBitmap failed: {ex.Message}");
            return;
        }

        if (!string.IsNullOrEmpty(sub))
        {
            float fontSize = (float)Math.Max(12, controlH * 0.035);
            using var font = new SKFont(SKTypeface.FromFamilyName("Microsoft YaHei", SKFontStyle.Normal), fontSize);

            float textWidth = font.MeasureText(sub);
            float textX = drawX + (drawW - textWidth) / 2f;
            float textY = drawY + drawH - fontSize - 4f;
            float baselineY = textY + fontSize;

            using var shadowPaint = new SKPaint { Color = SKColors.Black, IsAntialias = true };
            canvas.DrawText(sub, textX + 1, baselineY + 1, font, shadowPaint);

            using var textPaint = new SKPaint { Color = SKColors.White, IsAntialias = true };
            canvas.DrawText(sub, textX, baselineY, font, textPaint);
        }

        canvas.Flush();
        context.DrawImage(renderTarget, new Rect(0, 0, controlW, controlH));
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnDetachedFromVisualTree(e);

        lock (frameLock)
        {
            currentFrame?.Dispose();
            currentFrame = null;
        }

        renderTarget?.Dispose();
        renderTarget = null;
    }
}
