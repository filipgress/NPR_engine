cmake_minimum_required(VERSION 3.10)
project(npr_engine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
    message(STATUS "No build type selected, default to Debug")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# ---------------------------------------
# Fetch external dependencies
# ---------------------------------------

include(FetchContent)

# vulkan haders (vulkan-hpp)
FetchContent_Declare(
    vulkan_headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG v1.4.327
)
FetchContent_MakeAvailable(vulkan_headers)

# glfw
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# glm
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# tinygltf
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v2.9.6
)

set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL_VENDOR OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(tinygltf)

# imgui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.3
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    # ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)

FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
    GIT_TAG v4.1.1
)
FetchContent_MakeAvailable(flecs)

# Require Vulkan SDK to be present for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_package(Vulkan)
    if(NOT Vulkan_FOUND)
        message(FATAL_ERROR "Debug builds require Vulkan SDK!")
    endif()
endif()

# ---------------------------------------
# Project build setup
# ---------------------------------------

set(HEADER_FILES
    # core
    ./src/core/pch.h
    ./src/core/utils.h
    ./src/core/app.h
    ./src/core/frame_timer.h
    ./src/core/input_handler.h
    # scene
    ./src/scene/scene.h
    ./src/scene/scene_loader.h
    ./src/scene/components.h
    # window
    ./src/window/window.h
    ./src/window/event.h
    ./src/window/key_event.h
    ./src/window/mouse_event.h
    ./src/window/app_event.h
    # graphics
    ./src/graphics/renderer.h
    ./src/graphics/vulkan_context.h
    ./src/graphics/swapchain.h
    ./src/graphics/sync.h
    ./src/graphics/resources.h
    ./src/graphics/buffer.h
    ./src/graphics/image.h
    ./src/graphics/command_pool.h
    ./src/graphics/descriptor_pool.h
    ./src/graphics/descriptor_sets.h
    ./src/graphics/render_pass.h
    ./src/graphics/shader.h
    ./src/graphics/pipeline.h
    ./src/graphics/pipeline_cache.h
)
set(SOURCE_FILES
    # core
    ./src/core/main.cc
    ./src/core/utils.cc
    ./src/core/app.cc
    ./src/core/frame_timer.cc
    # scene
    ./src/scene/scene.cc
    ./src/scene/scene_loader.cc
    # window
    ./src/window/window.cc
    # graphics
    ./src/graphics/renderer.cc
    ./src/graphics/vulkan_context.cc
    ./src/graphics/swapchain.cc
    ./src/graphics/sync.cc
    ./src/graphics/resources.cc
    ./src/graphics/buffer.cc
    ./src/graphics/image.cc
    ./src/graphics/command_pool.cc
    ./src/graphics/descriptor_pool.cc
    ./src/graphics/descriptor_sets.cc
    ./src/graphics/render_pass.cc
    ./src/graphics/shader.cc
    ./src/graphics/pipeline.cc
    ./src/graphics/pipeline_cache.cc
)

configure_file(src/core/config.h.in config.h)
add_executable(${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_SOURCES})
target_compile_definitions(
    # vulkan functions will be loaded at run-time
    ${PROJECT_NAME}
    PRIVATE VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_NO_PROTOTYPES
)

target_precompile_headers(${PROJECT_NAME} PRIVATE ./src/core/pch.h)
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        ./src/
        ${CMAKE_CURRENT_BINARY_DIR}
        ${vulkan_headers_SOURCE_DIR}/include
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw glm tinygltf flecs::flecs)

# ---------------------------------------
# Enable additional debug flags
# ---------------------------------------

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        target_compile_options(
            ${PROJECT_NAME}
            PRIVATE /W4 /WX /ZI /RTC1 /FS /Od
        )
    else()
        target_compile_options(
            ${PROJECT_NAME}
            PRIVATE -Wall -Wextra -Werror -Wpedantic -g -O0
        )
    endif()
endif()

# ---------------------------------------
# Compile shaders during build
# ---------------------------------------

find_program(GLSLC_EXECUTABLE "glslc")
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Please make sure it's in your PATH.")
endif()

set(SHADER_DIR "shaders")
set(SHADER_BIN_DIR "${CMAKE_BINARY_DIR}/${SHADER_DIR}")

# ensure output dir is created
file(MAKE_DIRECTORY ${SHADER_BIN_DIR})
file(
    GLOB SHADERS
    "${SHADER_DIR}/*.vert"
    "${SHADER_DIR}/*.frag"
    "${SHADER_DIR}/*.comp"
)

foreach(SHADER IN LISTS SHADERS)
    get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
    get_filename_component(SHADER_EXT ${SHADER} EXT)
    string(SUBSTRING ${SHADER_EXT} 1 -1 SHADER_EXT_DOT)
    set(OUTPUT_FILE "${SHADER_BIN_DIR}/${SHADER_NAME}_${SHADER_EXT_DOT}.spv")

    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        DEPENDS ${SHADER}
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER} -o ${OUTPUT_FILE}
        COMMENT "Compiling shader: ${SHADER_NAME}"
        VERBATIM
    )

    list(APPEND SPV_SHADERS ${OUTPUT_FILE})
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SPV_SHADERS})
add_dependencies(${PROJECT_NAME} compile_shaders)

# ---------------------------------------
# Get compile_commands.json into root
# ---------------------------------------

if(UNIX)
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_SOURCE_DIR}/compile_commands.json
        COMMENT "Creating symlink to compile_commands.json in project root."
    )
else()
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_SOURCE_DIR}/compile_commands.json
        COMMENT "Copying compile_commands.json to project root."
    )
endif()
