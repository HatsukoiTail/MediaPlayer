#pragma once

#ifndef FFMPEG_IMAGELOADER_H
#define FFMPEG_IMAGELOADER_H

#include <string_view>

#include "CodecContext.h"
#include "FormatContext.h"
#include "Frame.h"
#include "Packet.h"

AVFramePtr load_image(std::string_view url);

class ImageLoader
{
public:
    ImageLoader() = default;
    ImageLoader(const ImageLoader &) = delete;
    ImageLoader &operator=(const ImageLoader &) = delete;
    ImageLoader(ImageLoader &&) noexcept;
    ImageLoader &operator=(ImageLoader &&) noexcept;
    ~ImageLoader();

public:
    void open(std::string_view url);
    AVFramePtr load_frame();
    bool is_valid() const;
    int width() const;
    int height() const;

public:
    static AVFramePtr load_frame(std::string_view url);

private:
    AVFormatContextPtr format_ctx;
    AVCodecContextPtr codec_ctx;
    AVPacketPtr packet;
    int video_stream_index = -1;
    int image_width = 0;
    int image_height = 0;
    bool frame_loaded = false;
};

#endif // FFMPEG_IMAGELOADER_H
