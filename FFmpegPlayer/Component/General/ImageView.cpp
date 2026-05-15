#include "ImageView.h"

#include <QPainter>
#include <QPainterPath>

ImageView::ImageView(QWidget *parent)
    : QWidget{parent}
{
}

void ImageView::setImage(const QImage &image)
{
    this->image = image;
}

void ImageView::fitToSize()
{
    if (this->image.isNull())
        return;
    this->image = this->image.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void ImageView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (this->image.isNull())
        return;
    QSize image_size = this->image.size();
    image_size.scale(size(), Qt::KeepAspectRatioByExpanding);
    int x = (width() - image_size.width()) / 2;
    int y = (height() - image_size.height()) / 2;
    QRect draw_rect(x, y, image_size.width(), image_size.height());
    QPainterPath path;
    path.addRoundedRect(rect(), 5.5, 5.5);
    painter.setClipPath(path);
    painter.drawPixmap(draw_rect, QPixmap::fromImage(this->image));
}
