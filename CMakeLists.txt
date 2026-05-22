cmake_minimum_required(VERSION 3.21)

project(3DMovingStereo VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(3DMovingStereo
    WIN32
    main.cpp
    app_resources.qrc
    app_icon.rc
)

target_link_libraries(3DMovingStereo PRIVATE Qt6::Widgets)
