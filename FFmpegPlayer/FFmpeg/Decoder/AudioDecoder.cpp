#include "AudioDecoder.h"

#include "Factory.h"
#include "Print.h"
#include "Util.h"

AudioDecoder::AudioDecoder(std::shared_ptr<Queue<AVPacketPointer>> packets)
    : Decoder{packets}
{

}

AudioDecoder::~AudioDecoder()
{
    this->close();
    print("AudioDecoder delete!");
}

void AudioDecoder::close()
{
    Decoder::close();
    this->swr_ctx.reset();
}

void AudioDecoder::set_format(const AudioFormat &format)
{
    assert(this->state() != Decoder::State::Closed);
    this->dst_format = format;
}

void AudioDecoder::set_speed(double speed)
{
    this->play_speed = speed;
}

AudioFormat AudioDecoder::default_format() const
{
    assert(this->state() != Decoder::State::Closed);
    return {
        .sampleRate = sample_rate(),
        .channels = channel_count(),
        .sampleFormat = from_ff_format(sample_format())
    };
}

AVFramePointer AudioDecoder::process(AVFramePointer frame)
{
    auto speed_frame = this->speedup(std::move(frame));
    if (!speed_frame)
        return {};

    auto resample_frame = this->resample(std::move(speed_frame));
    if (!resample_frame)
        return {};

    return resample_frame;
}

AVFramePointer AudioDecoder::speedup(AVFramePointer frame)
{
    auto play_speed = this->play_speed;
    if (std::fabs(play_speed - 1.0) < 0.01)
        return frame;

    if (std::fabs(play_speed - this->filter_speed) > 0.01)
    {
        this->filter_graph = AVFilterGraphPointer(open_filter_graph(play_speed, frame->sample_rate, static_cast<AVSampleFormat>(frame->format), &frame->ch_layout));
        this->filter_speed = play_speed;
    }
    if (!this->filter_graph)
        return frame;

    auto input = avfilter_graph_get_filter(this->filter_graph.get(), "in");
    auto output = avfilter_graph_get_filter(this->filter_graph.get(), "out");

    if (!input || !output)
        return frame;

    // 保存pts
    auto pts = frame->pts;
    auto time_base = frame->time_base;
    int result = av_buffersrc_add_frame(input, frame.get());
    if (result < 0)
    {
        print(Ansi::Red, "Fail to add frame to filter, {}", debug(result));
        return frame;
    }
    while (true)
    {
        AVFramePointer filter_frame = AVFramePointer(av_frame_alloc());
        result = av_buffersink_get_frame(output, filter_frame.get());
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
        {
            return nullptr;
        }
        if (result < 0)
        {
            return nullptr;
        }
        std::string speed_str = std::to_string(play_speed);
        av_dict_set(&filter_frame->metadata, "speed", speed_str.c_str(), 0);
        filter_frame->pts = pts;
        filter_frame->time_base = time_base;
        return filter_frame;
    }
}

AVFramePointer AudioDecoder::resample(AVFramePointer frame)
{
    const bool is_need_resample = frame->sample_rate != this->dst_format.sampleRate ||
                                  frame->ch_layout.nb_channels != this->dst_format.channels ||
                                  frame->format != to_ff_format(this->dst_format.sampleFormat);
    if (this->dst_format.isValid() && is_need_resample)
    {
        const bool need_new_swr = !this->swr_ctx ||
                                  (frame->sample_rate != this->src_format.sampleRate ||
                                   frame->ch_layout.nb_channels != this->src_format.channels ||
                                   frame->format != to_ff_format(this->src_format.sampleFormat));
        if (need_new_swr)
        {
            // 创建最新有效的重采样上下文
            this->src_format.sampleRate = frame->sample_rate;
            this->src_format.channels = frame->ch_layout.nb_channels;
            this->src_format.sampleFormat = from_ff_format(static_cast<AVSampleFormat>(frame->format));

            AVChannelLayout dst_layout;
            av_channel_layout_default(&dst_layout, this->dst_format.channels);

            this->swr_ctx = SwrContextPointer(open_swr_context(&frame->ch_layout, to_ff_format(this->src_format.sampleFormat), this->src_format.sampleRate,
                                                               &dst_layout, to_ff_format(this->dst_format.sampleFormat), this->dst_format.sampleRate));
            av_channel_layout_uninit(&dst_layout);
        }

        // 重采样
        AVFramePointer resample_frame = AVFramePointer(av_frame_alloc());
        resample_frame->format = to_ff_format(this->dst_format.sampleFormat);
        resample_frame->sample_rate = this->dst_format.sampleRate;
        av_channel_layout_default(&resample_frame->ch_layout, this->dst_format.channels);
        // 计算目标样本数
        resample_frame->nb_samples = av_rescale_rnd(swr_get_delay(this->swr_ctx.get(), frame->sample_rate) + frame->nb_samples,
                                                    resample_frame->sample_rate, frame->sample_rate, AV_ROUND_UP);

        int err = av_frame_get_buffer(resample_frame.get(), 0);
        if (err < 0)
        {
            print(Ansi::Red, "Fail to allocate audio frame buffer, {}", debug(err));
            return nullptr;
        }

        int result = swr_convert_frame(this->swr_ctx.get(), resample_frame.get(), frame.get());
        if (result < 0)
        {
            print(Ansi::Red, "Fail to convert audio frame, {}", debug(result));
            return nullptr;
        }

        resample_frame->pts = frame->pts;
        resample_frame->time_base = frame->time_base;
        return resample_frame;
    }
    return frame;
}
