#include "MediaPreviewCard.h"

#include <QFileInfo>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

MediaPreviewCard::MediaPreviewCard(QWidget *parent)
    : QWidget{parent}
{
    this->image_view = new QLabel(this);
    this->name_view = new TextEdit(this);
    this->size_view = new QLabel(this);
    this->duration_view = new QLabel(this);

    this->main_layout = new QVBoxLayout(this);
    this->bottom_layout = new QHBoxLayout();

    this->main_layout->addWidget(this->image_view);
    this->main_layout->addWidget(this->name_view);
    this->main_layout->addLayout(this->bottom_layout);

    this->bottom_layout->addWidget(this->size_view);
    this->bottom_layout->addStretch();
    this->bottom_layout->addWidget(this->duration_view);

    this->main_layout->setContentsMargins(0, 0, 0, 0);
    this->name_view->setContentsMargins(5, 0, 10, 0);
    this->bottom_layout->setContentsMargins(10, 0, 13, 10);

    this->styled();
}

void MediaPreviewCard::setImage(const QPixmap &image)
{
    this->image = image;
    update();
}

void MediaPreviewCard::setName(const QString &name)
{
    this->name_view->setText(QFileInfo(name).fileName());
}

void MediaPreviewCard::setSize(const qint64 size)
{
    static const QList<QPair<qint64, QString>> units = {
        {qint64(1024 * 1024 * 1024), "GB"},
        {qint64(1024 * 1024), "MB"},
        {qint64(1024), "KB"},
        {qint64(1), "B"}
    };
    QString size_text;
    for (const auto& pair : units)
    {
        if (size < pair.first) continue;
        qreal value = static_cast<qreal>(size) / static_cast<qreal>(pair.first);
        value = qRound(value * 10.0) / 10.0;
        size_text = QString::number(value) + pair.second;
        break;
    }
    this->size_data = size;
    this->size_view->setText(size_text);
}

void MediaPreviewCard::setDuration(const qint64 duration)
{
    qint64 totalSeconds = duration / 1000;

    // 计算小时、分钟、秒
    qint64 hours = totalSeconds / 3600;
    qint64 remainingSeconds = totalSeconds % 3600;
    qint64 minutes = remainingSeconds / 60;
    qint64 seconds = remainingSeconds % 60;

    // 根据小时是否为0选择格式
    QString duration_text;
    if (hours > 0)
    {
        duration_text = QString("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))  // 小时补零到2位
            .arg(minutes, 2, 10, QLatin1Char('0')) // 分钟补零到2位
            .arg(seconds, 2, 10, QLatin1Char('0')); // 秒补零到2位
    }
    else
    {
        duration_text = QString("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0')) // 分钟补零到2位
            .arg(seconds, 2, 10, QLatin1Char('0')); // 秒补零到2位
    }
    this->duration_data = duration;
    this->duration_view->setText(duration_text);
}

QString MediaPreviewCard::name() const
{
    return this->name_view->text();
}

qint64 MediaPreviewCard::size() const
{
    return this->size_data;
}

qint64 MediaPreviewCard::duration() const
{
    return this->duration_data;
}

void MediaPreviewCard::styled()
{
    this->image_view->setFixedSize(150, 90);
    this->image_view->setAlignment(Qt::AlignCenter | Qt::AlignHCenter);

    QFont font;
    font.setFamilies({"Times New Roman", "宋体"});
    font.setPixelSize(14);
    font.setWeight(QFont::Weight::Bold);
    this->name_view->setFont(font);
    this->name_view->setLineCount(2);

    font.setPixelSize(11);
    font.setWeight(QFont::Light);
    this->size_view->setFont(font);
    this->duration_view->setFont(font);

    this->image_view->setCursor(Qt::PointingHandCursor);
}

void MediaPreviewCard::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    constexpr int shadow_offset = 4;
    constexpr int blur_radius = 4;
    constexpr int radius = 5;

    // 阴影区域（右下角偏移）
    QRect shadowRect = this->rect().adjusted(0, 0, -shadow_offset, -shadow_offset);

    QPainterPath shadowPath;
    shadowPath.addRoundedRect(shadowRect.translated(shadow_offset, shadow_offset), radius, radius);

    QColor shadowColor(0, 0, 0, 0.1 * 255);  // 黑色半透明阴影
    for (int i = 0; i < blur_radius; ++i)
    {
        QColor c = shadowColor;
        c.setAlphaF(shadowColor.alphaF() * (1.0 - (float)i / blur_radius));
        QPainterPath p;
        p.addRoundedRect(shadowRect.translated(shadow_offset - i, shadow_offset - i), radius + i, radius + i);
        painter.fillPath(p, c);
    }

    QPainterPath path;
    QRect MediaPreviewCard_rect = rect().adjusted(0, 0, -shadow_offset, -shadow_offset);
    path.addRoundedRect(MediaPreviewCard_rect, radius, radius);
    painter.setClipPath(path);
    painter.fillPath(path, Qt::white);
    if (!this->image.isNull())
    {
        const QRect image_rect = this->image_view->rect();
        QPixmap scaled_pixmap = this->image.scaled(image_rect.width(), image_rect.height(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        int x = (scaled_pixmap.width() - image_rect.width()) / 2;
        int y = (scaled_pixmap.height() - image_rect.height()) / 2;
        painter.drawPixmap(image_rect, scaled_pixmap, QRect(x, y, image_rect.width(), image_rect.height()));
    }
    QWidget::paintEvent(event);
}

void MediaPreviewCard::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() & Qt::LeftButton)
    {
        emit this->doubleClicked(this);
    }
}

void MediaPreviewCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
    {
        emit this->rightClicked(this);
    }
    QWidget::mouseReleaseEvent(event);
}

