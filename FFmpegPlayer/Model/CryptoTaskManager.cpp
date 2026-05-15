#include "CryptoTaskManager.h"

#include "CryptoMethod.h"

#include <QFutureWatcher>
#include <QtConcurrentRun>

CryptoTaskManager::CryptoTaskManager(QObject *parent)
    : QObject{parent}
{}

CryptoTaskManager::~CryptoTaskManager()
{
    this->stop();
}

void CryptoTaskManager::encrypt(const QString &inputFile, const QString &outputFile)
{
    add_task(inputFile, outputFile, Type::Encrypt);
}

void CryptoTaskManager::decrypt(const QString &inputFile, const QString &outputFile)
{
    add_task(inputFile, outputFile, Type::Decrypt);
}

void CryptoTaskManager::cancel(const QString &inputFile)
{
    std::lock_guard<std::mutex> locker(this->mutex);
    auto it = std::find_if(this->tasks.begin(), this->tasks.end(), [inputFile](const Task& task){
        return inputFile == task.input;
    });
    if (it == this->tasks.end())
        return;
    it->cancel->store(true);
}

void CryptoTaskManager::stop()
{
    bool stopping = this->is_running.exchange(false);
    if (!stopping)
        return;
    std::lock_guard<std::mutex> locker(this->mutex);
    for (auto& task : this->tasks)
    {
        if (task.future.isRunning())
            task.future.waitForFinished();
    }
    this->tasks.clear();
}

void CryptoTaskManager::add_task(const QString& input, const QString& output, Type type)
{
    QFutureWatcher<void>* watcher = new QFutureWatcher<void>();
    watcher->setParent(this);
    auto cancel_token = std::make_shared<std::atomic_bool>(false);

    QFuture<void> future = QtConcurrent::run([this, input, output, type, cancel_token](){
        auto update_progress = [this, input](size_t cur, size_t total){
            QMetaObject::invokeMethod(this, [this, input, cur, total](){
                emit this->progressChanged(input, cur, total);
            }, Qt::QueuedConnection);
        };
        CryptoResult res = CryptoResult::Failure;
        if (type == Type::Encrypt)
            res = ::encrypt(input.toStdString(), output.toStdString(), update_progress, cancel_token);
        else
            res = ::decrypt(input.toStdString(), output.toStdString(), update_progress, cancel_token);

        QMetaObject::invokeMethod(this, [this, input, res](){
            this->on_finish(input, res);
        }, Qt::QueuedConnection);
    });

    connect(watcher, &QFutureWatcher<void>::finished, this, [watcher](){
        watcher->deleteLater();
    });
    watcher->setFuture(future);

    Task task;
    task.input = input;
    task.output = output;
    task.type = type;
    task.cancel = cancel_token;
    task.future = future;
    task.watcher = watcher;
    std::lock_guard<std::mutex> locker(this->mutex);
    this->tasks.emplace_back(std::move(task));
}

void CryptoTaskManager::on_finish(const QString &input, CryptoResult res)
{
    QString path;
    {
        std::lock_guard<std::mutex> locker(this->mutex);
        auto it = std::find_if(this->tasks.begin(), this->tasks.end(), [&input](const Task& task){ return input == task.input; });
        if (it != this->tasks.end())
        {
            path = it->input;
            this->tasks.erase(it);
        }
    }
    if (!path.isEmpty())
        emit this->taskFinished(path, res);
}
