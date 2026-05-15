#include "MediaListWidget.h"

#include "MediaListItem.h"

#include <QDir>
#include <QFileDialog>
#include <QPainter>

#include <cassert>

MediaListWidget::MediaListWidget(QWidget* parent)
    : QWidget{parent}
{
    this->layout = new QVBoxLayout(this);
    this->scroll_area = new QScrollArea(this);
    this->scroll_widget = new QWidget();
    this->scroll_layout = new QVBoxLayout(this->scroll_widget);
    this->divider = new QFrame(this);
    this->new_button_layout = new QHBoxLayout();
    this->new_button = new QPushButton(this);

    this->scroll_area->setWidget(this->scroll_widget);
    this->layout->addWidget(this->scroll_area);
    this->layout->addWidget(this->divider);
    this->layout->addLayout(this->new_button_layout);
    this->new_button_layout->addWidget(this->new_button);

    this->stylise();

    connect(this->new_button, &QPushButton::clicked, this, &MediaListWidget::open_folder);
}

void MediaListWidget::add_item(const QString &path)
{
    const auto result = this->list.find(path);
    if (result != this->list.end())
        return;

    auto item = new MediaListItem(this->scroll_widget);
    item->setText(path);
    this->scroll_layout->addWidget(item);
    this->list.insert(path, item);

    // 绑定事件
    connect(item, &MediaListItem::closeButtonClicked, this, [this](QWidget* widget){
        QMapIterator<QString, QWidget*> it(this->list);
        while (it.hasNext())
        {
            it.next();
            if (it.value() == widget)
            {
                emit this->toRemove(it.key());
            }
        }
    });
    connect(item, &MediaListItem::selected, this, [this](QWidget* widget){
        if (this->selected_widget == widget)
            return;
        if (this->selected_widget)
        {
            auto item = qobject_cast<MediaListItem*>(this->selected_widget);
            item->unselect();
        }
        this->selected_widget = widget;
        QMapIterator<QString, QWidget*> it(this->list);
        while (it.hasNext())
        {
            it.next();
            if (it.value() == widget)
            {
                emit this->selected(it.key());
            }
        }
    });
    item->select();
}

void MediaListWidget::open_folder()
{
    const QString select_dir = QFileDialog::getExistingDirectory(this, tr("选择文件夹"), "../../", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (select_dir.isEmpty())
        return;
    this->add_item(select_dir);
}

void MediaListWidget::remove(const QString &path)
{
    auto widget = this->list.take(path);
    if (!widget)
        return;
    if (this->selected_widget == widget)
        this->selected_widget = nullptr;
    this->scroll_layout->removeWidget(widget);
    widget->setParent(nullptr);
    widget->deleteLater();
}

void MediaListWidget::stylise()
{
    this->scroll_layout->setContentsMargins(0, 0, 0, 0);
    this->scroll_layout->setAlignment(Qt::AlignTop);
    this->scroll_layout->setSpacing(2);

    this->scroll_area->setWidgetResizable(true);
    this->scroll_area->setFrameShape(QFrame::NoFrame);
    this->scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    this->divider->setFrameShape(QFrame::HLine);
    this->divider->setFrameShadow(QFrame::Plain);
    this->divider->setLineWidth(1);
    this->divider->setStyleSheet("color: #6B6B6B;");
    this->divider->setContentsMargins(5, 0, 5, 0);

    this->new_button->setText("+");
    this->new_button->setFixedSize(30, 30);
    this->new_button->setStyleSheet("QPushButton{"
                                    "   border:none;"
                                    "   font-size:24px;"
                                    "   border-radius:5px;"
                                    "   text-align: center;"
                                    "   padding: 0px;"
                                    "   margin: 0px;"
                                    "}"
                                    "QPushButton:hover{"
                                    "   background:#85CBF5;"
                                    "}"
                                    "QPushButton:pressed{"
                                    "   background:#CCD8DB;"
                                    "}");
    this->new_button->setCursor(Qt::PointingHandCursor);
}

void MediaListWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(this->rect(), QColor(252, 252, 252));
}
