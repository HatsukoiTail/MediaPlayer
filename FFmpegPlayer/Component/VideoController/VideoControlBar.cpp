#include "VideoControlBar.h"

#include "PlayList.h"
#include "SpeedMenu.h"
#include "Tool.h"
#include "VolumeBar.h"

#include <QMouseEvent>

VideoControlBar::VideoControlBar(QWidget *parent)
    : QWidget{parent}
{
    auto layout = new QHBoxLayout(this);
    auto left_layout = new QHBoxLayout();
    auto right_layout = new QHBoxLayout();

    // 左侧包含播放按钮和时间
    this->play_button = new IconButton(this);
    this->time_label = new QLabel(this);

    // 右侧组件
    this->play_list_button = new QPushButton("播放列表", this);
    this->speed_button = new QPushButton("倍速", this);
    this->volume_button = new QPushButton(this);
    this->screen_button = new QPushButton(this);

    // 顶层为两端布局
    layout->addLayout(left_layout);
    layout->addStretch();
    layout->addLayout(right_layout);

    left_layout->addWidget(this->play_button);
    left_layout->addWidget(this->time_label);

    right_layout->addWidget(this->play_list_button);
    right_layout->addWidget(this->speed_button);
    right_layout->addWidget(this->volume_button);
    right_layout->addWidget(this->screen_button);

    left_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setContentsMargins(0, 0, 0, 0);

    this->stylise();
    this->bind();
}

void VideoControlBar::stylise()
{
    // 播放按钮样式
    this->play_button->setIcon(QIcon(":/Paused"), QSize(20, 20));
    this->play_button->setStyleSheet("QPushButton{"
                                     "  background:transparent;"
                                     "}");
    this->play_button->setAnimationDuration(200);
    this->play_button->setScale(0.95);

    this->time_label->setStyleSheet("QLabel { color:white; }");

    this->volume_button->setIcon(QIcon(":/Volume"));
    this->screen_button->setIcon(QIcon(":/FullScreen"));

    const auto style = "QPushButton {"
                       "    background: rgba(13, 13, 13, 50);"
                       "    color: rgba(227, 227, 227, 240);"
                       "    padding: 3px 5px;;"
                       "}"
                       "QPushButton:hover {"
                       "    background: rgba(56, 56, 56, 150);"
                       "}";
    this->play_list_button->setStyleSheet(style);
    this->speed_button->setStyleSheet(style);
    this->volume_button->setStyleSheet(style);
    this->screen_button->setStyleSheet(style);

    this->play_list_button->setFixedHeight(28);
    this->speed_button->setFixedHeight(28);
    this->volume_button->setFixedHeight(28);
    this->screen_button->setFixedHeight(28);

    this->play_button->setCursor(Qt::PointingHandCursor);
    this->play_list_button->setCursor(Qt::PointingHandCursor);
    this->speed_button->setCursor(Qt::PointingHandCursor);
    this->volume_button->setCursor(Qt::PointingHandCursor);
    this->screen_button->setCursor(Qt::PointingHandCursor);
}

void VideoControlBar::bind()
{
    connect(this->play_button, &QPushButton::clicked, this, &VideoControlBar::on_play_button_click);
    connect(this->play_list_button, &QPushButton::clicked, this, [this](){ emit this->requestVideoList(); });
    connect(this->volume_button, &QPushButton::clicked, this, &VideoControlBar::on_volume_button_click);
    connect(this->speed_button, &QPushButton::clicked, this, &VideoControlBar::on_speed_button_click);
    connect(this->screen_button, &QPushButton::clicked, this, &VideoControlBar::on_fullscreen_button_click);

    this->time_label->setAttribute(Qt::WA_Hover);

    this->play_button->installEventFilter(this);
    this->time_label->installEventFilter(this);
    this->play_list_button->installEventFilter(this);
    this->speed_button->installEventFilter(this);
    this->volume_button->installEventFilter(this);
    this->screen_button->installEventFilter(this);
}

void VideoControlBar::setTime(int64_t cur, int64_t total)
{
    this->time_label->setText(formatTime(cur) + " / " + formatTime(total));
}

