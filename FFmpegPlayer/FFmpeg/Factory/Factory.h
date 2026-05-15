#ifndef FACTORY_H
#define FACTORY_H

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

using ReadCallback = int (*)(void *opaque, uint8_t *buf, int buf_size);
using SeekCallback = int64_t (*)(void *opaque, int64_t offset, int whence);

AVFormatContext *open_format_context(const char *path);

AVFormatContext *open_format_context(void *opaque, ReadCallback read_callback, SeekCallback seek_callback);

AVCodecContext *open_codec_context(AVStream* stream);

SwsContext *open_sws_context(int src_width, int src_height, AVPixelFormat src_format,
                               int dst_width, int dst_height, AVPixelFormat dst_format);

SwrContext *open_swr_context(AVChannelLayout *src_layout, AVSampleFormat src_format, int src_rate,
                               AVChannelLayout *dst_layout, AVSampleFormat dst_format, int dst_rate);

AVFilterGraph* open_filter_graph(double speed, int sample_rate, AVSampleFormat sample_format, const AVChannelLayout* channel_layout);

#endif // FACTORY_H
