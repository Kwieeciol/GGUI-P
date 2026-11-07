cmake_minimum_required(VERSION 3.28)

project(GGUI-Projekt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SFML_BUILD_AUDIO OFF)
set(SFML_BUILD_NETWORK OFF)
set(SFML_BUILD_EXAMPLES OFF)
set(SFML_BUILD_TEST_SUITE OFF)

add_subdirectory(external/SFML)

find_package(OpenGL REQUIRED)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
set(IMGUI_SFML_DIR ${CMAKE_SOURCE_DIR}/external/imgui-sfml)

add_library(imgui_sfml STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_SFML_DIR}/imgui-SFML.cpp
)

target_include_directories(imgui_sfml PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_SFML_DIR}
)

target_link_libraries(imgui_sfml PUBLIC
    SFML::Graphics
    OpenGL::GL
)

add_executable(GGUI-Projekt main.cpp)

target_link_libraries(GGUI-Projekt PRIVATE imgui_sfml)

if(WIN32)
    set(FFMPEG_EXE ${CMAKE_SOURCE_DIR}/external/ffmpeg/windows/ffmpeg.exe)
elseif(APPLE)
    set(FFMPEG_EXE ${CMAKE_SOURCE_DIR}/external/ffmpeg/macos/ffmpeg)
endif()

if(FFMPEG_EXE AND EXISTS ${FFMPEG_EXE})
    add_custom_command(TARGET GGUI-Projekt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FFMPEG_EXE} $<TARGET_FILE_DIR:GGUI-Projekt>
        COMMENT "Copying ffmpeg next to the executable"
    )

    if(APPLE)
        add_custom_command(TARGET GGUI-Projekt POST_BUILD
            COMMAND chmod +x $<TARGET_FILE_DIR:GGUI-Projekt>/ffmpeg
            COMMAND xattr -c $<TARGET_FILE_DIR:GGUI-Projekt>/ffmpeg
            COMMENT "Preparing ffmpeg for macOS"
        )
    endif()
else()
    message(WARNING "ffmpeg not found in external/ffmpeg - saving to webp/tiff/avif will not work")
endif()
