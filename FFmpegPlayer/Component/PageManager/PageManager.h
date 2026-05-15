#ifndef PAGEMANAGER_H
#define PAGEMANAGER_H

#include <QStackedLayout>
#include <QWidget>

class PageManager : public QWidget
{
    Q_OBJECT
public:
    explicit PageManager(QWidget *parent = nullptr);
    void addPage(const QString& id, std::function<QWidget*()> creater);
    void removePage(const QString& id);
    void swithToPage(const QString& id);
    QWidget* widget(const QString& id);
    QString currentPageId() const;

private:
    QStackedLayout* layout;
    std::unordered_map<QString, QWidget*> pages;
    QString current_page_id;
    QWidget* null_widget;
};

#endif // PAGEMANAGER_H
