#pragma once

#ifndef FFMPEG_HWDEVICECONTEXT_H
#define FFMPEG_HWDEVICECONTEXT_H

#include <memory>
#include <string_view>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}

struct AVBufferRefDeleter
{
    void operator()(AVBufferRef *ptr) const
    {
        av_buffer_unref(&ptr);
    }
};

using AVBufferRefPtr = std::unique_ptr<AVBufferRef, AVBufferRefDeleter>;

AVPixelFormat codec_support_hw_format(const AVCodec *codec, AVHWDeviceType hw_type);

AVPixelFormat hw_support_encode_format(const AVCodec* codec);

bool encoder_support_format(const AVCodecContext* avctx, const AVCodec* codec, AVPixelFormat format);


// 获取硬件设备类型
AVHWDeviceType get_hwdevice_type(std::string_view hw_device_name);
AVHWDeviceType get_hwdevice_type(const AVCodecContext *ctx);

// 检查硬件设备是否可用
bool is_hw_device_availability(AVHWDeviceType hw_type);

// 根据硬件设备查找编码器
const AVCodec* find_encoder_by_hw_device(AVHWDeviceType hw_type);

// 获取硬件编码器支持的opaque像素格式
AVPixelFormat get_hw_encoder_pix_fmt(const AVCodec* codec);

// 获取编码器的sw_format
AVPixelFormat get_encoder_sw_pix_fmt(const AVCodec* codec, AVBufferRef* hw_device_ctx);

// 获取编码器默认的像素格式
AVPixelFormat get_default_pix_fmt(const AVCodec* codec);

/// @brief 判断是否是硬件编码器
/// @param codec_name 
/// @return 
bool is_hw_encoder(std::string_view codec_name);


/// @brief 判断是否是硬件支持的像素格式
/// @param pix_fmt 
/// @return 
bool is_hw_pix_fmt(AVPixelFormat pix_fmt);

std::string gpu_scale_filter(AVHWDeviceType type);

AVPixelFormat hw_download_fmt(AVHWDeviceType type);

// 检测编码器是否支持硬件设备
bool encoder_support_hw_device(const AVCodec* codec, AVHWDeviceType hw_type);
bool encoder_support_hw_device(std::string_view codec_name, AVHWDeviceType hw_type);

#endif // FFMPEG_HWDEVICECONTEXT_H