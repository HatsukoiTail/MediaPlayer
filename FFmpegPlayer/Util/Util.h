#ifndef UTIL_H
#define UTIL_H

#include <QPainterPath>

struct CornerRadius
{
    int topLeft = 0;
    int topRight = 0;
    int bottomRight = 0;
    int bottomLeft = 0;
};

QPainterPath drawRoundRectPath(const QRect& rect, const CornerRadius& radius);

QPainterPath drawRoundRectPath(const QRect& rect, int topLeft, int topRight, int bottomLeft, int bottomRight);

namespace ansi
{
    constexpr std::string_view RED = "\033[31m";
    constexpr std::string_view GREEN = "\033[32m";
    constexpr std::string_view YELLOW = "\033[33m";
    constexpr std::string_view BLUE = "\033[34m";
    constexpr std::string_view MAGENTA = "\033[35m";
    constexpr std::string_view CYAN = "\033[36m";
    constexpr std::string_view RESET = "\033[0m";
}

void print(const std::string &message, std::string_view color = ansi::RESET);

void openFileInExplorer(const QString& filePath);

#endif // UTIL_H