void VideoControlBar::setVideoList(const std::vector<QString>& list)
{
    auto play_list = new PlayList(nullptr);
    play_list->setAttribute(Qt::WA_DeleteOnClose);

    play_list->setMinimumSize(180, 300);
    play_list->setMaximumSize(260, 450);
    play_list->setList(list);

    connect(play_list, &PlayList::selected, this, &VideoControlBar::on_play_list_select);
    connect(play_list, &PlayList::mouseEnter, this, [this]{ this->update_hover(true); });
    connect(play_list, &PlayList::destroyed, this, [this]{ this->update_hover(false); });

    play_list->show();
    QPoint pos = this->play_list_button->mapToGlobal(QPoint(0, 0));
    auto list_size = play_list->size();
    auto base_size = this->play_list_button->size();
    int x = pos.x() + (base_size.width() - list_size.width()) / 2;
    int y = pos.y() - list_size.height() - 10;
    play_list->move(x, y);
}

void VideoControlBar::setPlayState(bool playing)
{
    if (playing)
    {
        this->is_pause = false;
        this->play_button->setIcon(QIcon(":/Playing"), QSize(20, 20));
    }
    else
    {
        this->is_pause = true;
        this->play_button->setIcon(QIcon(":/Paused"), QSize(20, 20));
    }
}

void VideoControlBar::on_play_button_click()
{
    this->is_pause = !this->is_pause;
    if (this->is_pause)
    {
        this->play_button->setIcon(QIcon(":/Paused"), QSize(20, 20));
        emit this->requestPause();
    }
    else
    {
        this->play_button->setIcon(QIcon(":/Playing"), QSize(20, 20));
        emit this->requestPlay();
    }
}

void VideoControlBar::on_play_list_select(const QString &path)
{
    emit this->switchToPlay(path);
}

void VideoControlBar::on_volume_button_click()
{
    auto volume = new VolumeBar();
    volume->setFixedSize(50, 150);

    volume->setAttribute(Qt::WA_DeleteOnClose);
    connect(volume, &VolumeBar::volumeChange, this, [this](int value){ emit this->volumeChange(value); });
    connect(volume, &VolumeBar::mouseEnter, this, [this]{ this->update_hover(true); });
    connect(volume, &VolumeBar::destroyed, this, [this]{ this->update_hover(false); });

    volume->show();

    QPoint pos = this->volume_button->mapToGlobal(QPoint(0, 0));
    auto list_size = volume->size();
    auto base_size = this->volume_button->size();
    int x = pos.x() + (base_size.width() - list_size.width()) / 2;
    int y = pos.y() - list_size.height() - 10;
    volume->move(x, y);
}

void VideoControlBar::on_speed_button_click()
{
    auto speed_menu = new SpeedMenu();

    speed_menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(speed_menu, &SpeedMenu::speedSelected, this, [this](double value){ emit this->speedChange(value); });
    connect(speed_menu, &SpeedMenu::mouseEnter, this, [this]{ this->update_hover(true); });
    connect(speed_menu, &SpeedMenu::destroyed, this, [this]{ qDebug() << "des"; this->update_hover(false); });

    speed_menu->show();

    QPoint pos = this->speed_button->mapToGlobal(QPoint(0, 0));
    auto list_size = speed_menu->size();
    auto base_size = this->speed_button->size();
    int x = pos.x() + (base_size.width() - list_size.width()) / 2;
    int y = pos.y() - list_size.height() - 10;
    speed_menu->move(x, y);
}

void VideoControlBar::on_fullscreen_button_click()
{
    this->is_fullscreen = !this->is_fullscreen;
    emit this->requestFullScreen(this->is_fullscreen);
}

void VideoControlBar::update_hover(bool hover)
{
    if (this->is_hovering != hover)
    {
        this->is_hovering = hover;
        emit this->mouseHover(hover);
    }
}

bool VideoControlBar::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type()) {
    case QEvent::HoverEnter:
        this->update_hover(true);
        break;
    case QEvent::HoverLeave:
        this->update_hover(false);
    default:
        break;
    }
    return false;
}
