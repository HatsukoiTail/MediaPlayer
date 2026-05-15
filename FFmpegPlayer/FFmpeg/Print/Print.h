#ifndef PRINT_H
#define PRINT_H

#include <format>
#include <iostream>
#include <mutex>

enum class Ansi
{
    Reset = 0,
    Black = 30,
    Red = 31,
    Green = 32,
    Yellow = 33,
    Blue = 34,
    Magenta = 35,
    Cyan = 36,
    White = 37,
    BrightBlack = 90,
    BrightRed = 91,
    BrightGreen = 92,
    BrightYellow = 93,
    BrightBlue = 94,
    BrightMagenta = 95,
    BrightCyan = 96,
    BrightWhite = 97,

    // 背景色
    BgBlack = 40,
    BgRed = 41,
    BgGreen = 42,
    BgYellow = 43,
    BgBlue = 44,
    BgMagenta = 45,
    BgCyan = 46,
    BgWhite = 47,
    BgBrightBlack = 100,
    BgBrightRed = 101,
    BgBrightGreen = 102,
    BgBrightYellow = 103,
    BgBrightBlue = 104,
    BgBrightMagenta = 105,
    BgBrightCyan = 106,
    BgBrightWhite = 107,

    // 样式
    Bold = 1,
    Dim = 2,
    Italic = 3,
    Underline = 4,
    Blink = 5,
    Reverse = 7,
    Hidden = 8,
    Strikethrough = 9
};

inline std::mutex& print_mutex()
{
    static std::mutex mutex;
    return mutex;
};

inline std::string to_ansi(Ansi color)
{
    return std::format("\033[{}m", static_cast<int>(color));
};

template <typename... FormatArgs>
void print(std::format_string<FormatArgs...> fmt, FormatArgs&&... args)
{
    std::lock_guard<std::mutex> locker(print_mutex());
    std::cout << std::vformat(fmt.get(), std::make_format_args(args...)) << std::endl;
}

template <typename... FormatArgs>
void print(Ansi color, std::format_string<FormatArgs...> fmt, FormatArgs&&... args)
{
    std::lock_guard<std::mutex> locker(print_mutex());
    std::cout << to_ansi(color);
    std::cout << std::vformat(fmt.get(), std::make_format_args(args...));
    std::cout<< to_ansi(Ansi::Reset);
    std::cout << std::endl;
}

#endif // PRINT_H
