using System;
using System.Diagnostics;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using AvaloniaMedia.FFmpeg.Render;
using AvaloniaMedia.ViewModels;

namespace AvaloniaMedia.Views;

public partial class MediaPlayer : UserControl
{
    private double lastVolume = 80;

    public event Action? BackRequested;

    public MediaPlayer()
    {
        InitializeComponent();
    }

    public void RequestBack()
    {
        BackRequested?.Invoke();
    }

    protected override void OnDataContextChanged(EventArgs e)
    {
        base.OnDataContextChanged(e);
        if (DataContext is MediaPlayerViewModel vm && VideoControl != null)
        {
            // vm.MediaContext.SetVideoView(VideoControl);
            vm.MediaContext.VideoControl = VideoControl;
        }
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnAttachedToVisualTree(e);
        ProgressSlider.AddHandler(PointerPressedEvent, OnProgressSliderPressed,
            RoutingStrategies.Tunnel, true);
        ProgressSlider.AddHandler(PointerReleasedEvent, OnProgressSliderReleased,
            RoutingStrategies.Tunnel, true);

        if (DataContext is MediaPlayerViewModel vm && VideoControl != null)
        {
            vm.MediaContext.VideoControl = VideoControl;
        }
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnDetachedFromVisualTree(e);
        ProgressSlider.RemoveHandler(PointerPressedEvent, OnProgressSliderPressed);
        ProgressSlider.RemoveHandler(PointerReleasedEvent, OnProgressSliderReleased);

        if (DataContext is MediaPlayerViewModel vm)
        {
            // Task _ = vm.CloseAsync();
            vm.Close();
        }
    }

    #region 播放列表
    private DispatcherTimer? eposidesPopupCloseTimer;

    private void EposidesButtonEntered(object? sender, PointerEventArgs args)
    {
        eposidesPopupCloseTimer?.Stop();

        if (VolumePopup.IsOpen == true)
        {
            VolumePopup.IsOpen = false;
        }
        if (SpeedPopup.IsOpen)
        {
            SpeedPopup.IsOpen = false;
        }

        EposidesPopup.IsOpen = true;
    }

