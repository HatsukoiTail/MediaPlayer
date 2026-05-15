
set(PROJECT_SOURCES
        native.h
        native.cpp
)

add_library(Platform STATIC ${PROJECT_SOURCES})

target_include_directories(Platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
target_link_libraries(Platform PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
