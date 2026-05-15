#include "Format.h"

bool AudioFormat::isValid() const
{
    return this->sampleRate > 0 && this->channels > 0 && this->sampleFormat != SampleFormat::None;
}

bool VideoFormat::isValid() const
{
    return this->width > 0 && this->height > 0 && this->pixelFormat != PixelFormat::None;
}

AVSampleFormat to_ff_format(SampleFormat format)
{
    switch (format) {
    case SampleFormat::UInt8:
        return AV_SAMPLE_FMT_U8;
    case SampleFormat::SInt16:
        return AV_SAMPLE_FMT_S16;
    case SampleFormat::SInt32:
        return AV_SAMPLE_FMT_S32;
    case SampleFormat::Float32:
        return AV_SAMPLE_FMT_FLT;
    case SampleFormat::Float32P:
        return AV_SAMPLE_FMT_FLTP;
    default:
        return AV_SAMPLE_FMT_NONE;
    }
}

PaSampleFormat to_pa_format(SampleFormat format)
{
    switch (format) {
    case SampleFormat::UInt8:
        return paUInt8;
    case SampleFormat::SInt16:
        return paInt16;
    case SampleFormat::SInt32:
        return paInt32;
    case SampleFormat::Float32:
        return paFloat32;
    default:
        return -1;
    }
}

AVPixelFormat to_ff_format(PixelFormat format)
{
    switch (format) {
    case PixelFormat::YUV420P:
        return AV_PIX_FMT_YUV420P;
    case PixelFormat::RGB24:
        return AV_PIX_FMT_RGB24;
    case PixelFormat::RGBA32:
        return AV_PIX_FMT_RGBA;
    case PixelFormat::NV12:
        return AV_PIX_FMT_NV12;
    default:
        return AV_PIX_FMT_NONE;
    }
}

int sample_size(SampleFormat format)
{
    switch (format) {
    case SampleFormat::UInt8:
        return 1;
    case SampleFormat::SInt16:
        return 2;
    case SampleFormat::SInt32:
        return 4;
    case SampleFormat::Float32:
        return 4;
    default:
        return 0;
    }
}

SampleFormat from_ff_format(AVSampleFormat format)
{
    switch (format) {
    case AV_SAMPLE_FMT_U8:
        return SampleFormat::UInt8;
    case AV_SAMPLE_FMT_S16:
        return SampleFormat::SInt16;
    case AV_SAMPLE_FMT_S32:
        return SampleFormat::SInt32;
    case AV_SAMPLE_FMT_FLT:
        return SampleFormat::Float32;
    case AV_SAMPLE_FMT_FLTP:
        return SampleFormat::Float32P;
    default:
        return SampleFormat::None;
    }
}

PixelFormat from_ff_format(AVPixelFormat format)
{
    switch (format) {
    case AV_PIX_FMT_YUV420P:
        return PixelFormat::YUV420P;
    case AV_PIX_FMT_RGB24:
        return PixelFormat::RGB24;
    case AV_PIX_FMT_RGBA:
        return PixelFormat::RGBA32;
    case AV_PIX_FMT_NV12:
        return PixelFormat::NV12;
    default:
        return PixelFormat::None;
    }
}

bool isFormatSupport(SampleFormat format)
{
    return format == SampleFormat::UInt8 ||
           format == SampleFormat::SInt16 ||
           format == SampleFormat::SInt32 ||
           format == SampleFormat::Float32;
}

bool isFormatSupport(PixelFormat format)
{
    return format == PixelFormat::RGB24 ||
           format == PixelFormat::RGBA32;
}
