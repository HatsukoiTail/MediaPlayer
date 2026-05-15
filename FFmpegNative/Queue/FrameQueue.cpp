#include "FrameQueue.h"

#include <cassert>

FrameQueue::FrameQueue(int max_count)
    : max_count(max_count)
{
}

FrameQueue::~FrameQueue()
{
    std::lock_guard<std::mutex> locker(this->mutex);
    this->stop_request = true;
    this->queue.clear();
    this->condition.notify_all();
}

bool FrameQueue::enqueue(Frame frame)
{
    std::unique_lock<std::mutex> locker(this->mutex);
    this->condition.wait(locker, [this]
                         { return this->queue.size() < this->max_count || this->stop_request; });
    if (this->stop_request)
        return false;
    this->queue.push_back(std::move(frame));
    this->condition.notify_one();
    return true;
}

bool FrameQueue::try_enqueue(Frame frame)
{
    std::lock_guard<std::mutex> locker(this->mutex);
    if (this->queue.size() >= this->max_count || this->stop_request)
        return false;
    this->queue.push_back(std::move(frame));
    this->condition.notify_one();
    return true;
}

Frame FrameQueue::peek()
{
    std::lock_guard<std::mutex> locker(this->mutex);
    if (this->queue.empty())
        return Frame();
    return Frame::ref(this->queue.front());
}

Frame FrameQueue::peek_next()
{
    std::lock_guard<std::mutex> locker(this->mutex);
    if (this->queue.size() < 2)
        return Frame();
    return Frame::ref(this->queue[1]);
}

Frame FrameQueue::dequeue()
{
    std::unique_lock<std::mutex> locker(this->mutex);

    this->condition.wait(locker, [this]
                         { return !this->queue.empty() || this->stop_request; });
    if (this->queue.empty())
        return Frame();

    auto result = std::move(this->queue.front());
    this->queue.pop_front();
    this->condition.notify_one();
    return result;
}

Frame FrameQueue::try_dequeue()
{
    std::lock_guard<std::mutex> locker(this->mutex);
    if (this->queue.empty())
        return Frame();

    auto result = std::move(this->queue.front());
    this->queue.pop_front();
    this->condition.notify_one();
    return result;
}

void FrameQueue::flush()
{
    std::lock_guard<std::mutex> locker(this->mutex);
    this->queue.clear();
    this->condition.notify_all();
}

void FrameQueue::start()
{
    std::lock_guard<std::mutex> locker(this->mutex);
    this->stop_request = false;
}

void FrameQueue::stop()
{
    std::lock_guard<std::mutex> locker(this->mutex);
    this->stop_request = true;
    this->condition.notify_all();
}

void FrameQueue::awake()
{
    std::lock_guard<std::mutex> locker(this->mutex);
    this->condition.notify_all();
}

size_t FrameQueue::size() const
{
    std::lock_guard<std::mutex> locker(this->mutex);
    return this->queue.size();
}
