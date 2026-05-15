#include "DataStream.h"

#include "CryptoMethod.h"
#include "Print.h"

#include <cassert>
#include <filesystem>

DataStream::DataStream(const std::string &filePath)
{
    this->open(filePath);
}

bool DataStream::open(const std::string &filePath)
{
    assert(std::filesystem::exists(filePath) && std::filesystem::is_regular_file(filePath));

    std::filesystem::path file_path(filePath);
    auto file_size = std::filesystem::file_size(file_path);

    this->file.open(file_path, std::ios::binary);
    if (!this->file.is_open())
        return false;

    this->file.seekg(-(sizeof(Header)), std::ios::end);
    if (this->file.fail())
    {
        this->file.close();
        return false;
    }

    Header header;
    this->file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (this->file.gcount() != sizeof(header))
    {
        this->file.close();
        return false;
    }

    if (header.magic == MAGIC)
    {
        this->file.seekg(-static_cast<int64_t>(sizeof(header) + header.size), std::ios::end);
        if (this->file.fail())
        {
            this->file.close();
            return false;
        }
        std::string data_str(header.size, 0);
        this->file.read(reinterpret_cast<char*>(data_str.data()), data_str.size());
        if (this->file.gcount() != data_str.size())
        {
            this->file.close();
            return false;
        }
        this->file.seekg(0, std::ios::beg);
        if (this->file.fail())
        {
            this->file.close();
            return false;
        }
        this->meta_data = parserJson(data_str);
        this->key = header.key;
        this->file_size = file_size - sizeof(header) - header.size;
        this->is_encrypted = true;
    }
    else
    {
        this->file.seekg(0);
        if (this->file.fail())
        {
            this->file.close();
            return false;
        }
        this->meta_data = getFileMetaData(file_path);
        this->is_encrypted = false;
        this->meta_data = getFileMetaData(file_path);
        this->file_size = file_size;
    }
    this->offset = 0;
    this->file_path = filePath;
    return true;
}

void DataStream::close()
{
    this->file.close();
}

int64_t DataStream::read(uint8_t *out, size_t len)
{
    assert(this->file.is_open());

    this->file.read(reinterpret_cast<char*>(out), len);
    const auto true_read = this->file.gcount();
    if (true_read == 0)
    {
        this->offset += true_read;
        if (this->file.eof())
            return 0;
        return -1;
    }

    if (!this->is_encrypted)
    {
        this->offset += true_read;
        return true_read;
    }

    const size_t key_size = this->key.size();
    for (size_t i = 0; i < true_read; ++i)
    {
        const size_t key_offset = (this->offset + i) % key_size;
        out[i] ^= this->key[key_offset];
    }
    this->offset += true_read;
    return true_read;
}

std::vector<uint8_t> DataStream::readAll()
{
    assert(this->file.is_open());
    std::vector<uint8_t> data(this->file_size, 0);
    this->file.seekg(0, std::ios::beg);
    if (this->file.fail())
        return {};
    this->file.read(reinterpret_cast<char*>(data.data()), data.size());
    const auto true_read = this->file.gcount();
    if (true_read != data.size())
        return {};
    if (this->is_encrypted)
    {
        const size_t key_size = this->key.size();
        for (size_t i = 0; i < true_read; ++i)
        {
            const size_t key_offset = i % key_size;
            data[i] ^= this->key[key_offset];
        }
    }
    this->offset = true_read;
    return data;
}

bool DataStream::seek(size_t offset, std::ios_base::seekdir dir)
{
    assert(this->file.is_open());
    this->file.clear();
    this->file.seekg(offset, dir);
    if (this->file.fail())
    {
        std::ios_base::iostate state = file.rdstate();

        if (state & std::ios::failbit) {
            print("  - failbit: 逻辑错误（如格式错误、文件未找到）");
        }
        if (state & std::ios::badbit) {
            print("  - badbit: 严重错误（如磁盘错误、内存分配失败）");
        }
        if (state & std::ios::eofbit) {
            print("  - eofbit: 已到达文件末尾");
        }
        return false;
    }
    this->offset = static_cast<size_t>(this->file.tellg());
    return true;
}

size_t DataStream::pos() const
{
    return this->offset;
}

bool DataStream::isOpen() const
{
    return this->file.is_open();
}

QJsonObject DataStream::metaData() const
{
    assert(this->file.is_open());
    return this->meta_data;
}

std::string DataStream::filePath() const
{
    return this->file_path;
}

size_t DataStream::fileSize() const
{
    assert(this->file.is_open());
    return this->file_size;
}

bool DataStream::isEncrypted() const
{
    assert(this->file.is_open());
    return this->is_encrypted;
}


