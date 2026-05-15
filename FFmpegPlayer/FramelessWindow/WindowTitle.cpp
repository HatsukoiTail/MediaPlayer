#include "WindowTitle.h"

#include <QDebug>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

WindowTitle::WindowTitle(QWidget *parent)
    : QWidget{parent}
{
    // 创建控件并设置布局
    this->layout = new QHBoxLayout(this);

    this->titleIcon = new QLabel(this);
    this->titleName = new QLabel(this);
    this->titleButtonLayout = new QHBoxLayout();

    this->minimizeButton = new QPushButton(this);
    this->shiftButton = new QPushButton(this);
    this->closeButton = new QPushButton(this);

    this->titleIcon->setText("🔴 🟡 🟢");
    this->titleName->setText("Windows");

    this->minimizeButton->setText("–");
    this->shiftButton->setText("▢");
    this->closeButton->setText("✕");

    this->titleButtonLayout->addWidget(this->minimizeButton);
    this->titleButtonLayout->addWidget(this->shiftButton);
    this->titleButtonLayout->addWidget(this->closeButton);

    this->layout->addWidget(this->titleIcon);
    this->layout->addStretch();
    this->layout->addLayout(this->titleButtonLayout);

    // 设置样式
    this->stylise();
    this->bind();
}

void WindowTitle::stylise()
{
    constexpr int icon_margin = 10;
    this->layout->setContentsMargins(icon_margin, 0, 0, 0);

    this->titleName->setAlignment(Qt::AlignCenter);
    this->titleName->setStyleSheet(R"(QLabel { font-family: Calibri;})");

    this->titleButtonLayout->setContentsMargins(0, 0, 0, 0);
    this->titleButtonLayout->setSpacing(0);

    constexpr auto buttonMinWidth = 40;
    constexpr auto buttonMinHeight = 30;
    this->minimizeButton->setMinimumWidth(buttonMinWidth);
    this->shiftButton->setMinimumWidth(buttonMinWidth);
    this->closeButton->setMinimumWidth(buttonMinWidth);

    this->minimizeButton->setMinimumHeight(buttonMinHeight);
    this->shiftButton->setMinimumHeight(buttonMinHeight);
    this->closeButton->setMinimumHeight(buttonMinHeight);

    const auto minimizeButtonStyle = "QPushButton {"
                                     "    background-color:transparent;"
                                     "    border:none;"
                                     "    border-radius:5px;"
                                     "    font-family:SimSun;"
                                     "    font-size:25px;"
                                     "}"
                                     "QPushButton:hover {"
                                     "    background-color:#C9D4D7;"
                                     "}"
                                     "QPushButton:pressed {"
                                     "    background-color:#CCD8DB;"
                                     "}";
    const auto shiftButtonStyle = "QPushButton {"
                                  "    background-color:transparent;"
                                  "    border:none;"
                                  "    border-radius:5px;"
                                  "    font-size:25px;"
                                  "}"
                                  "QPushButton:hover {"
                                  "    background-color:#C9D4D7;"
                                  "}"
                                  "QPushButton:pressed {"
                                  "    background-color:#CCD8DB;"
                                  "}";
    const auto closeButtonStyle = "QPushButton {"
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
                                  "}";
    this->minimizeButton->setStyleSheet(minimizeButtonStyle);
    this->shiftButton->setStyleSheet(shiftButtonStyle);
    this->closeButton->setStyleSheet(closeButtonStyle);
}

void WindowTitle::bind()
{
    connect(this->minimizeButton, &QPushButton::clicked, this, [this](){ emit this->minimize(); });
    connect(this->shiftButton, &QPushButton::clicked, this, [this](){ emit this->shift(); });
    connect(this->closeButton, &QPushButton::clicked, this, [this](){ emit this->quit(); });
}

void WindowTitle::resizeEvent(QResizeEvent *event)
{
    QSize titleNameSize = this->titleName->size();
    QSize parentSize = this->size();
    auto x = (parentSize.width() - titleNameSize.width()) / 2;
    auto y = (parentSize.height() - titleNameSize.height()) / 2;
    this->titleName->move(x, y);
}

void WindowTitle::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(232, 248, 244));
    painter.drawRect(this->rect());
}

void WindowTitle::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit this->shift();
    }
    QWidget::mouseDoubleClickEvent(event);
}
