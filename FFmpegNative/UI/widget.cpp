#include "widget.h"

#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <format>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("MediaPlayer Test");
    resize(1024, 700);

    auto *layout = new QVBoxLayout(this);

    video_display = new QLabel("No Video");
    video_display->setMinimumSize(960, 540);
    video_display->setAlignment(Qt::AlignCenter);
    video_display->setStyleSheet("background: black; color: white;");
    layout->addWidget(video_display);

    auto *btn_layout = new QHBoxLayout;
    btn_open  = new QPushButton("Open");
    btn_play  = new QPushButton("Play");
    btn_pause = new QPushButton("Pause");
    btn_stop  = new QPushButton("Stop");

    btn_layout->addWidget(btn_open);
    btn_layout->addWidget(btn_play);
    btn_layout->addWidget(btn_pause);
    btn_layout->addWidget(btn_stop);
    layout->addLayout(btn_layout);

    slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 1000);
    layout->addWidget(slider);

    connect(btn_open,  &QPushButton::clicked, this, &Widget::onOpen);
    connect(btn_play,  &QPushButton::clicked, this, &Widget::onPlay);
    connect(btn_pause, &QPushButton::clicked, this, &Widget::onPause);
    connect(btn_stop,  &QPushButton::clicked, this, &Widget::onStop);
    connect(slider,    &QSlider::sliderReleased, this, [this]() {
        double pos = slider->value() / 1000.0 * player.duration();
        onSeek((int)pos);
    });

    connect(&video_renderer, &SimpleVideoRenderer::frame_ready,
            this, &Widget::onFrameReady);

    // 进度更新定时器
    auto *prog_timer = new QTimer(this);
    connect(prog_timer, &QTimer::timeout, this, [this]() {
        if (player.state() == MediaPlayer::MediaPlayer::Playing) {
            double dur = player.duration();
            double pos = player.position();
            if (dur > 0)
                slider->setValue((int)(pos / dur * 1000));
        }
    });
    prog_timer->start(200);

    player.set_video_renderer(&video_renderer);
    player.set_audio_renderer(&audio_renderer);

    updateState();
}

Widget::~Widget()
{
    player.stop();
}

void Widget::onOpen()
{
    QString file = QFileDialog::getOpenFileName(this, "Open Media File");
    if (file.isEmpty()) return;

    player.stop();
    player.open(file.toUtf8().constData());
    updateState();
}

void Widget::onPlay()
{
    player.play();
    updateState();
}

void Widget::onPause()
{
    player.pause();
    updateState();
}

void Widget::onStop()
{
    player.stop();
    slider->setValue(0);
    video_display->setText("No Video");
    updateState();
}

void Widget::onSeek(int value)
{
    double pos = value / 1000.0 * player.duration();
    player.seek(pos);
    updateState();
}

void Widget::onFrameReady(const QImage &image)
{
    video_display->setPixmap(
        QPixmap::fromImage(image).scaled(
            video_display->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void Widget::updateState()
{
    bool playing = (player.state() == MediaPlayer::MediaPlayer::Playing);
    bool stopped = (player.state() == MediaPlayer::MediaPlayer::Stopped);

    btn_play->setEnabled(!playing);
    btn_pause->setEnabled(playing);
    btn_stop->setEnabled(!stopped);
    slider->setEnabled(!stopped);
}
