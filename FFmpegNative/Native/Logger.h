#pragma once

#ifndef FFMPEG_LOGGER_H
#define FFMPEG_LOGGER_H

#include <chrono>
#include <fstream>
#include <format>
#include <mutex>
#include <string_view>

class Logger
{
public:
    Logger() = default;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    Logger(Logger &&) noexcept;
    Logger &operator=(Logger &&) noexcept;
    ~Logger();

public:
    void open(std::string_view file_path);
    void close();
    void log(std::string_view message);

private:
    std::ofstream file;
    std::recursive_mutex mutex;
    bool is_opened = false;
};

#endif // FFMPEG_LOGGER_H
