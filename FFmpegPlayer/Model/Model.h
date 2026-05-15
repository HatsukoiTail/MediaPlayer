#ifndef MODEL_H
#define MODEL_H

#include <QImage>
#include <QString>

enum class MetaType
{
    Unknown,
    Video,
    Audio,
    Image
};

struct MediaData
{
    bool isEncrypted;
    QString filePath;
    MetaType metaType;
    size_t fileSize;
    size_t duration;
    QImage image;

    MediaData() = default;
    MediaData(const MediaData& other);
    MediaData(MediaData&& other) noexcept;
    MediaData& operator=(const MediaData& other);
    MediaData& operator=(MediaData&& other) noexcept;
};

enum class SortingCriteria
{
    Default,
    FileName,
    FileSize,
    Duration,
    ModifiedTime
};

enum class SortingOrder
{
    Ascending,
    Descending
};

MetaType inferMetaType(std::string_view fileName);

std::string stringifyMetaType(MetaType type);

MetaType formatMetaType(std::string_view str);

#endif // MODEL_H
