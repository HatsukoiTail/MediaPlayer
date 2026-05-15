#ifndef CRYPTOTASKMANAGER_H
#define CRYPTOTASKMANAGER_H

#include "CryptoMethod.h"

#include <QObject>

#include <memory>
#include <thread>

enum class CryptoType
{
    Encrypt,
    Decrypt
};

struct CryptoTask
{
public:
    enum class State
    {
        Waitting,
        Processing,
        Cancelled,
        Done
    };
public:
    CryptoTask(CryptoType type, const QString& inputPath, const QString& outputPath);
    CryptoType type() const;
    QString inputPath() const;
    QString outputPath() const;
    State state() const;
    const std::atomic<bool>& cancelToken() const;
    void setState(State state);
    void cancel();

private:
    State task_state;
    CryptoType task_type;
    QString input_path;
    QString output_path;
    std::atomic<bool> cancel_token;
};

class CryptoTaskManager : public QObject
{
    Q_OBJECT
public:
    static CryptoTaskManager& instance();
    explicit CryptoTaskManager(QObject *parent = nullptr);
    ~CryptoTaskManager();

public:
    void start();
    void stop();
    void clear();

    bool isRuning() const;

signals:
    void taskFinished(const QString& filePath, CryptoResult result);
    void taskProgress(const QString& filePath, size_t current, size_t total);
    void allTaskFinished();

public slots:
    std::shared_ptr<CryptoTask> addEncryptTask(const QString& inputPath, const QString& outputPath);
    std::shared_ptr<CryptoTask> addDecryptTask(const QString& inputPath, const QString& outputPath);

private:
    void work();

private:
    std::vector<std::shared_ptr<CryptoTask>> tasks;
    std::thread worker_thread;
    std::atomic<bool> running;
    std::mutex mutex;
};

#endif // CRYPTOTASKMANAGER_H
