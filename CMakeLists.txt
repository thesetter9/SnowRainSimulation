cmake_minimum_required(VERSION 3.16)

project(SnowRainSimulation VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets OpenGL)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL)

if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
endif()

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    glwidget.cpp
    glwidget.h
    particle.h
    particlesystem.cpp
    particlesystem.h
    scenedata.h
    surfacecover.cpp
    surfacecover.h
)

add_executable(SnowRainSimulation ${PROJECT_SOURCES}
    scenedata.h
    surfacecover.h surfacecover.cpp)

target_link_libraries(SnowRainSimulation PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
)

if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(SnowRainSimulation PRIVATE
        Qt6::OpenGLWidgets
    )
endif()

# GLM находится в:
# project/glm/glm/glm.hpp
target_include_directories(SnowRainSimulation PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../glm
)
