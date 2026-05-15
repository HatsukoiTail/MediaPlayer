#include "Image.h"

#include "MediaInfo.h"

#include <QPainter>
#include <QWheelEvent>

Image::Image(QWidget *parent)
    : QWidget{parent}
{}

bool Image::open(const QString &path)
{
    MediaInfo info(path);
    if (!info.isOpen())
        return false;
    this->image = info.image();
    return true;
}

void Image::play()
{
    update();
}

void Image::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (this->image.isNull())
        return;

    QRect image_rect(this->offset.toPoint(), this->image.size() * this->scale);
    QRect visible_rect = rect().intersected(image_rect);

    QRectF source_rect;
    source_rect.setLeft((visible_rect.left() - image_rect.left()) * image.width() / image_rect.width());
    source_rect.setTop((visible_rect.top() - image_rect.top()) * image.height() / image_rect.height());
    source_rect.setWidth(visible_rect.width() * image.width() / image_rect.width());
    source_rect.setHeight(visible_rect.height() * image.height() / image_rect.height());

    painter.drawImage(visible_rect, image, source_rect);
}

void Image::resizeEvent(QResizeEvent *event)
{
    if (this->image.isNull())
        return;

    if (this->fit_mode)
    {
        this->center_image();
        update();
        return;
    }
    QSizeF image_size = this->image.size() * this->scale;

    bool fit_width = image_size.width()  <= width();
    bool fit_height = image_size.height() <= height();
    if (fit_width && fit_height)
    {
        this->fit_mode = true;
        this->center_image();
        update();
        return;
    }

    this->translate_image_pos();
    update();
}

void Image::wheelEvent(QWheelEvent *event)
{
    if (this->image.isNull())
        return;
    constexpr double factor = 1.05;
    double old_scale = this->scale;
    if (event->angleDelta().y() > 0)
        this->scale *= factor;
    else
        this->scale /= factor;

    double scale_fit_width = static_cast<double>(width()) / image.width();
    double scale_fit_height = static_cast<double>(height()) / image.height();
    double scale_min = std::min(scale_fit_width, scale_fit_height);

    this->scale = std::clamp(this->scale, scale_min, 20.0);

    QPointF mouse_in_widget = event->position();
    QPointF mouse_in_image = mouse_in_widget - this->offset;

    QSize image_size = this->image.size() * old_scale;
    const QPointF mouse_in_image_ratio(mouse_in_image.x() / image_size.width(), mouse_in_image.y() / image_size.height());

    QSize new_image_size = this->image.size() * this->scale;
    QPointF new_offset(mouse_in_widget.x() - mouse_in_image_ratio.x() * new_image_size.width(),
                       mouse_in_widget.y() - mouse_in_image_ratio.y() * new_image_size.height());
    this->offset = new_offset;

    this->translate_image_pos();
    this->fit_mode = false;
    update();
}

void Image::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        this->dragging = true;
        this->last_pos = event->pos();
        this->setCursor(Qt::PointingHandCursor);
    }
}

void Image::mouseMoveEvent(QMouseEvent *event)
{
    if (this->dragging)
    {
        QPointF delta = event->pos() - this->last_pos;
        this->offset += delta;
        this->last_pos = event->pos();
        this->translate_image_pos();
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void Image::mouseReleaseEvent(QMouseEvent *event)
{
    this->dragging = false;
    this->unsetCursor();
}

void Image::center_image()
{
    double scale_x = static_cast<double>(width()) / this->image.width();
    double scale_y = static_cast<double>(height()) / this->image.height();
    this->scale = std::min(scale_x, scale_y);
    QSize new_image_size = this->image.size() * this->scale;
    double offset_x = (width() - new_image_size.width()) / 2;
    double offset_y = (height() - new_image_size.height()) / 2;
    this->offset = QPointF(offset_x, offset_y);
}

void Image::translate_image_pos()
{
    QSizeF image_size = this->image.size() * this->scale;

    if (image_size.width() >= width())
    {
        double min_x = width() - image_size.width();
        double max_x = 0;
        offset.setX(std::clamp(this->offset.x(), min_x, max_x));
    }
    else
    {
        offset.setX((width() - image_size.width()) / 2.0);
    }

    if (image_size.height() >= height())
    {
        double min_y = height() - image_size.height();
        double max_y = 0;
        offset.setY(std::clamp(this->offset.y(), min_y, max_y));
    } else
    {
        offset.setY((height() - image_size.height()) / 2.0);
    }
}

