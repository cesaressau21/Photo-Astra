cmake_minimum_required(VERSION 3.24)
project(PhotoAstra VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PHOTO_ASTRA_BUILD_UI "Build the Qt desktop application" ON)
option(PHOTO_ASTRA_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
include(CTest)
include(cmake/ProjectOptions.cmake)

add_subdirectory(src/core)
add_subdirectory(src/render)

if(PHOTO_ASTRA_BUILD_UI)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets OpenGLWidgets Concurrent)
    qt_standard_project_setup(REQUIRES 6.5)
    add_subdirectory(src/io)
    add_subdirectory(src/application)
    add_subdirectory(src/ui)
    add_subdirectory(src/app)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
