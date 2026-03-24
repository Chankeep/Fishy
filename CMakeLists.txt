cmake_minimum_required(VERSION 3.20)

# Project Name
project(Fishy LANGUAGES C CXX)

# Set C++ Standard (Vulkan-Hpp recommends C++20, or at least C++17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Global Compile Definitions
add_compile_definitions(VK_NO_PROTOTYPES)
add_compile_definitions(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE)
add_compile_definitions(VULKAN_HPP_NO_CONSTRUCTORS)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ==============================================================================
# 1. Find Vulkan SDK (Must be installed on all platforms)
# ==============================================================================
find_package(Vulkan REQUIRED)

# ==============================================================================
# 2. FetchContent Dependencies (Cross-platform auto-download)
# ==============================================================================
include(FetchContent)

# Optimize FetchContent: Avoid checking for updates on every configure
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Don't update deps on every configure")

# ------------------------------------------------------------------------------
# GLFW - Window Management
# ------------------------------------------------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
# Disable Wayland backend, force X11 to fix missing window decorations under WSLg
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glfw)

# ------------------------------------------------------------------------------
# GLM - Math Library
# ------------------------------------------------------------------------------
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# ------------------------------------------------------------------------------
# vk-bootstrap - Vulkan Initialization Helper Library
# ------------------------------------------------------------------------------
FetchContent_Declare(
    vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.4.328
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(vk-bootstrap)

# ------------------------------------------------------------------------------
# volk - Vulkan Function Loader
# ------------------------------------------------------------------------------
set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG        vulkan-sdk-1.4.328.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(volk)
target_link_libraries(volk PRIVATE Vulkan::Vulkan)

# ------------------------------------------------------------------------------
# spdlog - Logging Library (Recommended: compile as static library + use built-in fmt)
# ------------------------------------------------------------------------------
set(SPDLOG_BUILD_SHARED     OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE    OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH      OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_WAL        OFF CACHE BOOL "" FORCE)
set(SPDLOG_COMPILED_LIB     ON  CACHE BOOL "" FORCE)   # Key: Compile as library

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(spdlog)

# ------------------------------------------------------------------------------
# fastgltf - glTF Model Loading
# ------------------------------------------------------------------------------
set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG        v0.9.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(fastgltf)

# ------------------------------------------------------------------------------
# stb - Image loading (previously bundled in tinygltf)
# ------------------------------------------------------------------------------
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        2fb8c5a3deb2110c89669f8d6f36e5833b556b44 # master as of 2026-03-08
)
FetchContent_MakeAvailable(stb)

# stb is header-only, create interface target
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# ------------------------------------------------------------------------------
# VMA - Vulkan Memory Allocator
# ------------------------------------------------------------------------------
FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.1.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

# ------------------------------------------------------------------------------
# libktx - KTX/KTX2 Texture Loading (for IBL cubemaps)
# ------------------------------------------------------------------------------
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VK_UPLOAD ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    libktx
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG        v4.3.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(libktx)

if(MSVC)
    # Disable libktx warnings to avoid interference from third-party libraries
    target_compile_options(ktx PRIVATE /W0)
endif()

# ------------------------------------------------------------------------------
# EnTT - Entity Component System
# ------------------------------------------------------------------------------
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.13.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(entt)

# ------------------------------------------------------------------------------
# Dear ImGui - Immediate Mode GUI (Docking Branch)
# ------------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(imgui)

# Create ImGui static library (includes core + GLFW/Vulkan backends)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC
    glfw
    Vulkan::Vulkan
    volk::volk
)

# Use volk to load Vulkan functions
target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_VULKAN_USE_VOLK
    VK_NO_PROTOTYPES
)


# Find Slang library
find_library(SLANG_LIB
    NAMES slang
    HINTS "$ENV{VULKAN_SDK}/Lib"
    REQUIRED
)
message(STATUS "Found Slang library: ${SLANG_LIB}")

# ==============================================================================
# 2. Set Output Directory
# ==============================================================================
# Ensure compiled executables run in bin directory, shaders in bin/shaders
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==============================================================================
# 4. Add Source Files
# ==============================================================================

# Recursively include all .cpp and .h files under src
# Note: Manually listing files is better than GLOB_RECURSE in production, but use GLOB for dev convenience
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp" "src/*.h")

# ==============================================================================
# 5. Define Executable
# ==============================================================================

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Include Directories
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${Vulkan_INCLUDE_DIRS}
    $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
)

# Linking Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    vk-bootstrap::vk-bootstrap
    volk::volk
    spdlog::spdlog
    fastgltf
    stb
    GPUOpen::VulkanMemoryAllocator
    ktx
    EnTT::EnTT
    imgui
    ${SLANG_LIB}
    ${CMAKE_DL_LIBS}
)

target_precompile_headers(${PROJECT_NAME} PRIVATE
    # Place libraries meant for global use here
    
    "${CMAKE_SOURCE_DIR}/src/core/LogSystem.h"
)

# ==============================================================================
# 7. Copy Assets and Shaders to Output Directory
# ==============================================================================

add_custom_command(TARGET ${PROJECT_NAME}
 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
    COMMENT "Copying assets to binary directory..."
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders
    COMMENT "Copying shaders to binary directory..."
)