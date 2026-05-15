#pragma once

#include "MediaPlayer/MediaPlayer.h"
#include "SimpleAudioRenderer.h"
#include "SimpleVideoRenderer.h"

#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void onOpen();
    void onPlay();
    void onPause();
    void onStop();
    void onSeek(int value);
    void onFrameReady(const QImage &image);

private:
    void updateState();

    MediaPlayer::MediaPlayer player;
    QLabel    *video_display;
    QPushButton *btn_open;
    QPushButton *btn_play;
    QPushButton *btn_pause;
    QPushButton *btn_stop;
    QSlider      *slider;

    SimpleAudioRenderer audio_renderer;
    SimpleVideoRenderer video_renderer;
};
