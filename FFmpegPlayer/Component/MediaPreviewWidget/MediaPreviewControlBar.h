#ifndef MEDIAPREVIEWCONTROLBAR_H
#define MEDIAPREVIEWCONTROLBAR_H

#include "Model.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

class MediaPreviewControlBar : public QWidget
{
    Q_OBJECT
public:
    explicit MediaPreviewControlBar(QWidget *parent = nullptr);

public:
    void showLoadIcon();
    void hideLoadIcon();
    void setFileCount(size_t count);

private:
    void on_sort_click();
    void on_filter_click();

signals:
    void requestSort(SortingCriteria criteria, SortingOrder order);
    void requestFilter(const std::unordered_set<MetaType>& type_list);

private:
    void stylise();

private:
    SortingCriteria criteria;
    SortingOrder order;
    std::unordered_set<MetaType> type_list;

private:
    QLabel* count_label;
    QLabel* loading_label;
    QPushButton* filter_button;
    QPushButton* sort_button;
};

#endif // MEDIAPREVIEWCONTROLBAR_H
