#ifndef MEDIAFINDER_H
#define MEDIAFINDER_H

#include <QFuture>
#include <QObject>

class MediaFinder : public QObject
{
    Q_OBJECT
public:
    explicit MediaFinder(QObject *parent = nullptr);
    ~MediaFinder();
    void find(const QString& path);
    void stop();

signals:
    void found(const QString& path);
    void started();
    void stopped();

private:
    void run(const QString& path);

private:
    std::atomic<bool> is_running {false};
    QFuture<void> future;
};

#endif // MEDIAFINDER_H
