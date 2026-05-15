#ifndef MEDIAINFOLIST_H
#define MEDIAINFOLIST_H

#include "Model.h"

#include <QString>
#include <QWidget>

#include <unordered_map>

enum class MediaState { Encrypted, Encrypting, Decrypting, Unencrypted };

struct MediaInfoSummary
{
    MediaState state;
    QString path;
    MetaType type;
    size_t size;
    size_t duration;
};

class MediaInfoList
{
public:
    void insert(QWidget* widget, MediaInfoSummary&& info);
    void remove(QWidget* widget);
    void clear();
    size_t size() const;
    size_t visibleSize() const;
    MediaInfoSummary& find(QWidget*);
    QWidget* find(const QString& path);
    QString last(const QString& path) const;
    QString next(const QString& path) const;
    std::vector<QString> list() const;
    std::vector<QString> list(MetaType type) const;
    void execute(std::function<void(QWidget*, MediaInfoSummary&)> executor);
    void sort(std::function<bool(const MediaInfoSummary&, const MediaInfoSummary&)>);
    void filter(const std::unordered_set<MetaType>& types);

private:
    const MediaInfoSummary& findInfo(const QString& path) const;

private:
    std::unordered_map<QWidget*, MediaInfoSummary> forward_map;
    std::unordered_map<QString, QWidget*> reverse_map;
    std::vector<QString> order_list;
};

#endif // MEDIAINFOLIST_H
