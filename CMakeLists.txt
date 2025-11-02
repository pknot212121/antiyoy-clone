cmake_minimum_required(VERSION 4.0)

set(CMAKE_CXX_STANDARD 17)
project(anti)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)


add_executable(anti main.cpp game.cpp Grid.cpp player.cpp shader.cpp sprite_renderer.cpp warrior.cpp texture.cpp Hexagon.cpp resource_manager.cpp glad.c)
include_directories(anti PRIVATE include)

target_link_libraries(anti ${GLFW3_LIBRARY} OpenGL::GL)