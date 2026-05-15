using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Threading;
using AvaloniaMedia.FFmpeg.Model;
using FFmpeg.AutoGen;
using SkiaSharp;

namespace AvaloniaMedia.FFmpeg.Render;

public class VulkanVideoRender : Control, IVideoRender
{
    private readonly VulkanDevice? _vkDevice;
    private GRContext? _grContext;
    private SKBitmap? _currentFrame;
    private WriteableBitmap? _renderTarget;
    private readonly object _frameLock = new();
    private string _subtitleText = string.Empty;

    public VulkanVideoRender()
    {
        try
        {
            _vkDevice = new VulkanDevice();
            if (_vkDevice.IsValid)
            {
                _grContext = CreateVulkanGrContext(_vkDevice);
                Console.WriteLine("[VulkanRender] GPU context created.");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[VulkanRender] Vulkan not available (CPU fallback): {ex.Message}");
        }
    }

    public unsafe void Draw(Frame frame)
    {
        if (frame.Width <= 0 || frame.Height <= 0)
            return;

        var info = new SKImageInfo(frame.Width, frame.Height, SKColorType.Rgba8888, SKAlphaType.Opaque);
        var bitmap = new SKBitmap(info);
        var dstPtr = (byte*)bitmap.GetPixels();
        var srcPtr = frame.DataPointer(0);
        var srcStride = frame.LineSize(0);
        var dstRowBytes = info.RowBytes;
        var copyRowBytes = Math.Min(dstRowBytes, srcStride);

        for (int y = 0; y < frame.Height; y++)
        {
            global::System.Buffer.MemoryCopy(
                srcPtr + y * (long)srcStride,
                dstPtr + y * (long)dstRowBytes,
                copyRowBytes, copyRowBytes);
        }

        var sub = frame.Subtitle;

        lock (_frameLock)
        {
            var old = _currentFrame;
            _currentFrame = bitmap;
            old?.Dispose();
            _subtitleText = sub ?? string.Empty;
        }

        Dispatcher.UIThread.Post(InvalidateVisual, DispatcherPriority.Render);
    }

    public override void Render(DrawingContext context)
    {
        context.FillRectangle(Brushes.Black, new Rect(Bounds.Size));

        SKBitmap? frame;
        string sub;
        lock (_frameLock)
        {
            frame = _currentFrame;
            sub = _subtitleText;
        }

        if (frame == null || frame.Width <= 0 || frame.Height <= 0)
            return;

        var controlW = Math.Max(1, (int)Bounds.Width);
        var controlH = Math.Max(1, (int)Bounds.Height);

        if (_renderTarget is null
            || _renderTarget.PixelSize.Width != controlW
            || _renderTarget.PixelSize.Height != controlH)
        {
            _renderTarget?.Dispose();
            _renderTarget = new WriteableBitmap(
                new PixelSize(controlW, controlH),
                new Vector(96, 96),
                PixelFormat.Rgba8888,
                AlphaFormat.Opaque);
        }

        using var buffer = _renderTarget.Lock();
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

        if (_grContext != null)
        {
            using var gpuImage = SKImage.FromBitmap(frame);
            canvas.DrawImage(gpuImage, destRect, paint);
        }
        else
        {
            canvas.DrawBitmap(frame, destRect, paint);
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
        context.DrawImage(_renderTarget, new Rect(0, 0, controlW, controlH));
    }

    private unsafe GRContext CreateVulkanGrContext(VulkanDevice vkDevice)
    {
        var vk = vkDevice.VkApi;

        var backend = new GRVkBackendContext
        {
            VkInstance = (IntPtr)(long)vkDevice.VkInstance.Handle,
            VkPhysicalDevice = (IntPtr)(long)vkDevice.VkPhysicalDevice.Handle,
            VkDevice = (IntPtr)(long)vkDevice.VkDevice.Handle,
            VkQueue = (IntPtr)(long)vkDevice.GraphicsQueue.Handle,
            GraphicsQueueIndex = vkDevice.GraphicsQueueFamilyIndex,
            GetProcedureAddress = (name, instance, device) =>
            {
                var nameBytes = System.Text.Encoding.UTF8.GetBytes(name);
                fixed (byte* pName = nameBytes)
                {
                    if (device != IntPtr.Zero)
                    {
                        return (IntPtr)(long)vk.GetDeviceProcAddr(
                            new Silk.NET.Vulkan.Device((nint)device), pName);
                    }
                    return (IntPtr)(long)vk.GetInstanceProcAddr(
                        new Silk.NET.Vulkan.Instance((nint)instance), pName);
                }
            }
        };

        return GRContext.CreateVulkan(backend);
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnDetachedFromVisualTree(e);

        lock (_frameLock)
        {
            _currentFrame?.Dispose();
            _currentFrame = null;
        }

        _renderTarget?.Dispose();
        _renderTarget = null;
        _grContext?.Dispose();
        _grContext = null;
        _vkDevice?.Dispose();
    }
}
