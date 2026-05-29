cmake_minimum_required(VERSION 3.20)
project(ComplexVisualizer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

set(SRC_FILES 
    "src/main.cpp"
    "src/ComplexParser.h"
    "src/ComplexParser.cpp"
)

add_executable(ComplexVisualizer ${SRC_FILES})

target_link_libraries(ComplexVisualizer PRIVATE 
    glad::glad 
    glfw 
    imgui::imgui
)
target_compile_definitions(ComplexVisualizer PRIVATE 
    SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/"
)