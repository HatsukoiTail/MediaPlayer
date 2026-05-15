#include "MediaDataLoader.h"

#include "MediaInfo.h"

#include <QtConcurrentRun>

MediaDataLoader::MediaDataLoader(QObject *parent)
    : QObject{parent}
{}

MediaDataLoader::~MediaDataLoader()
{
    this->close();
}

void MediaDataLoader::load(QWidget* widget, const QString& path)
{
    std::lock_guard<std::mutex> locker(this->mutex);
    this->tasks.emplace_back(widget, path);
    if (!this->is_running.load())
    {
        this->future = QtConcurrent::run(&MediaDataLoader::run, this);
    }
}

void MediaDataLoader::cancel(const QString &path)
{
    std::lock_guard<std::mutex> locker(this->mutex);
    auto it = std::find_if(this->tasks.begin(), this->tasks.end(), [&path](const std::pair<QWidget*, QString>& pair){
        return pair.second == path;
    });
    if (it != this->tasks.end())
        this->tasks.erase(it);
}

void MediaDataLoader::close()
{
    this->is_running.store(false);
    if (this->future.isRunning())
        this->future.waitForFinished();
}

void MediaDataLoader::run()
{
    this->is_running.store(true);
    while (true)
    {
        std::unique_lock<std::mutex> locker(this->mutex);
        if (!this->is_running.load())
            break;
        if (this->tasks.empty())
            break;
        std::pair<QWidget*, QString> task = this->tasks.front();
        this->tasks.pop_front();
        locker.unlock();

        auto data = std::make_shared<MediaData>(MediaInfo::mediaData(task.second));
        emit this->loaded(task.first, data);
    }
    this->is_running.store(false);
}