    private void EposidesButtonExited(object? sender, PointerEventArgs args)
    {
        if (eposidesPopupCloseTimer == null)
        {
            eposidesPopupCloseTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromMilliseconds(500)
            };
            eposidesPopupCloseTimer.Tick += (_, _) =>
            {
                eposidesPopupCloseTimer.Stop();
                if (EposidesPopup.IsOpen == true && EposidesButton.IsPointerOver == false && EposidesPopupContent.IsPointerOver == false)
                {
                    EposidesPopup.IsOpen = false;
                    eposidesPopupCloseTimer = null;
                }
            };
        }
        eposidesPopupCloseTimer.Start();
    }

    private void EposidesPopupEntered(object? sender, PointerEventArgs args)
    {
        eposidesPopupCloseTimer?.Stop();
    }

    private void EposidesPopupExited(object? sender, PointerEventArgs args)
    {
        eposidesPopupCloseTimer?.Start();
    }
    #endregion


    #region 音量

    private DispatcherTimer? volumePopupCloseTimer;

    private void VolumeButtonEntered(object? sender, PointerEventArgs args)
    {
        volumePopupCloseTimer?.Stop();

        if (EposidesPopup.IsOpen == true)
        {
            EposidesPopup.IsOpen = false;
        }
        if (SpeedPopup.IsOpen)
        {
            SpeedPopup.IsOpen = false;
        }

        VolumePopup.IsOpen = true;
    }

    private void VolumeButtonExited(object? sender, PointerEventArgs args)
    {
        if (volumePopupCloseTimer == null)
        {
            volumePopupCloseTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromMilliseconds(500)
            };
            volumePopupCloseTimer.Tick += (_, _) =>
            {
                volumePopupCloseTimer.Stop();
                if (VolumePopup.IsOpen == true && VolumeButton.IsPointerOver == false && VolumePopupContent.IsPointerOver == false)
                {
                    VolumePopup.IsOpen = false;
                    volumePopupCloseTimer = null;
                }
            };
        }
        volumePopupCloseTimer.Start();
    }

    private void VolumePopupEntered(object? sender, PointerEventArgs args)
    {
        volumePopupCloseTimer?.Stop();
    }

    private void VolumePopupExited(object? sender, PointerEventArgs args)
    {
        volumePopupCloseTimer?.Start();
    }

    #endregion

    #region 倍速

    private DispatcherTimer? speedPopupCloseTimer;

    private void SpeedButtonEntered(object? sender, PointerEventArgs args)
    {
        speedPopupCloseTimer?.Stop();

        if (EposidesPopup.IsOpen == true)
        {
            EposidesPopup.IsOpen = false;
        }
        if (VolumePopup.IsOpen == true)
        {
            VolumePopup.IsOpen = false;
        }

        SpeedPopup.IsOpen = true;
    }

    private void SpeedButtonExited(object? sender, PointerEventArgs args)
    {
        if (speedPopupCloseTimer == null)
        {
            speedPopupCloseTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromMilliseconds(300)
            };
            speedPopupCloseTimer.Tick += (_, _) =>
            {
                speedPopupCloseTimer.Stop();
                if (SpeedPopup.IsOpen == true && SpeedButton.IsPointerOver == false && SpeedPopupContent.IsPointerOver == false)
                {
                    SpeedPopup.IsOpen = false;
                    speedPopupCloseTimer = null;
                }
            };
        }
        speedPopupCloseTimer.Start();
    }

    private void SpeedPopupEntered(object? sender, PointerEventArgs args)
    {
        speedPopupCloseTimer?.Stop();
    }

    private void SpeedPopupExited(object? sender, PointerEventArgs args)
    {
        speedPopupCloseTimer?.Start();
    }

    #endregion

    protected override void OnKeyDown(KeyEventArgs e)
    {
        base.OnKeyDown(e);
        if (DataContext is not MediaPlayerViewModel vm)
            return;

        switch (e.Key)
        {
            case Key.Space:
                e.Handled = true;
                vm.TogglePlayPauseCommand.Execute(null);
                break;
            case Key.Left:
                e.Handled = true;
                vm.Position = Math.Max(0, vm.Position - 5);
                vm.SeekCommand.Execute(null);
                break;
            case Key.Right:
                e.Handled = true;
                vm.Position = Math.Min(vm.Duration, vm.Position + 5);
                vm.SeekCommand.Execute(null);
                break;
            case Key.PageDown:
                e.Handled = true;
                vm.StepNextCommand.Execute(null);
                break;
            case Key.Up:
                e.Handled = true;
                vm.Speed += 0.5;
                break;
            case Key.Down:
                e.Handled = true;
                vm.Speed -= 0.5;
                break;
        }
    }

    private void OnProgressSliderPressed(object? sender, PointerPressedEventArgs e)
    {
        if (DataContext is MediaPlayerViewModel vm)
        {
            vm.IsSeeking = true;
        }
    }

    private void OnProgressSliderReleased(object? sender, PointerReleasedEventArgs e)
    {
        if (DataContext is MediaPlayerViewModel vm)
        {
            vm.SeekCommand.Execute(null);
        }
    }

    private void OnVolumeToggle(object? sender, RoutedEventArgs e)
    {
        if (DataContext is not MediaPlayerViewModel vm)
            return;

        if (vm.Volume > 0)
        {
            lastVolume = vm.Volume;
            vm.Volume = 0;
        }
        else
        {
            vm.Volume = lastVolume;
        }
    }

    #region 全屏

    public static readonly StyledProperty<ICommand?> ToggleFullscreenCommandProperty
        = AvaloniaProperty.Register<MediaPlayer, ICommand?>(nameof(ToggleFullscreenCommand));

    public ICommand? ToggleFullscreenCommand { get; set; }

    private void OnToggleFullscreen(object? sender, RoutedEventArgs e)
    {
        if (ToggleFullscreenCommand?.CanExecute(null) == true)
        {
            ToggleFullscreenCommand.Execute(null);
        }
    }

    #endregion
}
