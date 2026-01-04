cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
project(anti)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(OpenGL REQUIRED)

set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(freetype)
add_subdirectory(glfw)
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
        shaders-arr/text_vs.h
        shaders-arr/instance_vs.h
        shaders-arr/instance_fs.h
        textures-arr/b.h
        textures-arr/castle_256.h
        textures-arr/exclamation.h
        textures-arr/farm1_256.h
        textures-arr/gravestone_256.h
        textures-arr/hexagon.h
        textures-arr/palmTree_256.h
        textures-arr/pineTree_256.h
        textures-arr/shield_placeholder.h
        textures-arr/soilder1_256.h
        textures-arr/soldier2_256.h
        textures-arr/soldier3_256.h
        textures-arr/soldier4_256.h
        textures-arr/strongTower_256.h
        textures-arr/tower_256.h)
include_directories(anti PRIVATE include)
set_property(TARGET anti PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(anti glfw OpenGL::GL freetype)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
endif()