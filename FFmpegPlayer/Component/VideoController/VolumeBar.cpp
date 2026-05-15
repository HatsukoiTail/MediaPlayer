#include "VolumeBar.h"

#include <QPainter>

VolumeBar::VolumeBar(QWidget *parent)
    : QWidget{parent}
{
    this->layout = new QVBoxLayout(this);
    this->label = new QLabel(this);
    this->slider = new QSlider(this);

    QHBoxLayout* label_layout = new QHBoxLayout();

    this->layout->addLayout(label_layout);
    this->layout->addWidget(this->slider);
    label_layout->addWidget(this->label);

    this->layout->setAlignment(Qt::AlignHCenter);
    label_layout->setAlignment(Qt::AlignHCenter);

    this->slider->setRange(0, 100);
    this->slider->setValue(100);
    this->label->setText("100");
    this->label->setStyleSheet("QLabel {"
                               "    color: white;"
                               "    text-align: center;"
                               "}");

    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    connect(this->slider, &QSlider::sliderMoved, this, &VolumeBar::on_slider_change);
}

void VolumeBar::on_slider_change(int value)
{
    this->label->setText(QString::number(value));
    emit this->volumeChange(value);
}

void VolumeBar::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(24, 24, 24, 240));
    painter.drawRoundedRect(this->rect(), 5, 5);
}

void VolumeBar::enterEvent(QEnterEvent *event)
{
    emit this->mouseEnter();
    QWidget::enterEvent(event);
}

void VolumeBar::leaveEvent(QEvent *event)
{
    emit this->mouseLeave();
    QWidget::leaveEvent(event);
}
