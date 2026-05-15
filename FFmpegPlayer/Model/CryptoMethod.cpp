#include "CryptoMethod.h"

#include "Model.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>

std::array<uint8_t, 32> generate_key()
{
    std::random_device rd;
    std::seed_seq seed_seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
    std::mt19937_64 rng(seed_seq);
    std::array<uint8_t, 32> key;
    for (auto& item : key)
    {
        item = static_cast<uint8_t>(rng() & 0XFF);
    }
    return key;
}

uint64_t file_time_to_uint64(const std::filesystem::path& file_path)
{
    auto file_time = std::filesystem::last_write_time(file_path);
    auto system_time = std::chrono::clock_cast<std::chrono::system_clock>(file_time);
    std::time_t time_t_value = std::chrono::system_clock::to_time_t(system_time);
    return static_cast<uint64_t>(time_t_value);
}

void set_file_time(const std::filesystem::path& path, uint64_t time)
{
    std::time_t time_t_value = static_cast<std::time_t>(time);
    auto system_time = std::chrono::system_clock::from_time_t(time_t_value);
    auto file_time = std::chrono::clock_cast<std::chrono::file_clock>(system_time);
    std::filesystem::last_write_time(path, file_time);
}

CryptoResult encrypt(const std::string &inFilePath, const std::string &outFilePath, std::function<void (size_t, size_t)> progress, std::shared_ptr<std::atomic_bool> isStopped)
{
    assert(std::filesystem::path(inFilePath) != std::filesystem::path(outFilePath));
    namespace fs = std::filesystem;

    // 检查输入文件是否存在且是文件
    fs::path input_path(inFilePath);
    assert(fs::exists(input_path) && "Input file does not exist");
    assert(fs::is_regular_file(input_path) && "Input path is not a file");

    fs::path output_path(outFilePath);
    assert(!fs::exists(output_path) && "Output file exists");

    std::ifstream input_file(input_path, std::ios::binary);
    std::ofstream output_file(output_path, std::ios::binary);

    if (!input_file.is_open())
        return CryptoResult::Failure;
    if (!output_file.is_open())
        return CryptoResult::Failure;

    try {
        auto key = generate_key();
        const size_t total_size = fs::file_size(input_path);
        size_t processed = 0;
        constexpr size_t buffer_size = 32;
        std::vector<uint8_t> buffer(buffer_size);

        while (!input_file.eof() && !isStopped.get()->load())
        {
            input_file.read(reinterpret_cast<char*>(buffer.data()), buffer_size);
            const std::streamsize read_len = input_file.gcount();
            if (read_len < 0)
            {
                output_file.close();
                fs::remove(output_path);
                return CryptoResult::Failure;
            }

            for (size_t offset = 0; offset < read_len; ++offset)
            {
                const size_t key_offset = (processed + offset) % key.size();
                buffer[offset] ^= key[key_offset];
            }

            output_file.write(reinterpret_cast<const char*>(buffer.data()), read_len);

            processed += static_cast<size_t>(read_len);
            if (progress)
                progress(processed, total_size);
        }

        if (isStopped.get()->load())
        {
            output_file.close();
            fs::remove(output_path);
            return CryptoResult::Stopped;
        }

        auto meta_data = getFileMetaData(input_path);
        QJsonDocument doc(meta_data);
        auto data_string = doc.toJson(QJsonDocument::Indented).toStdString();
        output_file.write(data_string.data(), data_string.size());

        Header header;
        header.magic = MAGIC;
        header.key = key;
        header.size = data_string.size();
        output_file.write(reinterpret_cast<const char*>(&header), sizeof(header));

        output_file.flush();
        return CryptoResult::Success;

    } catch (...) {
        input_file.close();
        output_file.close();
        fs::remove(output_path);
        return CryptoResult::Failure;
    }
}

