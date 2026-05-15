#ifndef MEDIADATALOADER_H
#define MEDIADATALOADER_H

#include "Model.h"

#include <QFuture>
#include <QObject>
#include <QThreadPool>

#include <deque>

class MediaDataLoader : public QObject
{
    Q_OBJECT
public:
    explicit MediaDataLoader(QObject *parent = nullptr);
    ~MediaDataLoader() override;
    void load(QWidget* widget, const QString& path);
    void cancel(const QString& path);
    void close();

signals:
    void loaded(QWidget* widget, std::shared_ptr<MediaData> data);

private:
    void run();

private:
    QThreadPool pool;
    QFuture<void> future;
    std::atomic<bool> is_running {false};
    std::mutex mutex;
    std::deque<std::pair<QWidget*, QString>> tasks;
};

#endif // MEDIADATALOADER_H
