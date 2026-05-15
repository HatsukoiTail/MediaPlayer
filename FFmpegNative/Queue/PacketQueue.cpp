#include "PacketQueue.h"

PacketQueue::PacketQueue()
    : max_queue_size(5 * 1024 * 1024)
{
}

PacketQueue::PacketQueue(int max_size)
    : max_queue_size(max_size)
{
}

PacketQueue::~PacketQueue()
{
    std::lock_guard<std::mutex> locker(this->mutex);
    this->stop_request = true;
    this->queue.clear();
    this->cv.notify_all();
}

bool PacketQueue::enqueue(Packet packet)
{
    std::unique_lock<std::mutex> locker(this->mutex);

    this->cv.wait(locker, [this]
                  { return this->queue_size < this->max_queue_size || this->stop_request; });
    if (this->stop_request)
        return false;

    packet.set_serial(this->queue_serial);

    this->queue_size += packet.size();
    this->queue_duration += packet.duration();
    this->queue.emplace_back(std::move(packet));

    this->cv.notify_one();
    return true;
}

bool PacketQueue::try_enqueue(Packet packet)
{
    std::lock_guard<std::mutex> locker(this->mutex);

    if (this->stop_request || this->queue_size >= this->max_queue_size)
        return false;

    packet.set_serial(this->queue_serial);

    this->queue_size += packet.size();
    this->queue_duration += packet.duration();
    this->queue.emplace_back(std::move(packet));

    this->cv.notify_one();
    return true;
}

Packet PacketQueue::dequeue()
{
    std::unique_lock<std::mutex> locker(this->mutex);

    this->cv.wait(locker, [this]
                  { return !this->queue.empty() || this->stop_request; });
    if (this->queue.empty())
        return {};

    auto packet = std::move(this->queue.front());
    this->queue.pop_front();

    this->queue_size -= packet.size();
    this->queue_duration -= packet.duration();

    this->cv.notify_one();
    return packet;
}

bool PacketQueue::try_dequeue(Packet &packet)
{
    std::unique_lock<std::mutex> locker(this->mutex);

    if (this->queue.empty())
        return false;

    packet = std::move(this->queue.front());
    this->queue.pop_front();

    this->queue_size -= packet.size();
    this->queue_duration -= packet.duration();

    return true;
}

void PacketQueue::flush()
{
    std::lock_guard<std::mutex> locker(this->mutex);

    this->queue.clear();
    this->queue_size = 0;
    this->queue_duration = 0;
    this->queue_serial++;
}

void PacketQueue::start()
{
    std::lock_guard<std::mutex> locker(this->mutex);
    this->stop_request = false;
    this->queue_serial++;
}

void PacketQueue::stop()
{
    std::lock_guard<std::mutex> locker(this->mutex);
    this->stop_request = true;
    this->cv.notify_all();
}

bool PacketQueue::is_stopped() const
{
    return this->stop_request;
}

int PacketQueue::serial() const
{
    return this->queue_serial;
}

int PacketQueue::count() const
{
    std::lock_guard<std::mutex> locker(this->mutex);
    return static_cast<int>(this->queue.size());
}

int PacketQueue::size() const
{
    std::lock_guard<std::mutex> locker(this->mutex);
    return this->queue_size;
}

int64_t PacketQueue::duration() const
{
    std::lock_guard<std::mutex> locker(this->mutex);
    return this->queue_duration;
}

int PacketQueue::max_size() const
{
    return this->max_queue_size;
}
