#ifndef DATA_MODEL_H
#define DATA_MODEL_H

#include "Print.h"

#include <cassert>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

// 环形缓冲区
class RingBuffer
{
public:
    explicit RingBuffer(size_t capacity);
    size_t write(const uint8_t *data, size_t size);
    size_t read(uint8_t *data, size_t size);
    size_t available() const;
    size_t space() const;

private:
    std::vector<uint8_t> buffer;
    size_t head = 0;
    size_t tail = 0;
    size_t size = 0; // 缓冲区实际使用的大小
};


// 线程安全的队列
template <class T>
class Queue
{
public:
    Queue(std::size_t max_size = 0) : max_size(max_size) {}
    ~Queue(){ print(Ansi::Red, "Queue delete!"); }

public:
    template <typename U>
    bool push(U &&data);

    std::optional<T> pop();

    void set_eof(bool is_eof);

    std::size_t size() const;

    bool empty() const;

    bool eof() const;

    void clear();

    void resize(std::size_t max_size);

private:
    std::deque<T> queue;
    mutable std::mutex mutex;
    std::size_t max_size = 0;
    bool is_eof {false};
};

// Queue<T> 成员函数的实现
template<class T>
template<typename U>
bool Queue<T>::push(U &&data)
{
    std::lock_guard<std::mutex> lock(this->mutex);
    assert(!this->is_eof);
    bool has_space = this->max_size == 0 || this->queue.size() < this->max_size;
    if (has_space)
    {
        this->queue.emplace_back(std::forward<U>(data));
        return true;
    }
    return false;
}

template <class T>
std::optional<T> Queue<T>::pop()
{
    std::lock_guard<std::mutex> lock(this->mutex);
    if (this->queue.empty())
        return std::nullopt;
    T data = std::move(this->queue.front());
    this->queue.pop_front();
    return data;
}

template<class T>
void Queue<T>::set_eof(bool is_eof)
{
    std::lock_guard<std::mutex> lock(this->mutex);
    this->is_eof = is_eof;
}

template <class T>
std::size_t Queue<T>::size() const
{
    std::lock_guard<std::mutex> lock(this->mutex);
    return this->queue.size();
}

template <class T>
bool Queue<T>::empty() const
{
    std::lock_guard<std::mutex> lock(this->mutex);
    return this->queue.empty();
}

template<class T>
bool Queue<T>::eof() const
{
    std::lock_guard<std::mutex> lock(this->mutex);
    return this->is_eof;
}

template <class T>
void Queue<T>::clear()
{
    std::lock_guard<std::mutex> lock(this->mutex);
    this->queue.clear();
}

template <class T>
void Queue<T>::resize(std::size_t max_size)
{
    std::lock_guard<std::mutex> lock(this->mutex);
    this->max_size = max_size;
}

#endif // DATA_MODEL_H
