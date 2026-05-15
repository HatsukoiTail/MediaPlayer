#ifndef VIDEOCONTROLBAR_H
#define VIDEOCONTROLBAR_H

#include "IconButton.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

class VideoControlBar : public QWidget
{
    Q_OBJECT
public:
    explicit VideoControlBar(QWidget *parent = nullptr);
    void setTime(int64_t cur, int64_t total);
    void setVideoList(const std::vector<QString>& list);
    void setPlayState(bool playing);

signals:
    void requestVideoList();
    void switchToPlay(const QString& path);
    void requestPlay();
    void requestPause();
    void volumeChange(int);
    void speedChange(double);
    void requestFullScreen(bool);
    void mouseEnter();
    void mouseLeave();
    void mouseHover(bool);

private:
    void stylise();
    void bind();
    void on_play_button_click();
    void on_play_list_select(const QString& path);
    void on_volume_button_click();
    void on_speed_button_click();
    void on_fullscreen_button_click();
    void update_hover(bool);

private:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    bool is_pause {false};
    bool is_fullscreen {false};
    bool is_hovering {false};

private:
    // 左侧组件
    IconButton* play_button;
    QLabel* time_label;
    // 右侧组件
    QPushButton* play_list_button;
    QPushButton* speed_button;
    QPushButton* volume_button;
    QPushButton* screen_button;
};

#endif // VIDEOCONTROLBAR_H
