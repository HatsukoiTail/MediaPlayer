#include "DialogBase.h"

#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>

enum class DialogBase::ResizeRegion
{
    None,
    Left, Right, Top, Bottom,
    TopLeft, TopRight, BottomLeft, BottomRight
};

DialogBase::DialogBase(QWidget* parent)
    : QDialog{parent}
{
    this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setMouseTracking(true);

    auto shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(this->shadow_size);
    shadow->setOffset(0, 0);
    shadow->setColor(QColor(0, 0, 0, 60));
    this->setGraphicsEffect(shadow);

    this->setMinimumSize(1, 1);
    this->setContentsMargins(this->contentsMargin());
}

void DialogBase::setTitleHeight(int height)
{
    this->title_height = height;
}

void DialogBase::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(255, 255, 255, 200));
    painter.setPen(Qt::NoPen);

    QRect rect = this->rect().adjusted(this->shadow_size, this->shadow_size, -this->shadow_size, -this->shadow_size);
    painter.drawRoundedRect(rect, this->corner_radius, this->corner_radius);
}

void DialogBase::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        // 判断是否进行窗口缩放
        auto region = this->locateMouseRegion(event->pos());
        if (region != ResizeRegion::None)
        {
            this->is_resizing = true;
            this->resize_region = region;
            this->start_point = event->globalPosition().toPoint();
            this->start_geometry = this->geometry();
            return;
        }
        // 计算是否进行窗口拖动
        auto pos = event->pos();
        if (pos.y() < this->shadow_size + this->resize_margin + this->title_height)
        {
            this->is_moving = true;
            this->start_point = event->globalPosition().toPoint();
            return;
        }
    }
}

void DialogBase::mouseMoveEvent(QMouseEvent *event)
{
    if (!this->is_resizing)
    {
        // 非拖动状态下更新鼠标形状
        auto region = this->locateMouseRegion(event->pos());
        this->setMouseShape(region);

        if (this->is_moving)
            this->moveWindow(event->globalPosition().toPoint());
    }
    else
    {
        this->resizeWindow(event->globalPosition().toPoint());
    }
}

void DialogBase::mouseReleaseEvent(QMouseEvent *event)
{
    this->is_resizing = false;
    this->is_moving = false;
}

QRect DialogBase::contentsRect() const
{
    return this->rect().adjusted(this->shadow_size, this->shadow_size, -this->shadow_size, -this->shadow_size);
}

QMargins DialogBase::contentsMargin() const
{
    return QMargins(this->shadow_size, this->shadow_size, this->shadow_size, this->shadow_size);
}

int DialogBase::cornerRadius() const
{
    return this->corner_radius;
}

void DialogBase::setMouseShape(ResizeRegion region)
{
    switch (region) {
    case ResizeRegion::Left:
        this->setCursor(QCursor(Qt::SizeHorCursor));
        break;
    case ResizeRegion::Right:
        this->setCursor(QCursor(Qt::SizeHorCursor));
        break;
    case ResizeRegion::Top:
        this->setCursor(QCursor(Qt::SizeVerCursor));
        break;
    case ResizeRegion::Bottom:
        this->setCursor(QCursor(Qt::SizeVerCursor));
        break;
    case ResizeRegion::TopLeft:
        this->setCursor(QCursor(Qt::SizeFDiagCursor));
        break;
    case ResizeRegion::BottomRight:
        this->setCursor(QCursor(Qt::SizeFDiagCursor));
        break;
    case ResizeRegion::TopRight:
        this->setCursor(QCursor(Qt::SizeBDiagCursor));
        break;
    case ResizeRegion::BottomLeft:
        this->setCursor(QCursor(Qt::SizeBDiagCursor));
        break;
    default:
        this->setCursor(QCursor(Qt::ArrowCursor));
        break;
    }
}

void DialogBase::resizeWindow(const QPoint &point)
{
    // delta：鼠标相对于起始点的偏移
    QPoint delta = point - this->start_point;
    QRect g = this->start_geometry;

    int left = g.left();
    int right = g.right();
    int top = g.top();
    int bottom = g.bottom();

    // 根据 resize_region 计算新的边（只改动对应的一侧）
    switch (this->resize_region) {
    case ResizeRegion::Left:
        left += delta.x();
        break;
    case ResizeRegion::Right:
        right += delta.x();
        break;
    case ResizeRegion::Top:
        top += delta.y();
        break;
    case ResizeRegion::Bottom:
        bottom += delta.y();
        break;
    case ResizeRegion::TopLeft:
        left += delta.x();
        top  += delta.y();
        break;
    case ResizeRegion::TopRight:
        right += delta.x();
        top   += delta.y();
        break;
    case ResizeRegion::BottomLeft:
        left  += delta.x();
        bottom += delta.y();
        break;
    case ResizeRegion::BottomRight:
        right  += delta.x();
        bottom += delta.y();
        break;
    default:
        break;
    }

    // 最小尺寸
    const int minW = qMax(1, this->minimumWidth());
    const int minH = qMax(1, this->minimumHeight());

    // 计算当前尝试的宽高
    int curW = right - left;
    int curH = bottom - top;

    // 判断哪一侧是“控制宽度/高度”的侧
    auto isLeftMoving = [this]() {
        return this->resize_region == ResizeRegion::Left
               || this->resize_region == ResizeRegion::TopLeft
               || this->resize_region == ResizeRegion::BottomLeft;
    };
    auto isTopMoving = [this]() {
        return this->resize_region == ResizeRegion::Top
               || this->resize_region == ResizeRegion::TopLeft
               || this->resize_region == ResizeRegion::TopRight;
    };

    // 修正宽度：如果宽度不足，按是哪一侧在移动来决定修正哪边
    if (curW < minW) {
        if (isLeftMoving()) {
            // 保持右边不动，向右收拢左边
            left = right - minW;
        } else {
            // 左边不动，向右推进右边
            right = left + minW;
        }
        curW = right - left;
    }

    // 修正高度：如果高度不足，按是哪一侧在移动来决定修正哪边
    if (curH < minH) {
        if (isTopMoving()) {
            // 保持底边不动，向下收拢上边
            top = bottom - minH;
        } else {
            // 上边不动，向下推进底边
            bottom = top + minH;
        }
        curH = bottom - top;
    }

    // 最终防御性约束（避免负宽高）
    if (curW <= 0)
        curW = minW;
    if (curH <= 0)
        curH = minH;

    this->move(left, top);
    this->resize(curW, curH);
}

void DialogBase::moveWindow(const QPoint &point)
{
    auto delta = point - this->start_point;
    this->start_point = point;
    auto x = this->x();
    auto y = this->y();
    this->move(x + delta.x(), y + delta.y());
}

DialogBase::ResizeRegion DialogBase::locateMouseRegion(const QPoint &pos)
{
    const int x = pos.x();
    const int y = pos.y();
    const int w = this->width();
    const int h = this->height();

    int margin = this->shadow_size + this->resize_margin;

    bool on_left = x < margin;
    bool on_right = x > w - margin;
    bool on_top = y < margin;
    bool on_bottom = y > h - margin;

    if (on_left && on_top) return ResizeRegion::TopLeft;
    if (on_right && on_top) return ResizeRegion::TopRight;
    if (on_left && on_bottom) return ResizeRegion::BottomLeft;
    if (on_right && on_bottom) return ResizeRegion::BottomRight;
    if (on_left) return ResizeRegion::Left;
    if (on_right) return ResizeRegion::Right;
    if (on_top) return ResizeRegion::Top;
    if (on_bottom) return ResizeRegion::Bottom;

    return ResizeRegion::None;
}
