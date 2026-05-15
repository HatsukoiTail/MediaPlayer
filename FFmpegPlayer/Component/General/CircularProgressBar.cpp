#include "CircularProgressBar.h"

#include <QPainter>

CircularProgressBar::CircularProgressBar(QWidget *parent)
    : QWidget{parent}
{}

void CircularProgressBar::setValue(double value)
{
    this->value = std::min(100.0, std::max(0.0, value));
    update();
}

void CircularProgressBar::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int size = qMin(w, h);

    int thickness = size * 0.10;    // 环宽（可调）
    QRectF rect(thickness, thickness, size - thickness * 2, size - thickness * 2);

    double startAngle = 90 * 16;    // Qt 的角度单位是 1/16°
    double spanAngle = -(this->value * 360.0 / 100.0) * 16;

    // 背景环
    QPen bgPen(QColor(200, 200, 200), thickness);
    painter.setPen(bgPen);
    painter.drawArc(rect, 0, 360 * 16);

    // 前景进度环
    QPen fgPen(QColor(80, 160, 255), thickness);
    painter.setPen(fgPen);
    painter.drawArc(rect, startAngle, spanAngle);

    // 中心文字
    painter.setPen(QPen(QColor(0, 2, 61)));
    painter.setFont(QFont("Microsoft YaHei", size * 0.18));
    painter.drawText(rect, Qt::AlignCenter, QString("%1%").arg(this->value * 100 / 100.0));
}
