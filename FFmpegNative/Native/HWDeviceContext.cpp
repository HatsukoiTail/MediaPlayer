#include "HWDeviceContext.h"

#include <stdexcept>

extern "C"
{
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

#include "FFmpegException.h"
#include <iostream>

AVPixelFormat codec_support_hw_format(const AVCodec *codec, AVHWDeviceType hw_type)
{
    // encoder_support_hw_format(codec, AV_PIX_FMT_CUDA);
    for (int i = 0;; i++)
    {
        auto config = avcodec_get_hw_config(codec, i);
        if (config == nullptr)
            break;

        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == hw_type)
        {
            return config->pix_fmt;
        }
    }
    return AV_PIX_FMT_NONE;
}

AVPixelFormat hw_support_encode_format(const AVCodec *codec)
{
    const void *configs = nullptr;
    avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &configs, nullptr);
    auto pix_fmts = static_cast<const AVPixelFormat *>(configs);

    for (int i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++)
    {
        std::cout << "HW support encode format: " << av_get_pix_fmt_name(pix_fmts[i]) << std::endl;
    }
    return AV_PIX_FMT_NONE;
}

bool encoder_support_format(const AVCodecContext *avctx, const AVCodec *codec, AVPixelFormat format)
{
    const void *configs = nullptr;
    avcodec_get_supported_config(avctx, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &configs, nullptr);
    auto pix_fmts = static_cast<const AVPixelFormat *>(configs);
    while (*pix_fmts != AV_PIX_FMT_NONE)
    {
        if (*pix_fmts == format)
            return true;
        pix_fmts++;
    }
    return false;
}

bool is_hw_encoder(std::string_view codec_name)
{
    auto *encoder = avcodec_find_encoder_by_name(codec_name.data());
    if (encoder == nullptr)
        throw FFmpegException("Failed to find encoder by name: " + std::string(codec_name));

    return encoder->capabilities & AV_CODEC_CAP_HARDWARE;
}

bool is_hw_pix_fmt(AVPixelFormat pix_fmt)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(pix_fmt);
    return desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL);
}

std::string gpu_scale_filter(AVHWDeviceType type)
{
    switch (type)
    {
    case AV_HWDEVICE_TYPE_CUDA:
        return "scale_cuda";
    case AV_HWDEVICE_TYPE_QSV:
        return "vpp_qsv";
    case AV_HWDEVICE_TYPE_VAAPI:
        return "scale_vaapi";
    default:
        return {};
    }
}

AVPixelFormat hw_download_fmt(AVHWDeviceType type)
{
    switch (type)
    {
    case AV_HWDEVICE_TYPE_CUDA:
        return AV_PIX_FMT_NV12;
    case AV_HWDEVICE_TYPE_D3D11VA:
        return AV_PIX_FMT_NV12;
    case AV_HWDEVICE_TYPE_VAAPI:
        return AV_PIX_FMT_NV12;
    case AV_HWDEVICE_TYPE_QSV:
        return AV_PIX_FMT_NV12;
    case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
        return AV_PIX_FMT_NV12;
    default:
        return AV_PIX_FMT_YUV420P;
    }
}

AVHWDeviceType get_hwdevice_type(std::string_view hw_device_name)
{
    return av_hwdevice_find_type_by_name(hw_device_name.data());
}

AVHWDeviceType get_hwdevice_type(const AVCodecContext *ctx)
{
    // 优先读 hw_frames_ctx（信息更完整，含 sw_format）
    if (ctx->hw_frames_ctx)
    {
        auto *fctx = reinterpret_cast<AVHWFramesContext *>(ctx->hw_frames_ctx->data);
        return fctx->device_ctx->type;
    }
    // 回退到 hw_device_ctx（始终在 avcodec_open2 后可用）
    if (ctx->hw_device_ctx)
    {
        auto *dctx = reinterpret_cast<AVHWDeviceContext *>(ctx->hw_device_ctx->data);
        return dctx->type;
    }
    return AV_HWDEVICE_TYPE_NONE;
}

