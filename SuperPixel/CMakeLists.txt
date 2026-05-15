cmake_minimum_required(VERSION 3.16)

project(SLIC_Super_Pixel)

set(CMAKE_CXX_STANDARD 20)

set(SourceFiles
    PixelConverter/PixelConverter.cpp
    Slic/Slic.cpp
)

add_executable(${PROJECT_NAME} main.cpp ${SourceFiles})

find_package(OpenMP REQUIRED)

if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
else()
    message(WARNING "OpenMP not found. The program will run without parallelization.")
endif()