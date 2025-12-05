cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 20)
project(anti)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# GLFW options
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(freetype)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_executable(anti
        main.cpp
        game.cpp
        board.cpp
        shader.cpp
        sprite_renderer.cpp
        texture.cpp
        resource_manager.cpp
        glad.c
        text_renderer.h
        text_renderer.cpp
        sockets.h
        sockets.cpp
        font.h
        shaders-arr/sprite_vs.h
        shaders-arr/sprite_fs.h
        shaders-arr/text_fs.h
        shaders-arr/text_vs.h)
include_directories(anti PRIVATE include)
set_property(TARGET anti PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(anti glfw /usr/lib/x86_64-linux-gnu/libGL.so.1 freetype)