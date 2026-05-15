#include "util.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include <cassert>
#include <iostream>
#include <mutex>

QPainterPath drawRoundRectPath(const QRect& rect, const CornerRadius& radius)
{
    QPainterPath path;

    const double x = rect.x();
    const double y = rect.y();
    const double w = rect.width();
    const double h = rect.height();

    const double tl = radius.topLeft;
    const double tr = radius.topRight;
    const double br = radius.bottomRight;
    const double bl = radius.bottomLeft;

    path.moveTo(x + tl, y);

    // top edge + top-right corner
    path.lineTo(x + w - tr, y);
    if (tr > 0)
        path.quadTo(x + w, y, x + w, y + tr);

    // right edge + bottom-right corner
    path.lineTo(x + w, y + h - br);
    if (br > 0)
        path.quadTo(x + w, y + h, x + w - br, y + h);

    // bottom edge + bottom-left corner
    path.lineTo(x + bl, y + h);
    if (bl > 0)
        path.quadTo(x, y + h, x, y + h - bl);

    // left edge + top-left corner
    path.lineTo(x, y + tl);
    if (tl > 0)
        path.quadTo(x, y, x + tl, y);

    path.closeSubpath();
    return path;
}

void print(const std::string &message, std::string_view color)
{
    static std::once_flag flag;
    std::call_once(flag, []()
                   { std::cout.setf(std::ios::unitbuf); });
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    std::cout << color << message << ansi::RESET << std::endl;
}

void openFileInExplorer(const QString &filePath)
{
    assert(QFileInfo::exists(filePath) && QFileInfo(filePath).isFile());

    QString nativePath = QDir::toNativeSeparators(filePath);

    QString command = "explorer";
    QStringList args;
    args << "/select," << nativePath;

    QProcess::startDetached(command, args);
}

QPainterPath drawRoundRectPath(const QRect &rect, int topLeft, int topRight, int bottomLeft, int bottomRight)
{
    QPainterPath path;

    const double x = rect.x();
    const double y = rect.y();
    const double w = rect.width();
    const double h = rect.height();

    const double tl = topLeft;
    const double tr = topRight;
    const double br = bottomRight;
    const double bl = bottomLeft;

    path.moveTo(x + tl, y);

    path.lineTo(x + w - tr, y);
    if (tr > 0)
        path.quadTo(x + w, y, x + w, y + tr);

    path.lineTo(x + w, y + h - br);
    if (br > 0)
        path.quadTo(x + w, y + h, x + w - br, y + h);

    path.lineTo(x + bl, y + h);
    if (bl > 0)
        path.quadTo(x, y + h, x, y + h - bl);

    path.lineTo(x, y + tl);
    if (tl > 0)
        path.quadTo(x, y, x + tl, y);

    path.closeSubpath();
    return path;
}
