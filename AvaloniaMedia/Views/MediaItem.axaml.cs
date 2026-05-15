using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using AvaloniaMedia.Message;
using AvaloniaMedia.ViewModels;
using CommunityToolkit.Mvvm.Messaging;

namespace AvaloniaMedia.Views;

public partial class MediaItem : UserControl
{
    public MediaItem()
    {
        InitializeComponent();
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnAttachedToVisualTree(e);
        if (DataContext is MediaItemViewModel vm)
            _ = vm.LoadAsync();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnDetachedFromVisualTree(e);
        if (DataContext is MediaItemViewModel vm)
            vm.Unload();
    }

    private void OnDoubleTapped(object? sender, TappedEventArgs e)
    {
        if (DataContext is MediaItemViewModel vm)
            WeakReferenceMessenger.Default.Send(new MediaShownMessage(vm.FilePath));
    }
}
