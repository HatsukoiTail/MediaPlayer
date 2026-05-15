set(LIB_NAME Model)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(${LIB_NAME} STATIC
        Model.h Model.cpp
        DataStream.h DataStream.cpp
        CryptoMethod.h CryptoMethod.cpp
        CryptoTaskManager.h CryptoTaskManager.cpp
        EventBus.h EventBus.cpp
        MediaInfo.h MediaInfo.cpp
        MediaDataLoader.h MediaDataLoader.cpp
        MediaFinder.h MediaFinder.cpp
        Tool.h Tool.cpp)

target_link_libraries(Model PRIVATE Qt6::Core)
target_link_libraries(Model PRIVATE Qt6::Core)
target_link_libraries(Model PRIVATE Qt6::Core)
target_link_libraries(Model PRIVATE Qt6::Core)

target_include_directories(${LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
target_link_libraries(${LIB_NAME} PUBLIC Qt${QT_VERSION_MAJOR}::Widgets)

find_package(Qt6 REQUIRED COMPONENTS Concurrent)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)
target_link_libraries(${LIB_NAME} PRIVATE Qt6::Concurrent)

target_link_libraries(${LIB_NAME} PUBLIC FFmpeg)
