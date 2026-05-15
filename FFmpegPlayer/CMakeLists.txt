cmake_minimum_required(VERSION 3.19)
project(FFmpegPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)

qt_standard_project_setup()

qt_add_executable(${PROJECT_NAME}
    WIN32 MACOSX_BUNDLE
    main.cpp
    Window.h Window.cpp
    WindowTitle.h WindowTitle.cpp
    Widget.h Widget.cpp
    Resource.qrc
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt::Core
        Qt::Widgets
)

find_package(Qt6 REQUIRED COMPONENTS Concurrent)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Concurrent)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ${PROJECT_NAME}
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

add_subdirectory(Model)
# add_subdirectory(FramelessWindow)
add_subdirectory(Component)
add_subdirectory(FFmpeg)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Model
    # Util
    Component
    FFmpeg
    dwmapi
)
