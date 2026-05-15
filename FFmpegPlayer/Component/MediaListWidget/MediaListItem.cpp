#include "MediaListItem.h"

#include <QPainter>
#include <QPainterPath>

MediaListItem::MediaListItem(QWidget *parent)
    : QWidget{parent}
{
    this->layout = new QHBoxLayout(this);
    this->label = new QLabel(this);
    this->button = new QPushButton(this);

    this->layout->addWidget(this->label);
    this->layout->addStretch();
    this->layout->addWidget(this->button);

    this->button->setCursor(Qt::PointingHandCursor);
    this->button->setText("✕");
    this->button->setStyleSheet("QPushButton{"
                                "background:transparent;"
                                "font-size:16px;"
                                "border:none;"
                                "padding:2px;"
                                "}"
                                "QPushButton:hover{"
                                "color:red;"
                                "}");

    connect(this->button, &QPushButton::clicked, this, [this](){ emit this->closeButtonClicked(this); });
}

void MediaListItem::setText(const QString &text)
{
    this->label->setText(text.mid(text.lastIndexOf('/') + 1));
    this->label->setToolTip(text);
}

void MediaListItem::select()
{
    this->is_selected = true;
    this->update();
    emit this->selected(this);
}

void MediaListItem::unselect()
{
    this->is_selected = false;
    this->update();
    emit this->unselected(this);
}

void MediaListItem::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (this->is_selected)
        painter.setBrush(QColor(154, 222, 236)); // 选中颜色
    else if (this->is_hovered)
        painter.setBrush(QColor(224, 232, 239)); // 鼠标悬停颜色
    else
        painter.setBrush(QColor(221, 240, 248)); //默认颜色

    if (this->is_selected)
        painter.setPen(QPen(Qt::black, 0.5));
    else
        painter.setPen(Qt::NoPen);

    QRectF rect = this->rect();
    rect.adjust(0.5, 0.5, -0.5, -0.5);
    painter.drawRoundedRect(rect, 10, 10);
}

void MediaListItem::mouseReleaseEvent(QMouseEvent *event)
{
    this->select();
}

void MediaListItem::enterEvent(QEnterEvent *event)
{
    this->is_hovered = true;
    this->update();
}

void MediaListItem::leaveEvent(QEvent *event)
{
    this->is_hovered = false;
    this->update();
}
