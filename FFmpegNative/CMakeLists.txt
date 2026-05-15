cmake_minimum_required(VERSION 3.15)

set(LIB_NAME FFmpegLib)

project(${LIB_NAME})

set(CMAKE_CXX_STANDARD 23)

set(Source
    FrameLoader/FrameLoader.h       FrameLoader/FrameLoader.cpp
    ImageLoader/ImageLoader.h       ImageLoader/ImageLoader.cpp
    MediaInfo/MediaInfo.h           MediaInfo/MediaInfo.cpp
    MediaPlayer/AudioFilter.h       MediaPlayer/AudioFilter.cpp
    MediaPlayer/AudioOutput.h       MediaPlayer/AudioOutput.cpp
    MediaPlayer/Clock.h
    MediaPlayer/Decoder.h           MediaPlayer/Decoder.cpp
    MediaPlayer/Demuxer.h           MediaPlayer/Demuxer.cpp
    MediaPlayer/IAudioRenderer.h
    MediaPlayer/IVideoRenderer.h
    MediaPlayer/MediaPlayer.h       MediaPlayer/MediaPlayer.cpp
    MediaPlayer/ThreadTask.h        MediaPlayer/ThreadTask.cpp
    MediaPlayer/VideoFilter.h       MediaPlayer/VideoFilter.cpp
    MediaPlayer/VideoScheduler.h    MediaPlayer/VideoScheduler.cpp
    Native/CodecContext.h           Native/CodecContext.cpp
    Native/FFmpegException.h        Native/FFmpegException.cpp
    Native/FilterGraph.h
    Native/FormatContext.h          Native/FormatContext.cpp
    Native/HWDeviceContext.h        Native/HWDeviceContext.cpp
    Native/Frame.h                  Native/Frame.cpp
    Native/Helper.h                 Native/Helper.cpp
    Native/Logger.h                 Native/Logger.cpp
    Native/Packet.h                 Native/Packet.cpp
    Native/PixelFormator.h          Native/PixelFormator.cpp
    Queue/FrameQueue.h              Queue/FrameQueue.cpp
    Queue/PacketQueue.h             Queue/PacketQueue.cpp
    Transcode/AudioFilter.h         Transcode/AudioFilter.cpp
    Transcode/Decoder.h             Transcode/Decoder.cpp
    Transcode/Demuxer.h             Transcode/Demuxer.cpp
    Transcode/Encoder.h             Transcode/Encoder.cpp
    Transcode/Muxer.h               Transcode/Muxer.cpp
    Transcode/TranscodeOptions.h    Transcode/TranscodeOptions.cpp
    Transcode/Transcoder.h          Transcode/Transcoder.cpp
    Transcode/VideoFilter.h         Transcode/VideoFilter.cpp
)

# --- 静态库（供 Qt 等项目链接） ---
add_library(${LIB_NAME} STATIC ${Source})

target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/FrameLoader
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageLoader
    ${CMAKE_CURRENT_SOURCE_DIR}/MediaInfo
    ${CMAKE_CURRENT_SOURCE_DIR}/MediaPlayer
    ${CMAKE_CURRENT_SOURCE_DIR}/Native
    ${CMAKE_CURRENT_SOURCE_DIR}/Queue
    ${CMAKE_CURRENT_SOURCE_DIR}/Transcode
)

# FFmpeg 库
set(FFmpeg_Path "D:/Enviroument/C++/FFmpeg/ffmpeg-8.0.1-full_build-shared")
set(FFmpeg_LIBS
    ${FFmpeg_Path}/lib/avformat.lib
    ${FFmpeg_Path}/lib/avcodec.lib
    ${FFmpeg_Path}/lib/avutil.lib
    ${FFmpeg_Path}/lib/swscale.lib
    ${FFmpeg_Path}/lib/swresample.lib
    ${FFmpeg_Path}/lib/avdevice.lib
    ${FFmpeg_Path}/lib/avfilter.lib
)

target_include_directories(${LIB_NAME} PUBLIC
    ${FFmpeg_Path}/include
)
target_link_libraries(${LIB_NAME} PUBLIC ${FFmpeg_LIBS})

# --- 转码测试可执行文件 ---
add_executable(FFmpeg main.cpp)
target_link_libraries(FFmpeg PRIVATE ${LIB_NAME})
