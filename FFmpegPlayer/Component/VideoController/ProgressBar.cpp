#include "ProgressBar.h"

ProgressBar::ProgressBar(QWidget *parent)
    : QWidget{parent}
{
    auto layout = new QHBoxLayout(this);
    this->progress_bar = new QSlider(Qt::Horizontal, this);
    layout->addWidget(this->progress_bar);

    QString style = R"(
        QSlider::groove:horizontal {
            height: 3px;
            background: rgba(176, 176, 176, 0.8);
            border-radius: 1px;
        }

        QSlider::handle:horizontal {
            background: #FFFFFF;
            border: 2px solid #2196F3;
            width: 12px;
            height: 12px;
            margin: -6px 0;
            border-radius: 6px;
        }

        QSlider::sub-page:horizontal {
            background: rgba(41, 110, 243, 0.7);
            height: 3px;
            border-radius: 1px;
        }
    )";
    this->progress_bar->setStyleSheet(style);
    this->progress_bar->setCursor(Qt::PointingHandCursor);

    connect(this->progress_bar, &QSlider::sliderPressed, this, [this](){
        emit this->readySeek();
    });
    connect(this->progress_bar, &QSlider::sliderReleased, this, [this](){
        auto value = this->progress_bar->value();
        emit this->seek(value / 100.0);
    });
}

void ProgressBar::setProgress(double rate)
{
    this->progress_bar->setValue(rate * 100);
}

void ProgressBar::enterEvent(QEnterEvent *event)
{
    emit this->mouseHover(true);
    QWidget::enterEvent(event);
}

void ProgressBar::leaveEvent(QEvent *event)
{
    emit this->mouseHover(false);
    QWidget::leaveEvent(event);
}