CryptoResult decrypt(const std::string &inFilePath, const std::string &outFilePath, std::function<void (size_t, size_t)> progress, std::shared_ptr<std::atomic_bool> isStopped)
{
    assert(std::filesystem::path(inFilePath) != std::filesystem::path(outFilePath));
    namespace fs = std::filesystem;

    fs::path input_path(inFilePath);
    assert(fs::exists(input_path) && "Encrypted file does not exist");
    assert(fs::is_regular_file(input_path) && "Encrypted file is not a file");

    fs::path output_path(outFilePath);
    assert(!fs::exists(output_path) && "Output path has been exist");

    std::ifstream input_file(input_path, std::ios::binary);
    if (!input_file.is_open())
        return CryptoResult::Failure;

    std::ofstream output_file(output_path, std::ios::binary);
    if (!output_file.is_open())
        return CryptoResult::Failure;

    try {
        // 读取固定头部
        Header header;

        input_file.seekg(-sizeof(header), std::ios::end);
        if (input_file.fail())
            return CryptoResult::Failure;

        input_file.read(reinterpret_cast<char*>(&header), sizeof(header));
        auto read_len = input_file.gcount();
        if (read_len != sizeof(header))
            return CryptoResult::Failure;

        // 验证魔数和版本
        if (header.magic != MAGIC)
            return CryptoResult::Failure;

        // 读取自定义头部
        std::string data_str(header.size, 0);

        input_file.seekg(-(sizeof(header) + data_str.size()), std::ios::end);
        if (input_file.fail())
            return CryptoResult::Failure;

        input_file.read(reinterpret_cast<char*>(data_str.data()), data_str.size());
        read_len = input_file.gcount();
        if (read_len != data_str.size())
            return CryptoResult::Failure;

        input_file.seekg(0, std::ios::beg);
        if (input_file.fail())
            return CryptoResult::Failure;

        // 计算数据大小
        const size_t header_size = sizeof(header) + data_str.size();
        const size_t total_size = fs::file_size(input_path) - header_size;
        size_t processed = 0;

        constexpr size_t buffer_size = 32;
        std::vector<uint8_t> buffer(buffer_size);

        while (processed < total_size && !isStopped.get()->load())
        {
            auto read_size = std::min(buffer_size, total_size - processed);
            input_file.read(reinterpret_cast<char*>(buffer.data()), read_size);
            const auto read_len = input_file.gcount();
            if (read_len < 0)
            {
                output_file.close();
                fs::remove(output_path);
                return CryptoResult::Failure;
            }
            if (read_len == 0)
                break;

            // XOR解密（与加密相同的操作）
            for (size_t offset = 0; offset < read_len; ++offset) {
                const size_t key_offset = (processed + offset) % header.key.size();
                buffer[offset] ^= header.key[key_offset];
            }

            // 写入解密后的数据
            output_file.write(reinterpret_cast<const char*>(buffer.data()), read_len);

            processed += read_len;

            // 进度回调
            if (progress)
                progress(processed, total_size);
        }

        // 检查是否被停止
        if (isStopped.get()->load())
        {
            output_file.close();
            fs::remove(output_path);
            return CryptoResult::Stopped;
        }

        // 设置最后修改时间

        // 反序列化自定义头部
        auto json = parserJson(data_str);
        auto last_modified = json["lastModifiedTime"].toInteger();
        set_file_time(output_path, static_cast<uint64_t>(last_modified));

        input_file.close();
        output_file.close();

        return CryptoResult::Success;

    } catch (const std::exception& e) {
        input_file.close();
        output_file.close();
        std::filesystem::remove(output_path);
        return CryptoResult::Failure;
    }
}

std::string formatFileTime(uint64_t timestamp)
{
    std::time_t time_t_value = static_cast<std::time_t>(timestamp);
    std::tm* local_time = std::localtime(&time_t_value);

    std::stringstream ss;
    ss << std::put_time(local_time, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

QJsonObject getFileMetaData(const std::filesystem::__cxx11::path &path)
{
    const auto filename = path.filename().string();
    QJsonObject json;
    json["fileName"] = QString::fromStdString(filename);
    json["metaType"] = QString::fromStdString(stringifyMetaType(inferMetaType(filename)));
    json["lastModifiedTime"] = static_cast<qint64>(file_time_to_uint64(path));
    return json;
}

QJsonObject parserJson(const std::string &str)
{
    QByteArray bytes(str);
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
    {
        return {};
    }
    return doc.object();
}
