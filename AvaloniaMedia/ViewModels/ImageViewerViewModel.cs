

using System;
using Avalonia.Media.Imaging;
using AvaloniaMedia.Message;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using CommunityToolkit.Mvvm.Messaging;

namespace AvaloniaMedia.ViewModels;

public partial class ImageViewerViewModel : ViewModelBase
{
    [ObservableProperty]
    public partial Bitmap? Image { get; set; }
    [ObservableProperty]
    public partial string Title { get; set; }

    [RelayCommand]
    public void Close()
    {
        WeakReferenceMessenger.Default.Send(new ImageViewerClosedMessage());
    }
}