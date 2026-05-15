#include "PageManager.h"

PageManager::PageManager(QWidget *parent)
    : QWidget{parent}
{
    this->layout = new QStackedLayout(this);
    this->null_widget = new QWidget(this);
    this->layout->addWidget(this->null_widget);
}

void PageManager::addPage(const QString &id, std::function<QWidget *()> creater)
{
    auto it = this->pages.find(id);
    if (it != this->pages.end())
        return;

    auto page = creater();
    page->setParent(this);
    this->layout->addWidget(page);
    this->pages.emplace(id, page);
}

void PageManager::removePage(const QString &id)
{
    auto it = this->pages.find(id);
    if (it == this->pages.end())
        return;

    auto page = it->second;
    this->layout->removeWidget(page);
    page->setParent(nullptr);
    page->deleteLater();
    this->pages.erase(it);
    if (this->current_page_id == id)
    {
        this->current_page_id = {};
        this->layout->setCurrentWidget(this->null_widget);
    }
}

void PageManager::swithToPage(const QString &id)
{
    auto it = this->pages.find(id);
    if (it == this->pages.end())
        return;
    auto page = it->second;
    this->current_page_id = id;
    this->layout->setCurrentWidget(page);
    page->show();
}

QWidget *PageManager::widget(const QString &id)
{
    auto it = this->pages.find(id);
    if (it == this->pages.end())
        return nullptr;
    return it->second;
}

QString PageManager::currentPageId() const
{
    return this->current_page_id;
}


