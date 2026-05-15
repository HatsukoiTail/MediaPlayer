cmake_minimum_required(VERSION 3.16)

# 需要先构建父项目的 FFmpegLib 库
project(UI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

# 链接父项目的 FFmpegLib 静态库
add_subdirectory(.. ffmpeg_build)

qt_standard_project_setup()

qt_add_executable(UI
    main.cpp
    widget.cpp
    widget.h
    SimpleAudioRenderer.h SimpleAudioRenderer.cpp
    SimpleVideoRenderer.h SimpleVideoRenderer.cpp
)

target_include_directories(UI PRIVATE FFmpeg)

target_link_libraries(UI PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    FFmpegLib
)

# Qt 需要链接 FFmpeg DLL
set(FFmpeg_Path "D:/Enviroument/C++/FFmpeg/ffmpeg-8.0.1-full_build-shared")
target_link_directories(UI PRIVATE ${FFmpeg_Path}/lib)

set_target_properties(UI PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS UI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
