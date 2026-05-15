#include "FFmpegException.h"

extern "C"
{
#include <libavformat/avformat.h>
}

static std::string ffmpeg_error_to_string(int error_code)
{
    if (error_code >= 0)
    {
        return {};
    }
    std::string error_message(AV_ERROR_MAX_STRING_SIZE, '\0');
    av_strerror(error_code, error_message.data(), error_message.size());
    return error_message.substr(0, error_message.find('\0'));
}

FFmpegException::FFmpegException(std::string_view message)
    : ffmpeg_error_code(0), user_error_message(message)
{
}

FFmpegException::FFmpegException(int error_code, std::string_view message)
    : ffmpeg_error_code(error_code), user_error_message(message)
{
}

std::string FFmpegException::error_message(int error_code)
{
    return ffmpeg_error_to_string(error_code);
}

int FFmpegException::error_code() const
{
    return this->ffmpeg_error_code;
}

const char *FFmpegException::what() const noexcept
{
    auto message = this->user_error_message + "(" + ffmpeg_error_to_string(this->ffmpeg_error_code) + ")";
    return message.c_str();
}
