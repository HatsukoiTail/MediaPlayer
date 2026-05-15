#include "MediaInfoList.h"

void MediaInfoList::insert(QWidget *widget, MediaInfoSummary &&info)
{
    this->forward_map.emplace(widget, info);
    this->reverse_map.emplace(info.path, widget);
    this->order_list.emplace_back(info.path);
}

void MediaInfoList::remove(QWidget *widget)
{
    auto forward_it = this->forward_map.find(widget);
    assert(forward_it != this->forward_map.end());
    const auto path = forward_it->second.path;
    this->forward_map.erase(forward_it);
    auto reverse_it = this->reverse_map.find(path);
    assert(reverse_it != this->reverse_map.end());
    this->reverse_map.erase(reverse_it);
    auto it = std::find(this->order_list.begin(), this->order_list.end(), path);
    if (it == this->order_list.end())
        return;
    this->order_list.erase(it);
}

void MediaInfoList::clear()
{
    this->forward_map.clear();
    this->reverse_map.clear();
    this->order_list.clear();
}

size_t MediaInfoList::visibleSize() const
{
    return this->order_list.size();
}

size_t MediaInfoList::size() const
{
    return this->forward_map.size();
}

MediaInfoSummary &MediaInfoList::find(QWidget *widget)
{
    auto it = this->forward_map.find(widget);
    assert(it != this->forward_map.end());
    return it->second;
}

QWidget *MediaInfoList::find(const QString &path)
{
    auto it = this->reverse_map.find(path);
    if (it == this->reverse_map.end())
        return {};
    return it->second;
}

QString MediaInfoList::last(const QString &path) const
{
    auto type = this->findInfo(path).type;
    auto it = std::find(this->order_list.begin(), this->order_list.end(), path);
    auto result_it = std::find_if(std::reverse_iterator<decltype(it)>(it),
                                  std::reverse_iterator<decltype(order_list.begin())>(order_list.begin()), [this, type](const auto& cur){
        auto info = this->findInfo(cur);
        return info.type == type;
    });
    if (result_it == this->order_list.rend())
        return {};
    return *result_it;
}

QString MediaInfoList::next(const QString &path) const
{
    auto type = this->findInfo(path).type;
    auto it = std::find(this->order_list.begin(), this->order_list.end(), path);
    auto result_it = std::find_if(++it, this->order_list.end(), [this, type](const auto& cur){
        auto info = this->findInfo(cur);
        if (info.type == type)
            return true;
        return false;
    });
    if (result_it == this->order_list.end())
        return {};
    return *result_it;
}

std::vector<QString> MediaInfoList::list() const
{
    return this->order_list;
}

std::vector<QString> MediaInfoList::list(MetaType type) const
{
    std::vector<QString> result;
    for (const auto& it : this->order_list)
    {
        const auto& info = this->findInfo(it);
        if (info.type == type)
            result.emplace_back(it);
    }
    return result;
}

void MediaInfoList::execute(std::function<void (QWidget *, MediaInfoSummary &)> executor)
{
    for (auto& [widget, info] : this->forward_map)
    {
        executor(widget, info);
    }
}

const MediaInfoSummary &MediaInfoList::findInfo(const QString &path) const
{
    auto reverse_it = this->reverse_map.find(path);
    assert(reverse_it != this->reverse_map.end());
    auto forward_it = this->forward_map.find(reverse_it->second);
    assert(forward_it != this->forward_map.end());
    return forward_it->second;
}

void MediaInfoList::sort(std::function<bool (const MediaInfoSummary &, const MediaInfoSummary &)> compare)
{
    std::sort(this->order_list.begin(), this->order_list.end(), [this, &compare](const auto& a, const auto& b){
        auto info1 = this->findInfo(a);
        auto info2 = this->findInfo(b);
        return compare(info1, info2);
    });
}

void MediaInfoList::filter(const std::unordered_set<MetaType> &types)
{
    std::vector<QString> result;
    for (const auto& [widget, info] : this->forward_map)
    {
        auto type = info.type;
        auto it = types.find(type);
        if (it != types.end())
        {
            result.push_back(info.path);
        }
    }
    this->order_list = result;
}
