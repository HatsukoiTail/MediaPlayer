#pragma once

#ifndef FFMPEG_MEDIAINFO_H
#define FFMPEG_MEDIAINFO_H

#include <map>
#include <string>
#include <string_view>
#include <vector>

extern "C"
{
#include <libavformat/avformat.h>
}

#include "Frame.h"

struct MediaChapter
{
    int64_t id;
    double start_time;
    double end_time;
    std::string title;
};


/// @brief MediaInfo provides information about media file/stream. 
/// The member functions will not modify the state of AVFormatContext.
class MediaInfo
{
public:
    MediaInfo() = default;
    MediaInfo(AVFormatContext* ctx, bool take_ownership = false);
    MediaInfo(const MediaInfo&) = delete;
    MediaInfo& operator=(const MediaInfo&) = delete;
    MediaInfo(MediaInfo&&) noexcept;
    MediaInfo& operator=(MediaInfo&&) noexcept;
    ~MediaInfo();

public:
    void open(std::string_view filename);
    void close();

public:
    bool is_opened() const;
    std::string file_path() const;
    std::string file_name() const;
    double duration() const;
    int64_t size() const;
    int64_t bit_rate() const;
    std::string format_name() const;
    int stream_count() const;
    AVStream* stream(int index) const;
    double start_time() const;
    AVFramePtr media_cover() const;
    std::map<std::string, std::string> metadata() const;
    std::string metadata(std::string_view key) const;
    std::vector<MediaChapter> chapters() const;

public:
    std::string video_codec_name() const;
    AVPixelFormat pixel_format() const;
    int width() const;
    int height() const;
    double frame_rate() const;
    double video_start_time() const;
    double video_duration() const;
    int64_t video_frame_count() const;
    int64_t video_bit_rate() const;

public:
    std::string audio_codec_name() const;
    AVSampleFormat sample_format() const;
    std::string channel_layout() const;
    int channel_count() const;
    int sample_rate() const;
    double audio_start_time() const;
    int64_t audio_frame_count() const;
    int64_t audio_bit_rate() const;
    int btyes_per_sample() const;

private:
    AVFormatContext *format_ctx = nullptr;
    bool format_ctx_owned = true;
    AVStream *video_stream = nullptr;
    AVStream *audio_stream = nullptr;
};

#endif // FFMPEG_MEDIAINFO_H