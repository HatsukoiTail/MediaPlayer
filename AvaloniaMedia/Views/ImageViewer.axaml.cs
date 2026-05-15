using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Threading;

namespace AvaloniaMedia.Views;

public partial class ImageViewer : UserControl
{
    private const double MinScale = 1.0;
    private const double MaxScale = 20.0;

    private double scale = 1.0;
    private double panX, panY;
    private bool isDragging;
    private Point dragStart;
    private double dragStartPanX, dragStartPanY;
    private double fitWidth, fitHeight;       // displayed size after Uniform stretch
    private double fitX, fitY; // offset of fit image within container

    private double lastContainerWidth;
    private double lastContainerHeight;

    public ImageViewer()
    {
        InitializeComponent();
        ImageView.PropertyChanged += (_, e) =>
        {
            if (e.Property == Image.SourceProperty)
            {
                ResetView();
            }
        };
    }

    private void ResetView()
    {
        scale = 1.0;
        panX = 0;
        panY = 0;
        RecalculateFit();
        ApplyTransform();
    }

    private void RecalculateFit()
    {
        if (ImageView.Source is not Avalonia.Media.Imaging.Bitmap bitmap) return;

        double imageWidth = bitmap.Size.Width;
        double imageHeight = bitmap.Size.Height;
        if (imageWidth <= 0 || imageHeight <= 0) return;

        double containerWidth = ImageContainer.Bounds.Width;
        double containerHeight = ImageContainer.Bounds.Height;
        if (containerWidth <= 0 || containerHeight <= 0) return;

        double fitScale = Math.Min(containerWidth / imageWidth, containerHeight / imageHeight);
        fitWidth = imageWidth * fitScale;
        fitHeight = imageHeight * fitScale;
        fitX = (containerWidth - fitWidth) / 2;
        fitY = (containerHeight - fitHeight) / 2;

        if (scale < MinScale)
        {
            scale = MinScale;
            panX = 0;
            panY = 0;
        }
    }

    private void OnContainerSizeChanged(object? sender, SizeChangedEventArgs e)
    {
        double prevCw = lastContainerWidth;
        double prevCh = lastContainerHeight;
        lastContainerWidth = ImageContainer.Bounds.Width;
        lastContainerHeight = ImageContainer.Bounds.Height;

        RecalculateFit();

        // If the container grew large enough that the zoomed image now fits entirely,
        // animate smoothly back to the centered fit state (scale=1, pan=0).
        bool containerGrew = lastContainerWidth >= prevCw && lastContainerHeight >= prevCh;
        bool imageNowFits = fitWidth * scale <= lastContainerWidth
                         && fitHeight * scale <= lastContainerHeight;

        if (containerGrew && imageNowFits && scale > MinScale)
        {
            AnimateToFit();
        }
        else
        {
            SnapToBounds();
            ApplyTransform();
        }
    }

    private void OnPointerWheel(object? sender, PointerWheelEventArgs e)
    {
        if (fitWidth <= 0 || fitHeight <= 0) return;

        // Cursor position relative to the layout origin of the fit image
        var pos = e.GetPosition(ImageContainer);
        double cx = pos.X - fitX;
        double cy = pos.Y - fitY;

        double delta = e.Delta.Y > 0 ? 1.12 : 1.0 / 1.12;
        double newScale = Math.Clamp(scale * delta, MinScale, MaxScale);

        if (newScale == scale) return;

        if (newScale <= MinScale)
        {
            AnimateToFit();
            return;
        }

        // Keep cursor point fixed during zoom
        double ratio = newScale / scale;
        panX = cx - ratio * (cx - panX);
        panY = cy - ratio * (cy - panY);
        scale = newScale;

        ApplyTransform();
    }

    private void OnPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        var pt = e.GetCurrentPoint(ImageContainer);
        if (!pt.Properties.IsLeftButtonPressed) return;

