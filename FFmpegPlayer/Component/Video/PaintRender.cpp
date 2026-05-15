#include "PaintRender.h"

#include <QPainter>

PaintRender::PaintRender(QWidget *parent)
    : QWidget{parent}
{}

void PaintRender::draw(AVFramePointer frame)
{
    std::lock_guard<std::mutex> lock(this->mutex);
    this->frame = std::move(frame);
    if (!this->frame)
    {
        this->image = QImage();
    }
    else if (this->frame->format == AV_PIX_FMT_RGB24)
    {
        this->image = QImage(this->frame->data[0], this->frame->width, this->frame->height, this->frame->linesize[0], QImage::Format_RGB888);
    }
    else if (this->frame->format == AV_PIX_FMT_BGR32)
    {
        this->image = QImage(this->frame->data[0], this->frame->width, this->frame->height, this->frame->linesize[0], QImage::Format_RGB32);
    }
    else if (this->frame->format == AV_PIX_FMT_RGBA)
    {
        this->image = QImage(this->frame->data[0], this->frame->width, this->frame->height, this->frame->linesize[0], QImage::Format_RGBA8888);
    }
    else
    {
        return;
    }
    update();
}

void PaintRender::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(this->rect(), Qt::black);
    std::lock_guard<std::mutex> lock(this->mutex);
    if (!this->image.isNull())
    {
        QRect rect = this->calc_draw_rect();
        painter.drawImage(rect, this->image);
    }
}

QRect PaintRender::calc_draw_rect()
{
    QSize widget_size = size();
    QSize image_size = this->image.size();

    // 计算等比例缩放后的尺寸
    image_size.scale(widget_size, Qt::KeepAspectRatio);

    // 计算居中位置
    int x = (widget_size.width() - image_size.width()) / 2;
    int y = (widget_size.height() - image_size.height()) / 2;

    return QRect(x, y, image_size.width(), image_size.height());
}
