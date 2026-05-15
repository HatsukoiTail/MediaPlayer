#pragma once

#ifndef FFMPEG_SWS_H
#define FFMPEG_SWS_H

extern "C"
{
#include <libswscale/swscale.h>
}

#include "Frame.h"

inline void format_frame(const AVFrame *input_frame, AVFrame *output_frame);
inline AVFramePtr format_frame(const AVFrame *input_frame, int dst_width, int dst_height, AVPixelFormat dst_format);

class PixelFormator
{
public:
    PixelFormator();
    PixelFormator(int dst_width, int dst_height, AVPixelFormat dst_format);
    PixelFormator(const PixelFormator &) = delete;
    PixelFormator &operator=(const PixelFormator &) = delete;
    PixelFormator(PixelFormator &&) = default;
    PixelFormator &operator=(PixelFormator &&) = default;
    ~PixelFormator();

public:
    void reset(int dst_width, int dst_height, AVPixelFormat dst_format);
    void format(const AVFrame *input_frame, AVFrame *output_frame);
    AVFramePtr format(const AVFrame *input_frame);

private:
    SwsContext *sws_ctx;
    int dst_width;
    int dst_height;
    AVPixelFormat dst_format;
};

#endif // FFMPEG_SWS_H