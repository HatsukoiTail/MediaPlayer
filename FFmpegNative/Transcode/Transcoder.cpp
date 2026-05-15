#include "Transcoder.h"

#include <cassert>
#include <format>
#include <stdexcept>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
}

#include "FFmpegException.h"
#include "Helper.h"

// ===========================================================================

void Transcoder::open(TranscodeOptions options)
{
    options.configure();

    this->demuxer.open(options.input_file);
    this->muxer.open(options.output_file, options.format);

    // --- metadata & cover ---
    if (!options.metadata.empty())
        this->muxer.set_metadata(options.metadata);
    if (!options.cover.empty())
        this->muxer.set_cover(options.cover);

    for (int i = 0; i < this->demuxer.stream_count(); i++)
    {
        auto in_stream = this->demuxer.stream(i);
        auto codec_type = in_stream->codecpar->codec_type;

        switch (codec_type)
        {
        case AVMEDIA_TYPE_VIDEO:
        {
            auto it = options.video_streams.find(i);
            if (it != options.video_streams.end())
                this->build_video_pipeline(it->second, in_stream);
            break;
        }
        case AVMEDIA_TYPE_AUDIO:
        {
            auto it = options.audio_streams.find(i);
            if (it != options.audio_streams.end())
                this->build_audio_pipeline(it->second, in_stream);
            break;
        }
        case AVMEDIA_TYPE_SUBTITLE:
        {
            this->build_subtitle_pipeline(in_stream);
            break;
        }
        }
    }

    this->options = std::move(options);
}

void Transcoder::close()
{
    this->stop();
    this->pipelines.clear();
    this->muxer.close();
    this->demuxer.close();
}

void Transcoder::start()
{
    this->demuxer.start();

    int pending = 0;
    for (auto &[_, pipeline] : this->pipelines)
    {
        if (pipeline.decoder)
            pipeline.decoder->start();
        if (pipeline.video_filter)
            pipeline.video_filter->start();
        if (pipeline.audio_filter)
            pipeline.audio_filter->start();
        if (pipeline.encoder)
            pipeline.encoder->start();
        else
            pending++; // 编码器尚未创建（延迟到回调中构建）
    }

    this->pending_callbacks = pending;
    if (pending == 0)
        this->muxer.start();
}

void Transcoder::stop()
{
    this->demuxer.stop();

    for (auto &[_, pipeline] : this->pipelines)
    {
        if (pipeline.decoder)
            pipeline.decoder->stop();
        if (pipeline.video_filter)
            pipeline.video_filter->stop();
        if (pipeline.audio_filter)
            pipeline.audio_filter->stop();
        if (pipeline.encoder)
            pipeline.encoder->stop();
    }

    this->muxer.stop();
}

double Transcoder::progress() const
{
    if (!this->muxer.is_opened())
        return 0.0;
    if (this->muxer.eof())
        return 1.0;
    return (this->muxer.progress() - this->options.start_time) /
           (this->options.end_time - this->options.start_time);
}

bool Transcoder::is_running() const
{
    return this->muxer.is_running() || this->demuxer.is_running();
}

// ---------------------------------------------------------------------------
// pipeline builders
// ---------------------------------------------------------------------------

void Transcoder::build_video_pipeline(const VideoStreamOptions &opt, const AVStream *stream)
{
    // --- 按流创建硬件设备 ---
    AVBufferRefPtr dec_device;
    if (!opt.hwaccel.empty())
        dec_device = create_hw_device_context(opt.hwaccel);

    Pipeline pipeline;

    pipeline.input_packet_queue = this->demuxer.stream_queue(stream->index);

    pipeline.decoder = std::make_unique<Transcode::Decoder>();
    pipeline.decoder->open(stream, dec_device.get());
    pipeline.decoder->set_input_queue(pipeline.input_packet_queue);
    pipeline.decoder->set_hwctx_ready_callback([this, index = stream->index](const AVCodecContext* ctx){
        auto pipeline_it = this->pipelines.find(index);
        assert(pipeline_it != this->pipelines.end());
        auto option_it = this->options.video_streams.find(index);
        assert(option_it != this->options.video_streams.end());
        auto stream = this->demuxer.stream(index);
        this->continue_build_video_pipeline(pipeline_it->second, option_it->second, stream);
    });

    this->pipelines.insert(std::make_pair(stream->index, std::move(pipeline)));
}

void Transcoder::build_audio_pipeline(const AudioStreamOptions &opt, const AVStream *stream)
{
    Pipeline pipeline;

    pipeline.input_packet_queue = this->demuxer.stream_queue(stream->index);

    pipeline.decoder = std::make_unique<Transcode::Decoder>();
    pipeline.decoder->open(stream, nullptr);
    pipeline.decoder->set_input_queue(pipeline.input_packet_queue);

    pipeline.encoder = std::make_unique<Transcode::Encoder>();
    pipeline.encoder->open(opt, nullptr);
    auto out_stream = this->muxer.add_stream(pipeline.encoder->codec_context());
    pipeline.encoder->set_stream_index(out_stream->index);

    pipeline.audio_filter = std::make_unique<Transcode::AudioFilter>();
    pipeline.audio_filter->open(pipeline.decoder->codec_context(),
                                pipeline.encoder->codec_context());
    pipeline.audio_filter->set_input_queue(pipeline.decoder->output_queue());

    pipeline.encoder->set_input_queue(pipeline.audio_filter->output_queue());

    pipeline.output_packet_queue = pipeline.encoder->output_queue();
    this->muxer.set_input_queue(out_stream->index, pipeline.output_packet_queue);

    this->pipelines.insert(std::make_pair(stream->index, std::move(pipeline)));
}

void Transcoder::build_subtitle_pipeline(const AVStream *stream)
{
    Pipeline pipeline;

    pipeline.input_packet_queue = this->demuxer.stream_queue(stream->index);
    pipeline.output_packet_queue = pipeline.input_packet_queue;
    this->muxer.set_input_queue(stream->index, pipeline.output_packet_queue);

    this->pipelines.insert(std::make_pair(stream->index, std::move(pipeline)));
}

void Transcoder::continue_build_video_pipeline(Pipeline &pipeline, const VideoStreamOptions &options, const AVStream *stream)
{
    pipeline.encoder = std::make_unique<Transcode::Encoder>();
    pipeline.encoder->open(options, pipeline.decoder->codec_context());
    auto out_stream = this->muxer.add_stream(pipeline.encoder->codec_context());
    pipeline.encoder->set_stream_index(out_stream->index);

    pipeline.video_filter = std::make_unique<Transcode::VideoFilter>();
    pipeline.video_filter->open(pipeline.decoder->codec_context(),
                                pipeline.encoder->codec_context());
    pipeline.video_filter->set_input_queue(pipeline.decoder->output_queue());

    pipeline.encoder->set_input_queue(pipeline.video_filter->output_queue());

    pipeline.output_packet_queue = pipeline.encoder->output_queue();
    this->muxer.set_input_queue(out_stream->index, pipeline.output_packet_queue);

    // 启动流水线
    pipeline.video_filter->start();
    pipeline.encoder->start();

    // 所有延迟流水线构建完毕 → 启动 muxer
    if (--this->pending_callbacks == 0)
        this->muxer.start();
}
