#include "IconButton.h"

#include <QPainter>

IconButton::IconButton(QWidget* parent)
    : QPushButton{parent}
{
    this->animation = new QPropertyAnimation(this, "scale");
    this->animation->setDuration(200);
    this->animation->setEasingCurve(QEasingCurve::OutQuad);
    this->setCursor(Qt::PointingHandCursor);
}

IconButton::IconButton(const QIcon &icon, QWidget *parent)
    : QPushButton{parent}
{
    this->icon = icon;
    this->animation = new QPropertyAnimation(this, "scale");
    this->animation->setDuration(200);
    this->animation->setEasingCurve(QEasingCurve::OutQuad);
    this->setCursor(Qt::PointingHandCursor);
}

void IconButton::setIcon(const QIcon& icon, const QSize& size)
{
    this->icon = icon;
    this->icon_size = size;
    update();
}

void IconButton::setAnimationDuration(int duration)
{
    this->animation->setDuration(duration);
}

qreal IconButton::scale() const
{
    return this->scale_rate;
}

void IconButton::setScale(qreal scale)
{
    this->scale_rate = scale;
    update();
}

void IconButton::startAnimation(qreal from, qreal to)
{
    this->animation->stop();
    this->animation->setStartValue(from);
    this->animation->setEndValue(to);
    this->animation->start();
}

void IconButton::enterEvent(QEnterEvent *event)
{
    this->startAnimation(this->scale_rate, 0.95);
}

void IconButton::leaveEvent(QEvent *event)
{
    this->startAnimation(this->scale_rate, 1.0);
}

void IconButton::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);
    auto size = this->icon_size * this->scale_rate;
    int x = (width() - size.width()) / 2;
    int y = (height() - size.height()) / 2;
    QPixmap pixmap = this->icon.pixmap(this->icon_size);
    painter.drawPixmap(QRect(x, y, size.width(), size.height()), pixmap);
}
