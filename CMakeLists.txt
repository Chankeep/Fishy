cmake_minimum_required(VERSION 3.20)

# 项目名称
project(Fishy LANGUAGES C CXX)

# 设置 C++ 标准 (Vulkan-Hpp 建议使用 C++20，或者至少 C++17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 全局编译定义
add_compile_definitions(VK_NO_PROTOTYPES)
add_compile_definitions(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE)
add_compile_definitions(VULKAN_HPP_NO_CONSTRUCTORS)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ==============================================================================
# 1. 寻找 Vulkan SDK (所有平台必须安装)
# ==============================================================================
find_package(Vulkan REQUIRED)

# ==============================================================================
# 2. FetchContent Dependencies (跨平台自动下载)
# ==============================================================================
include(FetchContent)

# 优化 FetchContent：避免每次 configure 都检查更新
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Don't update deps on every configure")

# ------------------------------------------------------------------------------
# GLFW - 窗口管理
# ------------------------------------------------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
# 禁用 Wayland 后端，强制使用 X11，解决 WSLg 下窗口装饰缺失问题
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glfw)

# ------------------------------------------------------------------------------
# GLM - 数学库
# ------------------------------------------------------------------------------
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# ------------------------------------------------------------------------------
# vk-bootstrap - Vulkan 初始化辅助库
# ------------------------------------------------------------------------------
FetchContent_Declare(
    vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.4.328
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(vk-bootstrap)

# ------------------------------------------------------------------------------
# volk - Vulkan 函数加载器
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
# spdlog - 日志库（推荐：编译为静态库 + 使用内置 fmt）
# ------------------------------------------------------------------------------
set(SPDLOG_BUILD_SHARED     OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE    OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH      OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_WAL        OFF CACHE BOOL "" FORCE)
set(SPDLOG_COMPILED_LIB     ON  CACHE BOOL "" FORCE)   # 关键：编译成库

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(spdlog)

# ------------------------------------------------------------------------------
# fastgltf - glTF 模型加载
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
    # 禁用 libktx 的警告，避免第三方库的警告干扰
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
# Dear ImGui - 即时模式 GUI (Docking Branch)
# ------------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(imgui)

# 创建 ImGui 静态库 (包含核心 + GLFW/Vulkan backends)
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

# 使用 volk 加载 Vulkan 函数
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
# 2. 设置输出目录
# ==============================================================================
# 让编译出来的可执行文件都在 bin 目录下，shader 也在 bin/shaders 下
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==============================================================================
# 4. 添加源文件
# ==============================================================================

# 递归包含 src 下的所有 .cpp 和 .h 文件
# 注意：在生产环境中手动列出文件比 GLOB_RECURSE 更好，但为了开发方便先用 GLOB
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp" "src/*.h")

# ==============================================================================
# 5. 定义可执行文件
# ==============================================================================

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# 包含路径 (Include Directories)
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${Vulkan_INCLUDE_DIRS}
    $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
)

# 链接库 (Linking)
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
    # 这里放你想全局使用的库
    
    "${CMAKE_SOURCE_DIR}/src/core/LogSystem.h"
)

# ==============================================================================
# 7. 复制 Assets (资源文件) 和 Shaders 到输出目录
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