bool is_hw_device_availability(AVHWDeviceType hw_type)
{
    AVBufferRef *hw_ctx = nullptr;
    int ret = av_hwdevice_ctx_create(&hw_ctx, hw_type, nullptr, nullptr, 0);
    if (ret < 0)
        return false;

    av_buffer_unref(&hw_ctx);
    return true;
}

const AVCodec *find_encoder_by_hw_device(AVHWDeviceType hw_type)
{
    if (hw_type == AV_HWDEVICE_TYPE_NONE)
        return nullptr;

    // 优先查找 H.264 硬件编码器
    for (int pass = 0; pass < 2; pass++)
    {
        void *iter = nullptr;
        const AVCodec *codec;
        while ((codec = av_codec_iterate(&iter)))
        {
            if (!av_codec_is_encoder(codec))
                continue;
            if (!(codec->capabilities & AV_CODEC_CAP_HARDWARE))
                continue;

            // 第一遍只收 H.264；第二遍不限
            if (pass == 0 && codec->id != AV_CODEC_ID_H264)
                continue;

            // 检查是否支持该硬件设备类型
            for (int i = 0;; i++)
            {
                const AVCodecHWConfig *cfg = avcodec_get_hw_config(codec, i);
                if (!cfg)
                    break;
                if (cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                    cfg->device_type == hw_type)
                {
                    return codec;
                }
            }
        }
    }
    return nullptr;
}

AVPixelFormat get_hw_encoder_pix_fmt(const AVCodec *codec)
{
    const void *configs = nullptr;
    avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &configs, nullptr);
    auto pix_fmts = static_cast<const AVPixelFormat *>(configs);
    while (*pix_fmts != AV_PIX_FMT_NONE)
    {
        if (is_hw_pix_fmt(*pix_fmts))
            return *pix_fmts;
        pix_fmts++;
    }
    return AV_PIX_FMT_NONE;
}

AVPixelFormat get_encoder_sw_pix_fmt(const AVCodec *codec, AVBufferRef *hw_device_ctx)
{
    AVHWFramesConstraints *constraints = av_hwdevice_get_hwframe_constraints(hw_device_ctx, nullptr);
    if (!constraints)
        return AV_PIX_FMT_NONE;

    const void *configs = nullptr;
    avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &configs, nullptr);
    auto pix_fmts = static_cast<const AVPixelFormat *>(configs);
    AVPixelFormat result = AV_PIX_FMT_NONE;
    for (const AVPixelFormat *hw_fmt = constraints->valid_sw_formats;
         hw_fmt && *hw_fmt != AV_PIX_FMT_NONE; hw_fmt++)
    {
        for (const AVPixelFormat *p = pix_fmts; *p != AV_PIX_FMT_NONE; p++)
        {
            if (*p == *hw_fmt && !is_hw_pix_fmt(*p))
            {
                result = *hw_fmt;
                goto done;
            }
        }
    }
done:
    av_hwframe_constraints_free(&constraints);
    return AV_PIX_FMT_YUV420P;
    return result;
}

AVPixelFormat get_default_pix_fmt(const AVCodec *codec)
{
    const void *configs = nullptr;
    avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &configs, nullptr);
    auto pix_fmts = static_cast<const AVPixelFormat *>(configs);
    while (*pix_fmts != AV_PIX_FMT_NONE)
    {
        return *pix_fmts;
    }
    return AV_PIX_FMT_NONE;
}

bool encoder_support_hw_device(const AVCodec *codec, AVHWDeviceType hw_type)
{
    for (int i = 0;; i++)
    {
        auto config = avcodec_get_hw_config(codec, i);
        if (config == nullptr)
            break;

        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == hw_type && config->pix_fmt != AV_PIX_FMT_NONE)
        {
            return true;
        }
    }
    return false;
}

bool encoder_support_hw_device(std::string_view codec_name, AVHWDeviceType hw_type)
{
    auto *encoder = avcodec_find_encoder_by_name(codec_name.data());
    if (encoder == nullptr)
        throw FFmpegException("Failed to find encoder by name: " + std::string(codec_name));

    for (int i = 0;; i++)
    {
        auto config = avcodec_get_hw_config(encoder, i);
        if (config == nullptr)
            break;

        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == hw_type)
        {
            return true;
        }
    }
    return false;
}
