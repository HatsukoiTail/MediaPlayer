#include "PlayList.h"

#include <QPainter>

PlayList::PlayList(QWidget *parent)
    : QWidget{parent}
{
    this->layout = new QVBoxLayout(this);

    this->scroll_widget = new QWidget();
    this->scroll_layout = new QVBoxLayout(this->scroll_widget);
    this->scroll_area = new QScrollArea(this);

    this->layout->addWidget(this->scroll_area);
    this->scroll_area->setWidget(this->scroll_widget);
    this->scroll_area->setWidgetResizable(true);
    this->scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    this->layout->setContentsMargins(0, 0, 0, 0);
    this->scroll_layout->setContentsMargins(0, 0, 0, 0);
    this->scroll_layout->setAlignment(Qt::AlignTop);
    this->scroll_layout->setSpacing(0);

    this->scroll_area->setStyleSheet("QScrollArea{ background:transparent; }");
    this->scroll_widget->setStyleSheet("background: transparent;");

    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
}

void PlayList::setList(const std::vector<QString>& list)
{
    for (const auto& item : list)
    {
        this->addItem(item);
    }
}

void PlayList::addItem(const QString &item)
{
    QPushButton* button = new QPushButton(this->scroll_widget);
    button->setText(item.mid(item.lastIndexOf('/') + 1));
    button->setStyleSheet("QPushButton{"
                          " background:transparent;"
                          " border:none;"
                          " padding:8px;"
                          " border-radius:5px;"
                          " text-align:left;"
                          " color:white;"
                          "}"
                          "QPushButton:hover{"
                          " background:rgba(87, 87, 87, 0.63);"
                          "}"
                          "QPushButton:pressed{"
                          " background:rgba(69, 69, 69, 0.63);"
                          "}");
    this->scroll_layout->addWidget(button);
    connect(button, &QPushButton::clicked, this, [this, button](){
        auto it = this->list.find(button);
        if (it == this->list.end())
            return;
        emit this->selected(it->second);
    });
    this->list.emplace(button, item);
}

void PlayList::removeItem(const QString &item)
{
    auto it = std::find_if(this->list.begin(), this->list.end(), [&item](const auto& pair){
        return pair.second == item;
    });

    if (it != this->list.end())
    {
        auto widget = it->first;
        this->layout->removeWidget(widget);
        widget->setParent(nullptr);
        widget->deleteLater();
        this->list.erase(it);
        return;
    }
}

void PlayList::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(24, 24, 24, 240));
    painter.drawRoundedRect(this->rect(), 5, 5);
}

void PlayList::enterEvent(QEnterEvent *event)
{
    emit this->mouseEnter();
    QWidget::enterEvent(event);
}
