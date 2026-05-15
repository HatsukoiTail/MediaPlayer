#ifndef MEDIAPREVIEWAREA_H
#define MEDIAPREVIEWAREA_H

#include "FlowLayout.h"

#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollEvent>
#include <QWidget>

using LoadMethod = std::function<void(QWidget*)>;
using UnLoadMethod = std::function<void(QWidget*)>;

class MediaPreviewArea : public QWidget
{
    Q_OBJECT
public:
    explicit MediaPreviewArea(QWidget *parent = nullptr);
    void addItem(QWidget* item, LoadMethod load, UnLoadMethod unload);
    void removeItem(QWidget* item);
    void clear();
    bool isVisible(QWidget* widget);
    void flush();

public slots:
    void sort(std::function<bool(QWidget*, QWidget*)> criteria);

signals:
    void itemHiden(QWidget* path);

private:
    struct Item;
    void bind();
    void dynamic_load(Item& item);
    void dynamic_load();

private:
    void resizeEvent(QResizeEvent* event) override;

private:
    std::vector<Item> items;

private:
    QHBoxLayout* layout;
    QScrollArea* scroll_area;
    QWidget* scroll_widget;
    FlowLayout* flow_layout;
};

struct MediaPreviewArea::Item
{
    QWidget* widget;
    LoadMethod load;
    UnLoadMethod unload;
    bool loaded;
};

#endif // MEDIAPREVIEWAREA_H
