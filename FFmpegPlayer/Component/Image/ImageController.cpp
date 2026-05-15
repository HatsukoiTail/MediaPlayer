#include "ImageController.h"

#include <QMouseEvent>

ImageController::ImageController(QWidget *parent)
    : QWidget{parent}
{
    auto layout = new QGridLayout(this);

    auto content_widget = new QWidget(this);
    auto content_layout = new QHBoxLayout(content_widget);
    this->last_button = new QPushButton(content_widget);
    this->next_button = new QPushButton(content_widget);

    this->title = new ControlTitleBar(this);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(content_widget, 0, 0);
    layout->addWidget(this->title, 0, 0, Qt::AlignTop | Qt::AlignLeft);

    content_layout->addWidget(this->last_button);
    content_layout->addStretch();
    content_layout->addWidget(this->next_button);

    content_layout->setContentsMargins(0, 0, 0, 0);

    this->last_button->setIcon(QIcon(":/Last"));
    this->next_button->setIcon(QIcon(":/Next"));

    const auto style = "QPushButton {"
                       "    background: rgba(207, 207, 207, 200);"
                       "    border: none;"
                       "    width: 30px;"
                       "    height: 30px;"
                       "    border-radius: 15px;"
                       "}"
                       "QPushButton:hover {"
                       "    background: rgba(167, 167, 167, 200);"
                       "}";
    this->last_button->setStyleSheet(style);
    this->next_button->setStyleSheet(style);
    this->last_button->setCursor(Qt::PointingHandCursor);
    this->next_button->setCursor(Qt::PointingHandCursor);

    this->bind();
}

void ImageController::setText(const QString &text)
{
    this->title->setText(text);
}

void ImageController::bind()
{
    connect(this->title, &ControlTitleBar::requestExit, this, [this]{ emit this->requestExit(); });
    connect(this->last_button, &QPushButton::clicked, this, [this]{ emit this->requestLast(); });
    connect(this->next_button, &QPushButton::clicked, this, [this]{ emit this->requestNext(); });
    connect(this->title, &ControlTitleBar::mouseHover, this, [this](bool hover){ emit this->mouseHover(hover); });
    this->last_button->installEventFilter(this);
    this->next_button->installEventFilter(this);
}

bool ImageController::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type()) {
    case QEvent::HoverEnter:
        emit this->mouseHover(true);
        break;
    case QEvent::HoverLeave:
        emit this->mouseHover(false);
        break;
    default:
        break;
    }
    return false;
}
