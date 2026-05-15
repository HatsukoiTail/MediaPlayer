#include "SpeedMenu.h"

#include <QPainter>

SpeedMenu::SpeedMenu(QWidget *parent)
    : QWidget{parent}
{
    this->layout = new QVBoxLayout(this);
    this->layout->setContentsMargins(0, 0, 0, 0);
    this->layout->setSpacing(0);
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(80);

    this->addSpeedItem("2.0x");
    this->addSpeedItem("1.5x");
    this->addSpeedItem("1.25x");
    this->addSpeedItem("1.0x");
    this->addSpeedItem("0.75x");
    this->addSpeedItem("0.5x");
}

void SpeedMenu::addSpeedItem(const QString& speed_str)
{
    QPushButton* button = new QPushButton(this);
    button->setText(speed_str);
    button->setStyleSheet("QPushButton{"
                          " background:transparent;"
                          " border:none;"
                          " padding:8px;"
                          " border-radius:5px;"
                          " text-align:center;"
                          " color:white;"
                          "}"
                          "QPushButton:hover{"
                          " background:rgba(87, 87, 87, 0.63);"
                          "}"
                          "QPushButton:pressed{"
                          " background:rgba(69, 69, 69, 0.63);"
                          "}");
    connect(button, &QPushButton::clicked, this, [this, button]{
        auto str = button->text();
        double speed = str.left(str.length() - 1).toDouble();
        emit this->speedSelected(speed);
    });
    this->layout->addWidget(button);
}

void SpeedMenu::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(24, 24, 24, 240));
    painter.drawRoundedRect(this->rect(), 5, 5);
}

void SpeedMenu::enterEvent(QEnterEvent *event)
{
    emit this->mouseEnter();
    QWidget::enterEvent(event);
}

void SpeedMenu::leaveEvent(QEvent *event)
{
    emit this->mouseLeave();
    QWidget::leaveEvent(event);
}
