#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include "Decoder.h"
#include "Format.h"

class VideoDecoder : public Decoder
{
public:
    VideoDecoder(std::shared_ptr<Queue<AVPacketPointer>> packets);
    ~VideoDecoder();
    void set_format(const VideoFormat& format);
    VideoFormat default_format() const;
    void close();

private:
    AVFramePointer process(AVFramePointer frame) override;

private:
    SwsContextPointer sws_ctx;
    VideoFormat src_format;
    VideoFormat dst_format;
};

#endif // VIDEODECODER_H
