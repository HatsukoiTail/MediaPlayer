#include "Tool.h"

#include <QDir>
#include <QFileInfo>
#include <QList>

QString formatTime(int64_t msec)
{
    qint64 totalSeconds = msec / 1000;

    // 计算小时、分钟、秒
    qint64 hours = totalSeconds / 3600;
    qint64 remainingSeconds = totalSeconds % 3600;
    qint64 minutes = remainingSeconds / 60;
    qint64 seconds = remainingSeconds % 60;

    // 根据小时是否为0选择格式
    QString timeText;
    if (hours > 0)
    {
        timeText = QString("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))  // 小时补零到2位
            .arg(minutes, 2, 10, QLatin1Char('0')) // 分钟补零到2位
            .arg(seconds, 2, 10, QLatin1Char('0')); // 秒补零到2位
    }
    else
    {
        timeText = QString("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0')) // 分钟补零到2位
            .arg(seconds, 2, 10, QLatin1Char('0')); // 秒补零到2位
    }
    return timeText;
}

QString formatSize(size_t size)
{
    static const QList<QPair<qint64, QString>> units = {
        {qint64(1024 * 1024 * 1024), "GB"},
        {qint64(1024 * 1024), "MB"},
        {qint64(1024), "KB"},
        {qint64(1), "B"}
    };
    QString sizeText;
    for (const auto& pair : units)
    {
        if (size < pair.first) continue;
        qreal value = static_cast<qreal>(size) / static_cast<qreal>(pair.first);
        value = qRound(value * 10.0) / 10.0;
        sizeText = QString::number(value) + pair.second;
        break;
    }
    return sizeText;
}

QString formatFileName(const QString &path)
{
    return QFileInfo(path).fileName();
}

QString formatPath(const QString &path)
{
    QFileInfo info(path);
    return info.absoluteDir().absolutePath();
}

QString combinePath(const QString &dir, const QString &name)
{
    return dir + '/' + name;
}