        isDragging = true;
        dragStart = e.GetPosition(ImageContainer);
        dragStartPanX = panX;
        dragStartPanY = panY;
        ImageContainer.Cursor = new Cursor(StandardCursorType.SizeAll);
    }

    private void OnPointerMoved(object? sender, PointerEventArgs e)
    {
        if (!isDragging) return;

        var pos = e.GetPosition(ImageContainer);
        panX = dragStartPanX + pos.X - dragStart.X;
        panY = dragStartPanY + pos.Y - dragStart.Y;

        ApplyTransform();
    }

    private void OnPointerReleased(object? sender, PointerReleasedEventArgs e)
    {
        if (!isDragging) return;
        isDragging = false;
        ImageContainer.Cursor = Cursor.Default;

        SnapToBounds();
    }

    private void SnapToBounds()
    {
        double cw = ImageContainer.Bounds.Width;
        double ch = ImageContainer.Bounds.Height;
        double sw = fitWidth * scale;
        double sh = fitHeight * scale;

        double targetX, targetY;

        if (sw > cw)
        {
            double minX = cw - fitX - sw;
            double maxX = -fitX;
            targetX = Math.Clamp(panX, minX, maxX);
        }
        else
        {
            // Center in container: cw/2 = fitX + panX + sw/2
            targetX = cw / 2 - fitX - sw / 2;
        }

        if (sh > ch)
        {
            double minY = ch - fitY - sh;
            double maxY = -fitY;
            targetY = Math.Clamp(panY, minY, maxY);
        }
        else
        {
            targetY = ch / 2 - fitY - sh / 2;
        }

        if (Math.Abs(targetX - panX) > 0.5 || Math.Abs(targetY - panY) > 0.5)
        {
            AnimatePan(panX, panY, targetX, targetY);
        }
    }

    private void AnimatePan(double fromX, double fromY, double toX, double toY)
    {
        const int durationMs = 200;
        const int intervalMs = 16;
        int steps = durationMs / intervalMs;
        int step = 0;

        var timer = new DispatcherTimer(
            TimeSpan.FromMilliseconds(intervalMs),
            DispatcherPriority.Render,
            (s, e) =>
            {
                step++;
                double t = Math.Clamp((double)step / steps, 0, 1);
                double eased = 1 - Math.Pow(1 - t, 3);

                panX = fromX + (toX - fromX) * eased;
                panY = fromY + (toY - fromY) * eased;
                ApplyTransform();

                if (step >= steps)
                {
                    panX = toX;
                    panY = toY;
                    ApplyTransform();
                    ((DispatcherTimer)s!).Stop();
                }
            });
        timer.Start();
    }

    private void AnimateToFit()
    {
        double fromScale = scale;
        double fromPanX = panX;
        double fromPanY = panY;
        const double toScale = MinScale;
        const double toPanX = 0;
        const double toPanY = 0;

        const int durationMs = 250;
        const int intervalMs = 16;
        int steps = durationMs / intervalMs;
        int step = 0;

        var timer = new DispatcherTimer(
            TimeSpan.FromMilliseconds(intervalMs),
            DispatcherPriority.Render,
            (s, e) =>
            {
                step++;
                double t = Math.Clamp((double)step / steps, 0, 1);
                double eased = 1 - Math.Pow(1 - t, 3); // ease-out cubic

                scale = fromScale + (toScale - fromScale) * eased;
                panX = fromPanX + (toPanX - fromPanX) * eased;
                panY = fromPanY + (toPanY - fromPanY) * eased;
                ApplyTransform();

                if (step >= steps)
                {
                    scale = toScale;
                    panX = toPanX;
                    panY = toPanY;
                    ApplyTransform();
                    ((DispatcherTimer)s!).Stop();
                }
            });
        timer.Start();
    }

    private void ApplyTransform()
    {
        var scaleTrans = new ScaleTransform(scale, scale);
        var panTrans = new TranslateTransform(panX, panY);
        var group = new TransformGroup();
        group.Children.Add(scaleTrans);
        group.Children.Add(panTrans);
        ImageView.RenderTransform = group;
    }
}
