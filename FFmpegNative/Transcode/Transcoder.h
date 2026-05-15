#pragma once

#ifndef FFMPEG_TRANSCODER_H
#define FFMPEG_TRANSCODER_H

#include <atomic>
#include <memory>
#include <vector>

#include "AudioFilter.h"
#include "Decoder.h"
#include "Demuxer.h"
#include "Encoder.h"
#include "HWDeviceContext.h"
#include "Muxer.h"
#include "TranscodeOptions.h"
#include "VideoFilter.h"

class Transcoder
{
public:
    Transcoder() = default;
    Transcoder(const Transcoder &) = delete;
    Transcoder &operator=(const Transcoder &) = delete;
    Transcoder(Transcoder &&) = delete;
    Transcoder &operator=(Transcoder &&) = delete;
    ~Transcoder() = default;

public:
    void open(TranscodeOptions options);
    void close();
    void start();
    void stop();
    double progress() const;

public:
    bool is_running() const;

private:
    struct Pipeline
    {
        std::shared_ptr<PacketQueue> input_packet_queue;
        std::unique_ptr<Transcode::Decoder> decoder;
        std::unique_ptr<Transcode::VideoFilter> video_filter;
        std::unique_ptr<Transcode::AudioFilter> audio_filter;
        std::unique_ptr<Transcode::Encoder> encoder;
        std::shared_ptr<PacketQueue> output_packet_queue;
    };

    void build_video_pipeline(const VideoStreamOptions &options, const AVStream *stream);
    void build_audio_pipeline(const AudioStreamOptions &options, const AVStream *stream);
    void build_subtitle_pipeline(const AVStream *stream);

    void continue_build_video_pipeline(Pipeline &pipeline, const VideoStreamOptions &options, const AVStream *stream);

private:
    TranscodeOptions options;
    Transcode::Demuxer demuxer;
    Transcode::Muxer muxer;
    std::map<int, Pipeline> pipelines;
    std::atomic<int> pending_callbacks{0};
};

#endif // FFMPEG_TRANSCODER_H
