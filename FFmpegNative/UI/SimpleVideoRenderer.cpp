#include "SimpleVideoRenderer.h"
#include <cstring>

extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

bool SimpleVideoRenderer::open(int width, int height, AVPixelFormat fmt)
{
    this->display_w = width > 0 ? width : 1920;
    this->display_h = height > 0 ? height : 1080;
    return true;
}

void SimpleVideoRenderer::close() {}
void SimpleVideoRenderer::set_hw_device(AVBufferRef *) {}

AVPixelFormat SimpleVideoRenderer::preferred_format() const
{
    return AV_PIX_FMT_YUV420P;  // 软解最通用
}

int SimpleVideoRenderer::width() const  { return this->display_w; }
int SimpleVideoRenderer::height() const { return this->display_h; }
bool SimpleVideoRenderer::wants_vsync() const { return false; }

void SimpleVideoRenderer::present(const AVFrame *frame)
{
    if (!frame || !frame->data[0])
        return;

    // YUV → RGB
    SwsContext *sws = sws_getContext(
        frame->width, frame->height, (AVPixelFormat)frame->format,
        frame->width, frame->height, AV_PIX_FMT_RGB32,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!sws)
        return;

    QImage img(frame->width, frame->height, QImage::Format_RGB32);

    uint8_t *dst[1] = { img.bits() };
    int dst_stride[1] = { (int)(img.bytesPerLine()) };

    sws_scale(sws, frame->data, frame->linesize, 0, frame->height,
              dst, dst_stride);
    sws_freeContext(sws);

    emit frame_ready(img);
}
