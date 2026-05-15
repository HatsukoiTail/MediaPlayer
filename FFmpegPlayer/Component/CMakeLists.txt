set(Library_Name Component)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(${Library_Name} STATIC
        General/TextEditor.h                        General/TextEditor.cpp
        General/FlowLayout.h                        General/FlowLayout.cpp
        MediaListWidget/MediaListWidget.h           MediaListWidget/MediaListWidget.cpp
        MediaListWidget/MediaListItem.h             MediaListWidget/MediaListItem.cpp
        MediaPreviewWidget/MediaPreviewWidget.h     MediaPreviewWidget/MediaPreviewWidget.cpp
        MediaPreviewWidget/MediaPreviewArea.h       MediaPreviewWidget/MediaPreviewArea.cpp
        MediaPreviewWidget/MediaPreviewCard.h       MediaPreviewWidget/MediaPreviewCard.cpp
        MediaPreviewWidget/MediaPreviewControlBar.h MediaPreviewWidget/MediaPreviewControlBar.cpp
        PageManager/PageManager.h                   PageManager/PageManager.cpp
        General/FileOutputDialog.h                  General/FileOutputDialog.cpp
        General/DialogBase.h                        General/DialogBase.cpp
        General/DialogTitle.h                       General/DialogTitle.cpp

        VideoController/VideoController.h         VideoController/VideoController.cpp
        General/ControlTitleBar.h               General/ControlTitleBar.cpp
        VideoController/VideoControlBar.h               VideoController/VideoControlBar.cpp
        VideoController/VideoControlPanel.h             VideoController/VideoControlPanel.cpp
        General/IconButton.h                        General/IconButton.cpp
        VideoController/PlayList.h                      VideoController/PlayList.cpp
        VideoController/ProgressBar.h                   VideoController/ProgressBar.cpp
        VideoController/VolumeBar.h                     VideoController/VolumeBar.cpp
        Video/Video.h                               Video/Video.cpp
        Video/OpenGLRender.h                        Video/OpenGLRender.cpp
        Video/PaintRender.h                         Video/PaintRender.cpp
        MediaInfoCard/MediaInfoCard.h                   MediaInfoCard/MediaInfoCard.cpp
        General/ImageView.h General/ImageView.cpp
        General/LineEdit.h General/LineEdit.cpp
        General/CircularProgressBar.h General/CircularProgressBar.cpp
        VideoController/SpeedMenu.h VideoController/SpeedMenu.cpp
        Image/Image.h Image/Image.cpp
        Media/Media.h Media/Media.cpp

        Image/ImageController.h Image/ImageController.cpp
        Image/ImagePlayer.h Image/ImagePlayer.cpp
        Video/VideoPlayer.h Video/VideoPlayer.cpp
        MediaPreviewWidget/MediaInfoList.h MediaPreviewWidget/MediaInfoList.cpp

)


target_link_libraries(Component PRIVATE Qt6::Widgets)
target_link_libraries(${Library_Name} PRIVATE Qt6::Widgets)

target_include_directories(${Library_Name} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/General
    ${CMAKE_CURRENT_SOURCE_DIR}/MediaInfoCard
    ${CMAKE_CURRENT_SOURCE_DIR}/MediaListWidget
    ${CMAKE_CURRENT_SOURCE_DIR}/VideoController
    ${CMAKE_CURRENT_SOURCE_DIR}/MediaPreviewWidget
    ${CMAKE_CURRENT_SOURCE_DIR}/PageManager
    ${CMAKE_CURRENT_SOURCE_DIR}/Video
    ${CMAKE_CURRENT_SOURCE_DIR}/Image
    ${CMAKE_CURRENT_SOURCE_DIR}/Media
)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
target_link_libraries(${Library_Name} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
target_link_libraries(${Library_Name} PUBLIC Qt6::OpenGLWidgets)

target_link_libraries(${Library_Name} PRIVATE
    Model
    FFmpeg
)
