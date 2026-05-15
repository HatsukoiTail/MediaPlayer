using Avalonia.Data.Converters;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

namespace AvaloniaMedia.Converters;

public class InverseTypeVisibilityConverter : IValueConverter
{
    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is null) 
            return true; // 如果没页面，默认显示导航栏

        // 1. 获取当前 Page 的类型名称
        string currentTypeName = value.GetType().Name;

        // 2. 解析参数（如果没有传参数，默认可见）
        if (parameter is not string hiddenTypesStr || string.IsNullOrWhiteSpace(hiddenTypesStr))
            return true;

        // 3. 分割字符串，得到“黑名单”列表
        // 支持格式: "TypeA, TypeB" 或 "TypeA|TypeB"
        var hiddenTypes = hiddenTypesStr.Split([',', '|'], StringSplitOptions.RemoveEmptyEntries)
                                        .Select(t => t.Trim())
                                        .ToList();

        // 4. 判断：如果当前类型在黑名单中，则隐藏 (false)；否则显示 (true)
        bool shouldBeHidden = hiddenTypes.Contains(currentTypeName);

        return !shouldBeHidden;
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}