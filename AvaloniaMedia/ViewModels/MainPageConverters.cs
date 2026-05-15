using System;
using System.Globalization;
using Avalonia.Data.Converters;

namespace AvaloniaMedia.ViewModels;

public static class MainPageConverters
{
    public static readonly IValueConverter IsLibrary =
        new MainPageConverter(MainPage.Library);
    public static readonly IValueConverter IsTranscoding =
        new MainPageConverter(MainPage.Transcoding);
    public static readonly IValueConverter IsPlayer =
        new MainPageConverter(MainPage.Player);
    public static readonly IValueConverter IsImageViewer =
        new MainPageConverter(MainPage.ImageViewer);
    public static readonly IValueConverter IsFullscreen =
        new MainPageConverter(MainPage.Player, MainPage.ImageViewer);
    public static readonly IValueConverter IsNotFullscreen =
        new InvertConverter(IsFullscreen);
}

internal class MainPageConverter(params MainPage[] pages) : IValueConverter
{
    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
        => value is MainPage p && Array.IndexOf(pages, p) >= 0;

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => throw new NotSupportedException();
}

internal class InvertConverter(IValueConverter inner) : IValueConverter
{
    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        var r = inner.Convert(value, targetType, parameter, culture);
        return r is bool b ? !b : r;
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => throw new NotSupportedException();
}
