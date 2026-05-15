#ifndef CRYPTOTASKMANAGER_H
#define CRYPTOTASKMANAGER_H

#include "CryptoMethod.h"

#include <QFuture>
#include <QObject>

#include <deque>

class CryptoTaskManager : public QObject
{
    Q_OBJECT
public:
    explicit CryptoTaskManager(QObject *parent = nullptr);
    ~CryptoTaskManager();
    void encrypt(const QString& inputFile, const QString& outputFile);
    void decrypt(const QString& inputFile, const QString& outputFile);
    void cancel(const QString& inputFile);
    void stop();

signals:
    void progressChanged(const QString& path, size_t cur, size_t total);
    void taskFinished(const QString& path, CryptoResult result);

private:
    enum class Type { Encrypt, Decrypt };
    struct Task;
    void add_task(const QString& input, const QString& output, Type type);
    void on_finish(const QString& input, CryptoResult res);
    void run();

private:
    QFuture<void> future;
    std::mutex mutex;
    std::deque<Task> tasks;
    std::atomic<bool> is_running {false};
};

struct CryptoTaskManager::Task
{
    Type type;
    QString input;
    QString output;
    std::shared_ptr<std::atomic<bool>> cancel;
    QFuture<void> future;
    QFutureWatcher<void>* watcher;
};

#endif // CRYPTOTASKMANAGER_H
