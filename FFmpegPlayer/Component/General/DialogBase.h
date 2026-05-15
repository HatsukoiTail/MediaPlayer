#ifndef DIALOGBASE_H
#define DIALOGBASE_H

#include <QDialog>

class DialogBase : public QDialog
{
    enum class ResizeRegion;
public:
    DialogBase(QWidget* parent = nullptr);
    void setTitleHeight(int height);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    QRect contentsRect() const;
    QMargins contentsMargin() const;
    int cornerRadius() const;

private:
    ResizeRegion locateMouseRegion(const QPoint& pos);
    void setMouseShape(ResizeRegion region);
    void resizeWindow(const QPoint& point);
    void moveWindow(const QPoint& point);

private:
    int title_height = 20;
    bool is_moving = false;

private:
    int corner_radius = 10;
    int shadow_size = 20;
    int resize_margin = 5;
    ResizeRegion resize_region; // 普通模式下鼠标所在区域
    bool is_resizing = false; // 鼠标按下&&鼠标处于缩放区
    QPoint start_point; // 鼠标按下&&鼠标处于缩放区 起始的鼠标坐标
    QRect start_geometry; // 鼠标按下&&鼠标处于缩放区 起始的窗口区域
    bool pending_update = false;
    QRect pending_geometry;
};

#endif // DIALOGBASE_H
