#include "MediaInfoCard.h"

#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

MediaInfoCard::MediaInfoCard(QWidget *parent)
    : QWidget{parent}
{
    auto layout = new QVBoxLayout(this);
    auto image_layout = new QHBoxLayout();
    auto bottom_layout = new QHBoxLayout();
    this->image_view = new ImageView(this);
    this->line_edit = new LineEdit(this);
    this->left_label = new QLabel(this);
    this->right_label = new QLabel(this);

    layout->addLayout(image_layout);
    layout->addWidget(this->line_edit);
    layout->addLayout(bottom_layout);
    image_layout->addWidget(this->image_view);
    bottom_layout->addWidget(this->left_label);
    bottom_layout->addStretch();
    bottom_layout->addWidget(this->right_label);

    layout->setContentsMargins(18, 18, 18, 18);
    this->image_view->setFixedSize(120, 80);

    QFont main_font;
    main_font.setBold(true);
    this->line_edit->setFont(main_font);
    QFont sub_font;
    sub_font.setPixelSize(10);
    this->left_label->setFont(sub_font);
    this->right_label->setFont(sub_font);
}

void MediaInfoCard::setImage(const QImage &image)
{
    this->image_view->setImage(image);
    this->image_view->fitToSize();
    this->image_view->update();
}

void MediaInfoCard::setMediaName(const QString &fileName)
{
    this->line_edit->setStyleSheet("QToolTip { background:white; }");
    this->line_edit->setToolTip(fileName);
    this->line_edit->setText(fileName.mid(fileName.lastIndexOf('/') + 1));
}

void MediaInfoCard::setLeftText(const QString &text)
{
    this->left_label->setText(text);
}

void MediaInfoCard::setRightText(const QString &text)
{
    this->right_label->setText(text);
}

void MediaInfoCard::setMediaType(const QString &type)
{
    if (!this->type_label)
    {
        this->type_label = new QLabel(this);
        this->type_label->setStyleSheet("QLabel {"
                                        "   background: rgba(109, 185, 209, 150);"
                                        "   font-size: 10px;"
                                        "   padding: 2px 4px;"
                                        "   border-radius: 4px;"
                                        "   color: #333333;"
                                        "}");
    }
    this->type_label->setText(type);
    auto x = this->image_view->x() + this->image_view->width() - this->type_label->sizeHint().width() - 5;
    auto y = this->image_view->y() + 5;
    this->type_label->setFixedSize(this->type_label->sizeHint());
    this->type_label->move(x, y);
}

void MediaInfoCard::setProgress(double value)
{
    if (!this->progress)
    {
        this->progress = new CircularProgressBar(this->image_view);
        this->progress->setFixedSize(40, 40);
        this->progress_layout = new QHBoxLayout(this->image_view);
        this->progress_layout->addWidget(this->progress);
    }
    this->progress->setValue(value);
}

void MediaInfoCard::hideProgress()
{
    if (!this->progress)
        return;
    this->progress_layout->removeWidget(this->progress);
    this->progress_layout->deleteLater();
    this->progress->deleteLater();
    this->progress = nullptr;
    this->progress_layout = nullptr;
}

void MediaInfoCard::paintEvent(QPaintEvent *event)
{
    const int radius = 10;        // 卡片圆角
    const int shadow_size = 10;    // 阴影模糊大小

    QRect r = rect().adjusted(shadow_size, shadow_size, -shadow_size, -shadow_size);

    auto shadow_image = this->draw_shadow_effect(r, shadow_size, radius);
    QPainter painter(this);
    painter.drawImage(0, 0, shadow_image);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::white);
    if (this->is_selected)
        painter.setPen(QPen(QColor(33, 150, 243), 2));
    else
        painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(r, radius, radius);
}

void MediaInfoCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        this->is_selected = !this->is_selected;
        update();
    }
    if (event->button() == Qt::RightButton)
    {
        emit this->requestPopupMenu(this);
    }
}

QImage MediaInfoCard::draw_shadow_effect(const QRect &rect, int shadow_size, int radius)
{
    QImage shadow_img(size(), QImage::Format_ARGB32_Premultiplied);
    shadow_img.fill(Qt::transparent);

    QPainter p(&shadow_img);
    p.setRenderHint(QPainter::Antialiasing);

    QColor shadowColor(0, 0, 0, 80);
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);

    p.fillPath(path, shadowColor);

    // 使用模糊滤镜
    QImage blurred(shadow_img.size(), QImage::Format_ARGB32_Premultiplied);
    blurred.fill(Qt::transparent);

    QGraphicsScene scene;

    auto *item = new QGraphicsPixmapItem(QPixmap::fromImage(shadow_img));
    auto *blur = new QGraphicsBlurEffect();

    blur->setBlurRadius(shadow_size);

    item->setGraphicsEffect(blur);
    scene.addItem(item);

    QImage res(shadow_img.size(), QImage::Format_ARGB32_Premultiplied);
    res.fill(Qt::transparent);
    QPainter painter(&res);

    scene.render(&painter);
    return res;
}
