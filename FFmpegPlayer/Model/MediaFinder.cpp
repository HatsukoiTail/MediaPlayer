#include "MediaFinder.h"

#include <QDir>
#include <QStack>
#include <QtConcurrentRun>

MediaFinder::MediaFinder(QObject *parent)
    : QObject{parent}
{}

MediaFinder::~MediaFinder()
{
    this->stop();
}

void MediaFinder::find(const QString &path)
{
    assert(!this->is_running.load());
    this->future = QtConcurrent::run([this, path](){
        this->run(path);
    });
}

void MediaFinder::stop()
{
    this->is_running.store(false);
    if (this->future.isRunning())
        this->future.waitForFinished();
}

void MediaFinder::run(const QString &path)
{
    static std::vector<QString> suffixs { "mp4", "png", "jpg", "st" };
    emit this->started();
    this->is_running.store(true);
    QStack<QString> directories;
    directories.push(path);
    while (!directories.isEmpty())
    {
        QString current_dir = directories.pop();
        QDir dir(current_dir);
        QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const auto& entry : std::as_const(entries))
        {
            if (!this->is_running.load())
                return;
            if (entry.isDir())
            {
                directories.push(entry.absoluteFilePath());
                continue;
            }
            const auto file_path = entry.absoluteFilePath();
            const auto extend_name = QFileInfo(file_path).suffix();
            auto it = std::find(suffixs.begin(), suffixs.end(), extend_name);
            if (it == suffixs.end())
                continue;

            emit this->found(file_path);
        }
    }
    this->is_running.store(false);
    emit this->stopped();
}
