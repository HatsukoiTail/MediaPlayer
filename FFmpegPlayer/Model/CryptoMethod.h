#ifndef CRYPTOMETHOD_H
#define CRYPTOMETHOD_H

#include <QJsonObject>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

constexpr uint64_t MAGIC = 0x0000000000990227;
constexpr uint32_t VERSION = 0x00000001;

struct Header
{
    uint32_t magic;
    uint32_t size;
    std::array<uint8_t, 32> key;
};

enum class CryptoResult
{
    Failure,
    Success,
    Stopped
};

QJsonObject getFileMetaData(const std::filesystem::path& path);
QJsonObject parserJson(const std::string& str);
std::string formatFileTime(uint64_t timestamp);

CryptoResult encrypt(const std::string& filePath, const std::string& outPath, std::function<void(size_t, size_t)> progress, std::shared_ptr<std::atomic_bool> isStopped);
CryptoResult decrypt(const std::string& inFilePath, const std::string& outFilePath, std::function<void(size_t, size_t)> progress, std::shared_ptr<std::atomic_bool> isStopped);

#endif // CRYPTOMETHOD_H
