set(LIB_NAME FramelessWindow)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(${LIB_NAME} STATIC
        Window.h        Window.cpp
        WindowTitle.h   WindowTitle.cpp
)

target_link_libraries(Model PRIVATE Qt6::Core)

target_include_directories(${LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
target_link_libraries(${LIB_NAME} PUBLIC Qt${QT_VERSION_MAJOR}::Widgets)
