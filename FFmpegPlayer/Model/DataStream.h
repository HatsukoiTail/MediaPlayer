#ifndef DATASTREAM_H
#define DATASTREAM_H

#include <QJsonObject>

#include <fstream>
#include <string>

class DataStream
{
public:
    DataStream() = default;
    explicit DataStream(const std::string& filePath);
    ~DataStream() = default;

public:
    bool open(const std::string& filePath);
    void close();
    int64_t read(uint8_t* data, size_t len);
    std::vector<uint8_t> readAll();
    bool seek(size_t offset, std::ios_base::seekdir dir = std::ios::beg);
    size_t pos() const;
    bool isOpen() const;

public:
    QJsonObject metaData() const;
    std::string filePath() const;
    size_t fileSize() const;
    bool isEncrypted() const;

private:
    QJsonObject meta_data;

private:
    std::ifstream file;
    std::string file_path;
    size_t file_size;

private:
    bool is_encrypted;
    size_t offset;
    std::array<uint8_t, 32> key;
};

#endif // DATASTREAM_H
