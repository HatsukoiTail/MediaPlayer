#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>

#include "Transcoder.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

int main()
{
    system("chcp 65001");

    TranscodeOptions options;
    options.input_file = "F:/Work/MediaPlayer/Resource/new world.mp4";
    options.output_file = "../Test/test.mp4";

    // 配置视频流：key=源流索引
    options.video_streams[0] = {
        .codec = "h264_nvenc",
        // .hwaccel = "d3d11va",
        .width = 1280,
        .height = 720,
        .pixel_format = AV_PIX_FMT_CUDA,
    };

    try
    {
        std::filesystem::create_directories("logs");

        Transcoder transcoder;
        transcoder.open(options);

        std::cout << "Transcoder opened.\n"
                  << "Starting transcode..." << std::endl;

        transcoder.start();

        std::this_thread::sleep_for(std::chrono::seconds(5));

        while (transcoder.is_running())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::cout << "Transcoding: " << transcoder.progress() * 100 << "%" << std::endl;
        }

        transcoder.stop();
        std::cout << "Transcode finished." << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
