#include "DataModel.h"

#include <cassert>

RingBuffer::RingBuffer(std::size_t capacity)
    : buffer(capacity), head(0), tail(0), size(0)
{
}

size_t RingBuffer::write(const uint8_t *data, size_t size)
{
    size_t written = 0;

    size_t space = this->buffer.size() - this->size;
    size_t to_write = std::min(size, space);
    if (to_write == 0) return 0;

    size_t first_part = std::min(to_write, this->buffer.size() - this->tail);
    std::copy(data, data + first_part, this->buffer.data() + this->tail);

    size_t second_part = to_write - first_part;
    if (second_part > 0)
        std::copy(data + first_part, data + to_write, this->buffer.data());

    this->tail = (this->tail + to_write) % this->buffer.size();
    this->size += to_write;
    
    return to_write;
}

size_t RingBuffer::read(uint8_t *data, size_t size)
{
    size_t to_read = std::min(size, this->size);
    if (to_read == 0) return 0;

    size_t first_part = std::min(to_read, this->buffer.size() - this->head);
    std::copy(this->buffer.data() + this->head, this->buffer.data() + this->head + first_part, data);

    size_t second_part = to_read - first_part;
    if (second_part > 0)
        std::copy(this->buffer.data(), this->buffer.data() + second_part, data + first_part);

    this->head = (this->head + to_read) % this->buffer.size();
    this->size -= to_read;

    return to_read;
}

size_t RingBuffer::available() const
{
    return this->size;
}

size_t RingBuffer::space() const
{
    return this->buffer.size() - this->size;
}

