#include "TranscodeOptions.h"

#include <format>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
}

#include "CodecContext.h"
#include "FFmpegException.h"
#include "HWDeviceContext.h"
#include "MediaInfo.h"

void TranscodeOptions::configure()
{
    if (this->input_file.empty())
        throw std::invalid_argument("Input file is required");
    if (this->output_file.empty())
        throw std::invalid_argument("Output file is required");

    MediaInfo info;
    info.open(this->input_file);

    // --- container-level defaults ---
    if (this->format.empty())
        this->format = info.format_name();
    if (std::isnan(this->start_time) || this->start_time < info.start_time())
        this->start_time = info.start_time();
    if (std::isnan(this->end_time) || (this->end_time - info.start_time() - info.duration()) > 0.1)
        this->end_time = info.duration();

    // --- video streams ---
    // 空 map → 自动添加所有源视频流
    if (this->video_streams.empty())
    {
        for (int i = 0; i < info.stream_count(); i++)
        {
            if (info.stream(i)->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                this->video_streams[i] = VideoStreamOptions{};
        }
    }

    // 逐个解析
    for (auto &[stream_index, opt] : this->video_streams)
    {
        if (stream_index < 0 || stream_index >= info.stream_count())
            throw std::invalid_argument(std::format("Video stream index {} out of range", stream_index));

        auto *st = info.stream(stream_index);
        if (st->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
            throw std::invalid_argument(std::format("Stream {} is not a video stream", stream_index));

        // --- hwaccel ---
        if (!opt.hwaccel.empty())
        {
            AVHWDeviceType hw_type = get_hwdevice_type(opt.hwaccel);
            if (hw_type == AV_HWDEVICE_TYPE_NONE)
                throw std::invalid_argument(std::format("Hardware acceleration not available: {}", opt.hwaccel));
        }

        // --- codec ---
        if (opt.codec.empty())
        {
            if (!opt.hwaccel.empty())
            {
                AVHWDeviceType hw_type = get_hwdevice_type(opt.hwaccel);
                const AVCodec *encoder = find_encoder_by_hw_device(hw_type);
                if (encoder)
                    opt.codec = encoder->name;
                else
                {
                    const AVCodec *fallback = avcodec_find_encoder(st->codecpar->codec_id);
                    if (fallback)
                        opt.codec = fallback->name;
                    else
                        throw std::invalid_argument(std::format("No encoder found for stream codec: {}",
                                                 avcodec_get_name(st->codecpar->codec_id)));
                }
            }
            else
            {
                const AVCodec *fallback = avcodec_find_encoder(st->codecpar->codec_id);
                if (fallback)
                    opt.codec = fallback->name;
                else
                    throw std::invalid_argument(std::format("No encoder found for stream codec: {}",
                                             avcodec_get_name(st->codecpar->codec_id)));
            }
        }

        const AVCodec *encoder = avcodec_find_encoder_by_name(opt.codec.c_str());
        if (!encoder)
            throw std::invalid_argument(std::format("Encoder not found: {}", opt.codec));

        // --- hwaccel auto-detect: 用户指定了硬件编码器但未指定 hwaccel ---
        if (opt.hwaccel.empty() && (encoder->capabilities & AV_CODEC_CAP_HARDWARE))
        {
            for (int i = 0;; i++)
            {
                const AVCodecHWConfig *cfg = avcodec_get_hw_config(encoder, i);
                if (!cfg) break;
                if (cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                    cfg->device_type != AV_HWDEVICE_TYPE_NONE)
                {
                    opt.hwaccel = av_hwdevice_get_type_name(cfg->device_type);
                    break;
                }
            }
        }

        // --- pixel format ---
        if (opt.pixel_format == AV_PIX_FMT_NONE)
        {
            if (is_hw_encoder(opt.codec))
                opt.pixel_format = get_hw_encoder_pix_fmt(encoder);
            else
                opt.pixel_format = get_default_pix_fmt(encoder);

            if (opt.pixel_format == AV_PIX_FMT_NONE)
                throw std::invalid_argument("Cannot find suitable pixel format for encoder");
        }
        else
        {
            if (!format_supported_by_encoder(opt.codec, opt.pixel_format))
                throw std::invalid_argument(std::format("Pixel format not supported by encoder {}", opt.codec));
        }

        // --- dimensions & framerate from source ---
        if (opt.width <= 0)
            opt.width = st->codecpar->width;
        if (opt.height <= 0)
            opt.height = st->codecpar->height;
        if (std::isnan(opt.frame_rate) || opt.frame_rate <= 0)
            opt.frame_rate = st->avg_frame_rate.num / (double)st->avg_frame_rate.den;

        if (opt.crf < 0 && opt.bitrate < 0)
            opt.crf = 23;
    }

    // --- audio streams ---
    if (this->audio_streams.empty())
    {
        for (int i = 0; i < info.stream_count(); i++)
        {
            if (info.stream(i)->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                this->audio_streams[i] = AudioStreamOptions{};
        }
    }

    for (auto &[stream_index, opt] : this->audio_streams)
    {
        if (stream_index < 0 || stream_index >= info.stream_count())
            throw std::invalid_argument(std::format("Audio stream index {} out of range", stream_index));

        auto *st = info.stream(stream_index);
        if (st->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
            throw std::invalid_argument(std::format("Stream {} is not an audio stream", stream_index));

        if (opt.codec.empty())
        {
            const AVCodec *encoder = avcodec_find_encoder(st->codecpar->codec_id);
            if (encoder)
                opt.codec = encoder->name;
            else
                throw std::invalid_argument(std::format("No encoder found for stream codec: {}",
                                         avcodec_get_name(st->codecpar->codec_id)));
        }

        if (opt.sample_format == AV_SAMPLE_FMT_NONE)
            opt.sample_format = (AVSampleFormat)st->codecpar->format;

        if (!format_supported_by_encoder(opt.codec, opt.sample_format))
            throw std::invalid_argument(std::format("Sample format not supported by encoder {}", opt.codec));

        if (opt.channel_layout.empty())
        {
            char buf[64];
            av_channel_layout_describe(&st->codecpar->ch_layout, buf, sizeof(buf));
            opt.channel_layout = buf;
        }

        AVChannelLayout layout;
        av_channel_layout_from_string(&layout, opt.channel_layout.c_str());
        if (layout.nb_channels == 0)
        {
            av_channel_layout_uninit(&layout);
            throw std::invalid_argument(std::format("Invalid channel layout: {}", opt.channel_layout));
        }
        av_channel_layout_uninit(&layout);

        if (opt.sample_rate <= 0)
            opt.sample_rate = st->codecpar->sample_rate;
    }
}
