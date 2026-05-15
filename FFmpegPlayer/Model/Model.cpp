#include "Model.h"

#include <cassert>
#include <filesystem>

MetaType inferMetaType(std::string_view fileName)
{
    const auto extend_name = std::filesystem::path(fileName).extension().string();

    if (extend_name == ".mp4")
        return MetaType::Video;
    else if (extend_name == ".mp3")
        return MetaType::Audio;
    else if (extend_name == ".jpg" || extend_name == ".png")
        return MetaType::Image;
    else
        return MetaType::Unknown;
}

MediaData::MediaData(const MediaData &other)
    : isEncrypted(other.isEncrypted), filePath(other.filePath), fileSize(other.fileSize), metaType(other.metaType), duration(other.duration)
{
    this->image = other.image.copy();
}

MediaData::MediaData(MediaData &&other) noexcept
    : isEncrypted(other.isEncrypted), filePath(std::move(other.filePath)), fileSize(other.fileSize), metaType(other.metaType), duration(other.duration)
{
    this->image = std::move(other.image);
}

MediaData &MediaData::operator=(const MediaData &other)
{
    this->isEncrypted = other.isEncrypted;
    this->filePath = other.filePath;
    this->fileSize = other.fileSize;
    this->metaType = other.metaType;
    this->duration = other.duration;
    this->image = other.image.copy();
    return *this;
}

MediaData &MediaData::operator=(MediaData&& other) noexcept
{
    this->isEncrypted = other.isEncrypted;
    this->filePath = std::move(other.filePath);
    this->fileSize = other.fileSize;
    this->metaType = other.metaType;
    this->duration = other.duration;
    this->image = std::move(other.image);
    return *this;
}

std::string stringifyMetaType(MetaType type)
{
    switch (type) {
    case MetaType::Video:
        return "video";
    case MetaType::Audio:
        return "audio";
    case MetaType::Image:
        return "image";
    default:
        return "unknown";
    }
}

MetaType formatMetaType(std::string_view str)
{
    if (str == "video")
        return MetaType::Video;
    if (str == "audio")
        return MetaType::Audio;
    if (str == "image")
        return MetaType::Image;
    return MetaType::Unknown;
}
