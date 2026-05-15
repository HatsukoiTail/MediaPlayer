using System;
using System.Text;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using AvaloniaMedia.Services;
using AvaloniaMedia.ViewModels;
using AvaloniaMedia.Views;
using CommunityToolkit.Mvvm.DependencyInjection;
using Microsoft.Extensions.DependencyInjection;

namespace AvaloniaMedia;

public partial class App : Application
{
    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
        Console.OutputEncoding = Encoding.UTF8;
    }

    public override void OnFrameworkInitializationCompleted()
    {
        var window = new MainWindow();

        var collection = new ServiceCollection();

        collection.AddSingleton<ThumbnailCache>();
        collection.AddSingleton<MainWindowViewModel>();
        collection.AddSingleton<MediaLibraryViewModel>();
        collection.AddSingleton<MediaTranscodingViewModel>();
        collection.AddTransient<MediaPlayerViewModel>();
        collection.AddTransient<ImageViewerViewModel>();

        collection.AddSingleton((_) => TopLevel.GetTopLevel(window)!.StorageProvider);

        var serviceProvider = collection.BuildServiceProvider();
        Ioc.Default.ConfigureServices(serviceProvider);

        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            window.DataContext = Ioc.Default.GetRequiredService<MainWindowViewModel>();
            desktop.MainWindow = window;
        }

        base.OnFrameworkInitializationCompleted();
    }
}