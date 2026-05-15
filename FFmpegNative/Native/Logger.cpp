#include "Logger.h"

#include <chrono>
#include <format>
#include <iostream>

Logger::Logger(Logger &&other) noexcept
{
    this->file = std::move(other.file);
    this->is_opened = other.is_opened;
    other.is_opened = false;
}

Logger &Logger::operator=(Logger &&other) noexcept
{
    if (this != &other)
    {
        this->file = std::move(other.file);
        this->is_opened = other.is_opened;
        other.is_opened = false;
    }
    return *this;
}

Logger::~Logger()
{
    this->close();
}

void Logger::open(std::string_view file_path)
{
    std::lock_guard<std::recursive_mutex> locker(this->mutex);

    if (this->is_opened)
    {
        this->file.close();
    }

    this->file.open(file_path.data(), std::ios::out | std::ios::trunc);
    if (this->file.is_open() == false)
    {
        std::cerr << std::format("Logger: failed to open {}\n", file_path);
        return;
    }

    this->is_opened = true;
    this->log("=== Logger started ===");
}

void Logger::close()
{
    std::lock_guard<std::recursive_mutex> locker(this->mutex);

    if (this->is_opened == false)
        return;

    this->log("=== Logger stopped ===");
    this->file.close();
    this->is_opened = false;
}

void Logger::log(std::string_view message)
{
    std::lock_guard<std::recursive_mutex> locker(this->mutex);

    if (this->is_opened == false)
        return;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    char time_buf[32];
    std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", std::localtime(&time));

    this->file << std::format("[{} {:03}] {}\n", time_buf, ms.count(), message);
    this->file.flush();
}
