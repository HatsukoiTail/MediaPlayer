#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include "Decoder.h"
#include "Format.h"

class AudioDecoder : public Decoder
{
public:
    AudioDecoder(std::shared_ptr<Queue<AVPacketPointer>> packets);
    ~AudioDecoder();

public:
    void close();
    void set_format(const AudioFormat& format);
    void set_speed(double speed);
    AudioFormat default_format() const;

private:
    AVFramePointer process(AVFramePointer frame) override;
    AVFramePointer speedup(AVFramePointer frame);
    AVFramePointer resample(AVFramePointer frame);

private:
    double play_speed {1.0};
    double filter_speed {1.0};

private:
    SwrContextPointer swr_ctx;
    AVFilterGraphPointer filter_graph;
    AudioFormat src_format;
    AudioFormat dst_format;
};

#endif // AUDIODECODER_H
