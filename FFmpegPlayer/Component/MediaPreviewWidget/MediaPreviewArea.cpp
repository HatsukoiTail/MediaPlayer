#include "MediaPreviewArea.h"

#include <QGraphicsItem>
#include <QScrollBar>

MediaPreviewArea::MediaPreviewArea(QWidget *parent)
    : QWidget{parent}
{
    this->layout = new QHBoxLayout(this);
    this->scroll_area = new QScrollArea(this);
    this->scroll_widget = new QWidget();
    this->flow_layout = new FlowLayout(this->scroll_widget);

    this->layout->addWidget(this->scroll_area);
    this->scroll_area->setWidget(this->scroll_widget);

    this->layout->setContentsMargins(0, 0, 0, 0);
    this->scroll_area->setContentsMargins(0, 0, 0, 0);
    this->flow_layout->setContentsMargins(0, 0, 0, 0);

    this->scroll_area->setWidgetResizable(true);

    this->scroll_area->setStyleSheet("QScrollArea{"
                                     "  background:transparent;"
                                     "  border:none;"
                                     "}");
    this->scroll_widget->setStyleSheet("background:transparent;");

    this->bind();
}

void MediaPreviewArea::addItem(QWidget *widget, LoadMethod load, UnLoadMethod unload)
{
    widget->setParent(this->scroll_widget);
    this->flow_layout->addWidget(widget);
    widget->show();

    auto item = Item{
        .widget = widget,
        .load = std::move(load),
        .unload = std::move(unload),
        .loaded = false
    };
    this->dynamic_load(item);
    this->items.emplace_back(std::move(item));
}

void MediaPreviewArea::removeItem(QWidget *widget)
{
    auto it = std::find_if(this->items.begin(), this->items.end(), [widget](const Item& item){ return item.widget == widget; });
    if (it == this->items.end())
        return;
    widget->setParent(nullptr);
    this->flow_layout->removeWidget(widget);
}

void MediaPreviewArea::clear()
{
    while (QLayoutItem* item = this->flow_layout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            widget->deleteLater();
        }
        delete item;
    }
    this->items.clear();
}

bool MediaPreviewArea::isVisible(QWidget *widget)
{
    if (!widget->isVisible())
        return false;

    QWidget* viewport = this->scroll_area->viewport();

    QRect local_rect = widget->rect();

    QPoint top_left = widget->mapTo(this->scroll_widget, local_rect.topLeft());
    QPoint bottom_right = widget->mapTo(this->scroll_widget, local_rect.bottomRight());
    QRect in_scroll = QRect(top_left, bottom_right);

    QPoint left_top = scroll_widget->mapTo(viewport, in_scroll.topLeft());
    QPoint right_bottom = scroll_widget->mapTo(viewport, in_scroll.bottomRight());
    QRect in_viewport = QRect(left_top, right_bottom);

    QRect view_rect = viewport->rect();

    return in_viewport.intersects(view_rect);
}

void MediaPreviewArea::flush()
{
    this->flow_layout->activate();
    this->dynamic_load();
}

void MediaPreviewArea::sort(std::function<bool (QWidget *, QWidget *)> criteria)
{
    assert(criteria && this->flow_layout);

    std::vector<QWidget*> widgets;
    widgets.reserve(this->items.size());

    for (const auto& item : this->items)
    {
        widgets.push_back(item.widget);
        this->flow_layout->removeWidget(item.widget);
    }
    std::sort(widgets.begin(), widgets.end(), criteria);

    for (auto* widget : widgets)
    {
        this->flow_layout->addWidget(widget);
    }

    this->flow_layout->activate();

    this->dynamic_load();
}

void MediaPreviewArea::bind()
{
    connect(this->scroll_area->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int){ this->dynamic_load(); });
}

void MediaPreviewArea::dynamic_load(Item &item)
{
    QWidget* widget = item.widget;
    bool visible = this->isVisible(widget);

    if (visible && !item.loaded)
    {
        item.load(widget);
        item.loaded = true;
    }
    else if (!visible && item.loaded)
    {
        item.unload(widget);
        item.loaded = false;
    }
}

void MediaPreviewArea::dynamic_load()
{
    for (auto& item : this->items)
        this->dynamic_load(item);
}

void MediaPreviewArea::resizeEvent(QResizeEvent *event)
{
    this->dynamic_load();
}
