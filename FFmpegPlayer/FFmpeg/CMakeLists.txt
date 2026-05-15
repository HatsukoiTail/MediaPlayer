set(LIB_NAME FFmpeg)

add_library(${LIB_NAME} STATIC
        MediaData/MediaData.h       MediaData/MediaData.cpp
        DataModel/DataModel.h       DataModel/DataModel.cpp
        SmartStruct/SmartStruct.h
        Factory/Factory.h           Factory/Factory.cpp
        Print/Print.h
        Util/Util.h                 Util/Util.cpp
        Demuxer/Demuxer.h           Demuxer/Demuxer.cpp
        Decoder/Decoder.h           Decoder/Decoder.cpp
        Decoder/AudioDecoder.h      Decoder/AudioDecoder.cpp
        Decoder/VideoDecoder.h      Decoder/VideoDecoder.cpp
        FFmpegPlayer/FFmpegPlayer.h FFmpegPlayer/FFmpegPlayer.cpp
        AudioRender/AudioRender.h   AudioRender/AudioRender.cpp
        VideoRender/VideoRender.h   VideoRender/VideoRender.cpp
        Format/Format.h             Format/Format.cpp
)

target_include_directories(${LIB_NAME} PUBLIC
        # ${CMAKE_CURRENT_SOURCE_DIR}/MediaData
        ${CMAKE_CURRENT_SOURCE_DIR}/AudioRender
        ${CMAKE_CURRENT_SOURCE_DIR}/DataModel
        ${CMAKE_CURRENT_SOURCE_DIR}/Decoder
        ${CMAKE_CURRENT_SOURCE_DIR}/Demuxer
        ${CMAKE_CURRENT_SOURCE_DIR}/Factory
        ${CMAKE_CURRENT_SOURCE_DIR}/Format
        ${CMAKE_CURRENT_SOURCE_DIR}/FFmpegPlayer
        ${CMAKE_CURRENT_SOURCE_DIR}/Print
        ${CMAKE_CURRENT_SOURCE_DIR}/SmartStruct
        ${CMAKE_CURRENT_SOURCE_DIR}/Util
        ${CMAKE_CURRENT_SOURCE_DIR}/VideoRender
)

target_include_directories(${LIB_NAME} PRIVATE
        # ${CMAKE_CURRENT_SOURCE_DIR}/MediaData
        # ${CMAKE_CURRENT_SOURCE_DIR}/AudioRender
        # ${CMAKE_CURRENT_SOURCE_DIR}/DataModel
        # ${CMAKE_CURRENT_SOURCE_DIR}/Decoder
        # ${CMAKE_CURRENT_SOURCE_DIR}/Demuxer
        # ${CMAKE_CURRENT_SOURCE_DIR}/Factory
        # ${CMAKE_CURRENT_SOURCE_DIR}/Format
        # ${CMAKE_CURRENT_SOURCE_DIR}/FFmpegPlayer
        # ${CMAKE_CURRENT_SOURCE_DIR}/Print
        # ${CMAKE_CURRENT_SOURCE_DIR}/SmartStruct
        # ${CMAKE_CURRENT_SOURCE_DIR}/Util
        # ${CMAKE_CURRENT_SOURCE_DIR}/VideoRender
)

target_link_libraries(${LIB_NAME} PUBLIC Model)

# 引入PortAudio库
set(PortAudio_Path "D:/Enviroument/C++/PortAudio")
set(PortAudio_LIBS
    ${PortAudio_Path}/bin/libportaudio.dll.a)

target_include_directories(${LIB_NAME} PUBLIC
    ${PortAudio_Path}/include
)
target_link_libraries(${LIB_NAME} PRIVATE ${PortAudio_LIBS})

# 引入FFmpeg库
set(FFmpeg_Path "D:/Enviroument/C++/FFmpeg/ffmpeg-7.0-full_build-shared")
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
target_link_libraries(${LIB_NAME} PRIVATE ${FFmpeg_LIBS})

# 拷贝动态库
set(FFmpeg_DLLS
    ${FFmpeg_Path}/bin/avcodec-61.dll
    ${FFmpeg_Path}/bin/avdevice-61.dll
    ${FFmpeg_Path}/bin/avfilter-10.dll
    ${FFmpeg_Path}/bin/avformat-61.dll
    ${FFmpeg_Path}/bin/avutil-59.dll
    ${FFmpeg_Path}/bin/postproc-58.dll
    ${FFmpeg_Path}/bin/swresample-5.dll
    ${FFmpeg_Path}/bin/swscale-8.dll
)

foreach(dll IN LISTS FFmpeg_DLLS)
    if(NOT EXISTS "${dll}")
        message(WARNING "DLL not found: ${dll}")
        list(REMOVE_ITEM FFmpeg_DLLS "${dll}")
    endif()
endforeach()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR} CACHE PATH "Output directory for runtime binaries")
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# 拷贝
if(FFmpeg_DLLS)
    add_custom_command(TARGET ${LIB_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FFmpeg_DLLS}
            ${PortAudio_DLLS}
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMENT "Copying required DLLs"
    )
else()
    message(WARNING "No DLLs to copy - check paths")
endif()
