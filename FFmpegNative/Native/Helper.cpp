#include "Helper.h"

#include <stdexcept>

#include "FFmpegException.h"

std::string search_metadata(AVDictionary *metadata, std::string_view key)
{
    AVDictionaryEntry *entry = av_dict_get(metadata, key.data(), nullptr, 0);
    if (entry == nullptr)
        return {};
    return std::string(entry->value);
}

AVFormatContextPtr open_format_context(std::string_view filename)
{
    AVFormatContext *format_ctx = avformat_alloc_context();
    if (format_ctx == nullptr)
    {
        throw FFmpegException("Failed to allocate format context");
    }

    int ret = avformat_open_input(&format_ctx, filename.data(), nullptr, nullptr);
    if (ret < 0)
    {
        avformat_free_context(format_ctx);
        throw FFmpegException(ret, "Failed to open input file");
    }

    ret = avformat_find_stream_info(format_ctx, nullptr);
    if (ret < 0)
    {
        avformat_close_input(&format_ctx);
        throw FFmpegException(ret, "Failed to find stream info");
    }
    return AVFormatContextPtr(format_ctx);
}

AVCodecContextPtr open_codec_context(const AVStream *stream)
{
    auto codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == nullptr)
    {
        throw FFmpegException("Failed to find decoder");
    }

    auto codec_context = avcodec_alloc_context3(codec);
    if (codec_context == nullptr)
    {
        throw FFmpegException("Failed to allocate codec context");
    }

    if (avcodec_parameters_to_context(codec_context, stream->codecpar) < 0)
    {
        avcodec_free_context(&codec_context);
        throw FFmpegException("Failed to copy codec parameters");
    }

    codec_context->pkt_timebase = stream->time_base;

    if (avcodec_open2(codec_context, codec, nullptr) < 0)
    {
        avcodec_free_context(&codec_context);
        throw FFmpegException("Failed to open codec");
    }

    return AVCodecContextPtr(codec_context);
}

AVBufferRefPtr create_hw_device_context(std::string_view device_name)
{
    auto hw_type = av_hwdevice_find_type_by_name(device_name.data());
    if (hw_type == AV_HWDEVICE_TYPE_NONE)
    {
        return {};
    }
    AVBufferRef *hw_device_ctx = nullptr;
    if (av_hwdevice_ctx_create(&hw_device_ctx, hw_type, nullptr, nullptr, 0) < 0)
    {
        throw FFmpegException("Failed to allocate hw frames context");
    }
    return AVBufferRefPtr(hw_device_ctx);
}

AVBufferRefPtr create_hw_device_context(AVHWDeviceType hw_type)
{
    AVBufferRef *hw_device_ctx = nullptr;
    if (av_hwdevice_ctx_create(&hw_device_ctx, hw_type, nullptr, nullptr, 0) < 0)
    {
        throw FFmpegException("Failed to allocate hw frames context");
    }
    return AVBufferRefPtr(hw_device_ctx);
}

AVFramePtr download_frame(AVFrame *frame)
{
    AVFramePtr result(av_frame_alloc());
    int error = av_hwframe_transfer_data(result.get(), frame, 0);
    if (error < 0)
    {
        throw FFmpegException(error, "Failed to transfer frame data");
    }
    av_frame_copy_props(result.get(), frame);
    return result;
}
