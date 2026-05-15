#pragma once

#ifndef FFMPEG_FRAMELOADER_H
#define FFMPEG_FRAMELOADER_H

#include <stop_token>

#include "CodecContext.h"
#include "FormatContext.h"
#include "Frame.h"
#include "Packet.h"

class FrameLoader
{
public:
    FrameLoader(std::string_view url);

public:
    bool is_valid() const;
    void seek(double position);
    AVFramePtr load_frame(std::stop_token cancel_token);

public:
    static AVFramePtr load_frame(std::stop_token cancel_token, std::string_view url, double position);

private:
    void open(std::string_view url);

private:
    FormatContext format_ctx;
    AVCodecContextPtr codec_ctx;
    int video_stream_index = -1;
    AVPacketPtr packet;
};

#endif // FFMPEG_FRAMELOADER_H