#pragma once

#ifndef FFMPEG_FFMPEGEXCEPTION_H
#define FFMPEG_FFMPEGEXCEPTION_H

#include <exception>
#include <string>

class FFmpegException : public std::exception
{
public:
    FFmpegException(std::string_view message);
    FFmpegException(int error_code, std::string_view message);
    static std::string error_message(int error_code);

public:
    int error_code() const;
    const char* what() const noexcept override;

private:
    int ffmpeg_error_code = 0;
    std::string user_error_message;
};

#endif // FFMPEG_FFMPEGEXCEPTION_H