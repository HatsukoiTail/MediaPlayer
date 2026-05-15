#include "DialogTitle.h"

#include "Util.h"

#include <QPainter>

DialogTitle::DialogTitle(QWidget *parent)
    : QWidget{parent}
{
    this->layout = new QHBoxLayout(this);
    this->title_icon = new QLabel(this);
    this->title_name = new QLabel(this);
    this->close_button = new QPushButton(this);

    this->layout->addWidget(this->title_icon);
    this->layout->addStretch();
    this->layout->addWidget(this->close_button);

    this->layout->setContentsMargins(0, 0, 0, 0);

    this->title_icon->setText("   🔴🟡🟢");
    this->close_button->setText("✕");
    this->close_button->setStyleSheet("QPushButton {"
                                      "    background-color:transparent;"
                                      "    border:none;"
                                      "    border-radius:5px;"
                                      "    font-size:14px;"
                                      "}"
                                      "QPushButton:hover {"
                                      "    background-color:#C42B1C;"
                                      "}"
                                      "QPushButton:pressed {"
                                      "    background-color:#C43E2F;"
                                      "}");
    this->close_button->setMinimumSize(40, 30);
    this->close_button->setCursor(Qt::ArrowCursor);

    this->setAttribute(Qt::WA_TranslucentBackground);

    connect(this->close_button, &QPushButton::clicked, this, [this](){ emit this->closeButtonClicked(); });
}

void DialogTitle::setTitleName(const QString &text)
{
    this->title_name->setText(text);
}

void DialogTitle::setCornerRadius(const int radius)
{
    this->radius = radius;
}

void DialogTitle::setBackgroundColor(const QColor &color)
{
    this->background_color = color;
}

void DialogTitle::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing);
    // QPainterPath path = drawRoundRectPath(this->rect(), {this->radius, this->radius, 0, 0});
    // painter.fillPath(path, this->background_color);
    painter.setBrush(this->background_color);
    painter.drawRect(this->rect());
}

void DialogTitle::resizeEvent(QResizeEvent *event)
{
    QSize name_size = this->title_name->size();
    QSize parentSize = this->size();
    auto x = (parentSize.width() - name_size.width()) / 2;
    auto y = (parentSize.height() - name_size.height()) / 2;
    this->title_name->move(x, y);
}
