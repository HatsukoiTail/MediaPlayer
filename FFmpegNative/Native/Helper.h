#pragma once

#ifndef FFMPEG_HELPER_H
#define FFMPEG_HELPER_H

#include <string_view>

#include "CodecContext.h"
#include "FormatContext.h"
#include "Frame.h"
#include "HWDeviceContext.h"

// AVPacket* peek_attached_pic(avstrea)

std::string search_metadata(AVDictionary* metadata, std::string_view key);

AVFormatContextPtr open_format_context(std::string_view filename);

AVCodecContextPtr open_codec_context(const AVStream* stream);

AVBufferRefPtr create_hw_device_context(std::string_view device_name);

AVBufferRefPtr create_hw_device_context(AVHWDeviceType hw_type);

AVFramePtr download_frame(AVFrame* frame);

#endif // FFMPEG_HELPER_H