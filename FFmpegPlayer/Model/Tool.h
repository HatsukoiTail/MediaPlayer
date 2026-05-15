#ifndef TOOL_H
#define TOOL_H

#include <QString>

QString formatTime(int64_t msec);

QString formatSize(size_t size);

QString formatPath(const QString& path);

QString formatFileName(const QString& path);

QString combinePath(const QString& dir, const QString& name);

#endif // TOOL_H
