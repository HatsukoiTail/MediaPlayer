#include "ControlTitleBar.h"

#include <QEvent>

ControlTitleBar::ControlTitleBar(QWidget *parent)
    : QWidget{parent}
{
    auto layout = new QHBoxLayout(this);
    this->button = new QPushButton(this);
    this->label = new QLabel(this);

    layout->addWidget(this->button);
    layout->addWidget(this->label);
    layout->addStretch();

    this->button->setText("↩");

    this->button->setStyleSheet("QPushButton {"
                                "   border: none;"
                                "   background: transparent;"
                                "   padding: 10px 5px;"
                                "   color: white;"
                                "   font-size: 30px;"
                                "}");
    this->button->setCursor(Qt::PointingHandCursor);
    this->label->setStyleSheet("QLabel {"
                               "   color: white;"
                               "   font-size: 14px;"
                               "}");

    layout->setContentsMargins(0, 0, 0, 0);

    connect(this->button, &QPushButton::clicked, this, [this](){ emit this->requestExit(); });

    this->label->setAttribute(Qt::WA_Hover);
    this->button->installEventFilter(this);
    this->label->installEventFilter(this);
}

void ControlTitleBar::setText(const QString &name)
{
    this->label->setText(name);
}

bool ControlTitleBar::eventFilter(QObject *obj, QEvent *event)
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
