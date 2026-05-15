#include "MediaPreviewControlBar.h"

#include <QMenu>
#include <QMovie>
#include <QPainter>

MediaPreviewControlBar::MediaPreviewControlBar(QWidget *parent)
    : QWidget{parent}
{
    auto layout = new QHBoxLayout(this);
    auto button_layout = new QHBoxLayout();
    auto count_layout = new QHBoxLayout();
    this->filter_button = new QPushButton(this);
    this->sort_button = new QPushButton(this);
    this->count_label = new QLabel(this);
    this->loading_label = new QLabel(this);

    layout->addLayout(count_layout);
    layout->addStretch();
    layout->addLayout(button_layout);

    count_layout->addWidget(this->count_label);
    count_layout->addWidget(this->loading_label);

    button_layout->addWidget(this->filter_button);
    button_layout->addWidget(this->sort_button);

    layout->setContentsMargins(0, 0, 0, 0);
    count_layout->setContentsMargins(0, 0, 0, 0);
    button_layout->setContentsMargins(0, 0, 0, 0);
    button_layout->setSpacing(3);

    this->loading_label->setFixedSize(256 / 10, 160 / 10);
    this->loading_label->setScaledContents(true);
    QMovie* loading_movie = new QMovie(":/Loading", QByteArray(), this->loading_label);
    this->loading_label->setMovie(loading_movie);
    this->count_label->setText("文件数量： 0");

    this->filter_button->setIcon(QIcon(":/Filter"));
    this->filter_button->setIconSize({16, 16});
    this->sort_button->setIcon(QIcon(":/Sort"));
    this->sort_button->setIconSize({16, 16});

    connect(this->sort_button, &QPushButton::clicked, this, &MediaPreviewControlBar::on_sort_click);
    connect(this->filter_button, &QPushButton::clicked, this, &MediaPreviewControlBar::on_filter_click);

    this->loading_label->hide();

    this->type_list = { MetaType::Video, MetaType::Audio, MetaType::Image };
}

void MediaPreviewControlBar::showLoadIcon()
{
    this->loading_label->movie()->start();
    this->loading_label->show();
}

void MediaPreviewControlBar::hideLoadIcon()
{
    this->loading_label->movie()->stop();
    this->loading_label->hide();
}

void MediaPreviewControlBar::setFileCount(size_t count)
{
    this->count_label->setText("文件数量：" + QString::number(count));
}

void MediaPreviewControlBar::on_filter_click()
{
    auto create_dot_icon = [](const QColor &color = Qt::red) {
        QPixmap pixmap(8, 8);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(0, 0, 8, 8);
        return QIcon(pixmap);
    };

    QMenu *menu = new QMenu();

    QAction *video_action = menu->addAction("视频", this, [this]() {
        auto it = this->type_list.find(MetaType::Video);
        if (it == this->type_list.end())
            this->type_list.insert(MetaType::Video);
        else
            this->type_list.erase(it);
        emit this->requestFilter(this->type_list);
    });
    QAction *audio_action = menu->addAction("音频", this, [this]() {
        auto it = this->type_list.find(MetaType::Audio);
        if (it == this->type_list.end())
            this->type_list.insert(MetaType::Audio);
        else
            this->type_list.erase(it);
        emit this->requestFilter(this->type_list);
    });
    QAction *image_action = menu->addAction("图片", this, [this]() {
        auto it = this->type_list.find(MetaType::Image);
        if (it == this->type_list.end())
            this->type_list.insert(MetaType::Image);
        else
            this->type_list.erase(it);
        emit this->requestFilter(this->type_list);
    });

    if (this->type_list.find(MetaType::Video) != this->type_list.end())
        video_action->setIcon(create_dot_icon());
    if (this->type_list.find(MetaType::Audio) != this->type_list.end())
        audio_action->setIcon(create_dot_icon());
    if (this->type_list.find(MetaType::Image) != this->type_list.end())
        image_action->setIcon(create_dot_icon());

    menu->setAttribute(Qt::WA_DeleteOnClose);

    QRect button_rect = this->filter_button->rect();
    QPoint bottom_center = QPoint(button_rect.width() / 2, button_rect.height());
    QPoint global_pos = this->sort_button->mapToGlobal(bottom_center);
    menu->popup(global_pos - QPoint(menu->sizeHint().width() / 2, 0));
}

void MediaPreviewControlBar::on_sort_click()
{
    // 创建圆点图标（用于当前选中项）
    auto create_dot_icon = [](const QColor &color = Qt::red) {
        QPixmap pixmap(8, 8);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(0, 0, 8, 8);
        return QIcon(pixmap);
    };

    QMenu *menu = new QMenu(this);

    // --- 文件排序方式 ---
    QAction *name_action = menu->addAction("名称", this, [this]() {
        this->criteria = SortingCriteria::FileName;
        emit this->requestSort(this->criteria, this->order);
    });
    QAction *size_action = menu->addAction("大小", this, [this]() {
        this->criteria = SortingCriteria::FileSize;
        emit this->requestSort(this->criteria, this->order);
    });
    QAction *duration_action = menu->addAction("时长", this, [this]() {
        this->criteria = SortingCriteria::Duration;
        emit this->requestSort(this->criteria, this->order);
    });

    // 根据当前 criteria 给对应项打红点
    switch (this->criteria) {
    case SortingCriteria::FileName:
        name_action->setIcon(create_dot_icon());
        break;
    case SortingCriteria::FileSize:
        size_action->setIcon(create_dot_icon());
        break;
    case SortingCriteria::Duration:
        duration_action->setIcon(create_dot_icon());
        break;
    default:
        break;
    }

    menu->addSeparator();

    // --- 升降序 ---
    QAction *asc_action = menu->addAction("升序", this, [this]() {
        this->order = SortingOrder::Ascending;
        emit this->requestSort(this->criteria, this->order);
    });
    QAction *desc_action = menu->addAction("降序", this, [this]() {
        this->order = SortingOrder::Descending;
        emit this->requestSort(this->criteria, this->order);
    });

    if (this->order == SortingOrder::Ascending)
        asc_action->setIcon(create_dot_icon());
    else
        desc_action->setIcon(create_dot_icon());


    menu->setAttribute(Qt::WA_DeleteOnClose);

    QRect button_rect = this->sort_button->rect();
    QPoint bottom_center = QPoint(button_rect.width() / 2, button_rect.height());
    QPoint global_pos = this->sort_button->mapToGlobal(bottom_center);
    menu->popup(global_pos - QPoint(menu->sizeHint().width() / 2, 0));
}
