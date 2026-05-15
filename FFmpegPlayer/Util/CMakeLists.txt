set(LIB_NAME Util)

add_library(${LIB_NAME} STATIC
    Util.h Util.cpp
)

target_include_directories(${LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
target_link_libraries(${LIB_NAME} PUBLIC Qt${QT_VERSION_MAJOR}::Widgets)
