using System.IO;
using System.Threading.Tasks;
using Avalonia.Media.Imaging;
using Avalonia.Platform.Storage;
using AvaloniaMedia.Helper;
using AvaloniaMedia.Message;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.DependencyInjection;
using CommunityToolkit.Mvvm.Input;
using CommunityToolkit.Mvvm.Messaging;

namespace AvaloniaMedia.ViewModels;

public enum MainPage { Library, Transcoding, Player, ImageViewer }

public partial class MainWindowViewModel : ViewModelBase
{
    [ObservableProperty]
    public partial ViewModelBase Page { get; set; } = Ioc.Default.GetRequiredService<MediaLibraryViewModel>();

    public MainWindowViewModel()
    {
        WeakReferenceMessenger.Default.
            Register<MainWindowViewModel, MediaShownMessage>(this, async (_, message) => await ShowMedia(message.Path));
        WeakReferenceMessenger.Default.
            Register<MainWindowViewModel, ImageViewerClosedMessage>(this, (_, _) => OnImageViewerClosed());
    }

    [RelayCommand]
    public void ShowLibrary() => Page = Ioc.Default.GetRequiredService<MediaLibraryViewModel>();

    [RelayCommand]
    public void ShowTranscoding() => Page = Ioc.Default.GetRequiredService<MediaTranscodingViewModel>();

    [RelayCommand]
    public async Task OpenFile()
    {
        // var storage = Ioc.Default.GetRequiredService<IStorageProvider>();
        // var result = await storage.OpenFilePickerAsync(new FilePickerOpenOptions
        // {
        //     Title = "Open media file",
        //     AllowMultiple = false
        // });
        // if (result.Count == 0) return;

        // var path = result[0].Path.LocalPath;
        // await ShowMedia(path);
        await ShowVideo("");
    }

    private async Task ShowMedia(string path)
    {
        if (MediaHelper.IsMedia(path) == false)
            return;

        if (MediaHelper.IsImage(path) == true)
        {
            ShowImage(path);
        }
        else
        {
            await ShowVideo(path);
        }
    }

    private void OnImageViewerClosed()
    {
        Page = Ioc.Default.GetRequiredService<MediaLibraryViewModel>();
    }

    private async Task ShowVideo(string path)
    {
        var player = Ioc.Default.GetRequiredService<MediaPlayerViewModel>();
        await player.OpenFile(path);
    }

    private void ShowImage(string path)
    {
        using var stream = File.OpenRead(path);
        var bitmap = new Bitmap(stream);
        var imageView = Ioc.Default.GetRequiredService<ImageViewerViewModel>();
        imageView.Image = bitmap;
        imageView.Title = Path.GetFileName(path);
        Page = imageView;
    }
}
