#pragma once

#ifndef FFMPEG_TRANSCODEOPTIONS_H
#define FFMPEG_TRANSCODEOPTIONS_H

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>

extern "C"
{
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
}

struct VideoStreamOptions
{
    std::string codec;
    std::string hwaccel; // 硬件解码设备类型（如 "cuda"、"d3d11va"），为空则软解
    int crf = -1;
    int bitrate = -1;
    int width = -1;
    int height = -1;
    double frame_rate = std::numeric_limits<double>::quiet_NaN();
    AVPixelFormat pixel_format = AV_PIX_FMT_NONE;
    int gop_size = -1;
};

struct AudioStreamOptions
{
    std::string codec;
    int bitrate = -1;
    int sample_rate = -1;
    std::string channel_layout;
    AVSampleFormat sample_format = AV_SAMPLE_FMT_NONE;
};

struct TranscodeOptions
{
    // --- container-level ---
    std::string input_file;
    std::string output_file;
    std::string format;
    double start_time = std::numeric_limits<double>::quiet_NaN();
    double end_time = std::numeric_limits<double>::quiet_NaN();
    std::map<std::string, std::string> metadata;
    std::vector<uint8_t> cover;

    // --- per-stream: key = source stream index ---
    // 空 map → 自动保留所有该类型流；非空 → 仅保留 map 中存在的索引
    std::map<int, VideoStreamOptions> video_streams;
    std::map<int, AudioStreamOptions> audio_streams;

    void configure();
};

#endif // FFMPEG_TRANSCODEOPTIONS_H
