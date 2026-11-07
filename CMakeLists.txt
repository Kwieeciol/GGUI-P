cmake_minimum_required(VERSION 3.28)

project(GGUI-Projekt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SFML_BUILD_AUDIO OFF)
set(SFML_BUILD_NETWORK OFF)
set(SFML_BUILD_EXAMPLES OFF)
set(SFML_BUILD_TEST_SUITE OFF)

add_subdirectory(external/SFML)

add_executable(GGUI-Projekt main.cpp)

target_link_libraries(GGUI-Projekt PRIVATE SFML::Graphics)