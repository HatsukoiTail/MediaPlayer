#ifndef FORMAT_H
#define FORMAT_H

extern "C"
{
#include "libavutil/pixfmt.h"
#include "libavutil/samplefmt.h"
#include "portaudio.h"
}

enum class SampleFormat
{
    None, UInt8, SInt16, SInt32, Float32, Float32P
};

enum class PixelFormat
{
    None, YUV420P, RGB24, RGBA32, NV12
};

struct AudioFormat
{
    int sampleRate {-1};
    int channels {-1};
    SampleFormat sampleFormat {SampleFormat::None};
    bool isValid() const;
};

struct VideoFormat
{
    int width {-1};
    int height {-1};
    PixelFormat pixelFormat {PixelFormat::None};
    bool isValid() const;
};

bool isFormatSupport(SampleFormat format);
bool isFormatSupport(PixelFormat format);

AVSampleFormat to_ff_format(SampleFormat format);

PaSampleFormat to_pa_format(SampleFormat format);

AVPixelFormat to_ff_format(PixelFormat format);

SampleFormat from_ff_format(AVSampleFormat format);

PixelFormat from_ff_format(AVPixelFormat format);

int sample_size(SampleFormat format);

#endif // FORMAT_H
