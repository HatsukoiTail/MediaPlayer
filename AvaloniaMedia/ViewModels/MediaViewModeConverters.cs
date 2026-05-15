using System;
using System.Globalization;
using Avalonia.Data.Converters;

namespace AvaloniaMedia.ViewModels;

public static class MediaViewModeConverters
{
    public static readonly IValueConverter IsLarge = new MediaViewModeConverter(MediaViewMode.LargeIcons);
    public static readonly IValueConverter IsMedium = new MediaViewModeConverter(MediaViewMode.MediumIcons);
    public static readonly IValueConverter IsList = new MediaViewModeConverter(MediaViewMode.List);
}

internal class MediaViewModeConverter(MediaViewMode mode) : IValueConverter
{
    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
        => value is MediaViewMode m && m == mode;

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => throw new NotSupportedException();
}
