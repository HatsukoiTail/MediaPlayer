#include "CryptoTaskManager.h"

CryptoTask::CryptoTask(CryptoType type, const QString &inputPath, const QString &outputPath)
    : task_state(State::Waitting), task_type(type), input_path(inputPath), output_path(outputPath), cancel_token(false)
{}

CryptoType CryptoTask::type() const
{
    return this->task_type;
}

QString CryptoTask::inputPath() const
{
    return this->input_path;
}

QString CryptoTask::outputPath() const
{
    return this->output_path;
}

const std::atomic<bool> &CryptoTask::cancelToken() const
{
    return this->cancel_token;
}

void CryptoTask::setState(State state)
{
    this->task_state = state;
}

void CryptoTask::cancel()
{
    this->cancel_token.store(false);
}

CryptoTask::State CryptoTask::state() const
{
    return this->task_state;
}

CryptoTaskManager &CryptoTaskManager::instance()
{
    static CryptoTaskManager manager;
    return manager;
}

CryptoTaskManager::CryptoTaskManager(QObject *parent)
    : QObject {parent}
{

}

CryptoTaskManager::~CryptoTaskManager()
{
    this->stop();
}

void CryptoTaskManager::start()
{
    if (this->running.load())
        return;
    this->running.store(true);
    this->worker_thread = std::thread(&CryptoTaskManager::work, this);
}

void CryptoTaskManager::stop()
{
    this->running.store(false);
    if (this->worker_thread.joinable())
        this->worker_thread.join();
    std::lock_guard<std::mutex> locker(this->mutex);
    for (auto& task : this->tasks)
        task->cancel();
}

void CryptoTaskManager::clear()
{
    assert(!this->running.load());
    std::lock_guard<std::mutex> locker(this->mutex);
    this->tasks.clear();
}

bool CryptoTaskManager::isRuning() const
{
    return this->running.load();
}

std::shared_ptr<CryptoTask> CryptoTaskManager::addEncryptTask(const QString &inputPath, const QString &outputPath)
{
    std::lock_guard<std::mutex> locker(this->mutex);
    auto task = std::make_shared<CryptoTask>(CryptoType::Encrypt, inputPath, outputPath);
    this->tasks.push_back(task);
    if (!this->running.load())
        this->start();
    return task;
}

std::shared_ptr<CryptoTask> CryptoTaskManager::addDecryptTask(const QString &inputPath, const QString &outputPath)
{
    std::lock_guard<std::mutex> locker(this->mutex);
    auto task = std::make_shared<CryptoTask>(CryptoType::Decrypt, inputPath, outputPath);
    this->tasks.push_back(task);
    if (!this->running.load())
        this->start();
    return task;
}

void CryptoTaskManager::work()
{
    while (this->running.load())
    {
        std::shared_ptr<CryptoTask> task;
        {
            std::lock_guard<std::mutex> locker(this->mutex);
            if (this->tasks.empty() || !this->running.load())
                return;
            task = this->tasks.front();
            this->tasks.erase(this->tasks.begin());
        }
        if (!task)
            continue;

        auto progress_callback = [this, task](size_t current, size_t total){
            emit this->taskProgress(task->inputPath(), current, total);
        };

        CryptoResult result;
        if (task->type() == CryptoType::Encrypt)
            result = encrypt(task->inputPath().toStdString(), task->outputPath().toUtf8().toStdString(), progress_callback, task->cancelToken());
        else
            result = decrypt(task->inputPath().toStdString(), task->outputPath().toUtf8().toStdString(), progress_callback, task->cancelToken());

        emit taskFinished(task->inputPath(), result);
    }

    std::lock_guard<std::mutex> locker(this->mutex);
    if (this->tasks.empty())
        emit this->allTaskFinished();
}
