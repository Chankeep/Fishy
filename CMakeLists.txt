cmake_minimum_required(VERSION 3.20)

# 项目名称
project(Fishy LANGUAGES CXX)

# 设置 C++ 标准 (Vulkan-Hpp 建议使用 C++20，或者至少 C++17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==============================================================================
# 1. 寻找依赖库 (Linux 环境)
# ==============================================================================

# 寻找 Vulkan SDK
find_package(Vulkan REQUIRED)

# 寻找 GLFW3
find_package(glfw3 3.3 REQUIRED)

# 寻找 GLM (数学库)
find_package(glm REQUIRED)

# 寻找 Slang 编译器 (slangc)
# 确保 slangc 已经在系统 PATH 中，或者手动指定路径
find_program(SLANG_COMPILER slangc REQUIRED)
message(STATUS "Found Slang compiler: ${SLANG_COMPILER}")

# ==============================================================================
# 2. 设置输出目录
# ==============================================================================
# 让编译出来的可执行文件都在 bin 目录下，shader 也在 bin/shaders 下
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==============================================================================
# 3. 自动编译 Slang Shaders 的函数
# ==============================================================================
# 这个宏会自动查找 shaders 目录下的 .slang 文件，并编译为 .spv
# 编译后的文件会放在 build/bin/shaders 目录中
# ==============================================================================

function(compile_slang_shaders TARGET_NAME SHADER_DIR)
    # 扫描目录下所有的 .slang 文件
    file(GLOB_RECURSE SHADER_SOURCES "${SHADER_DIR}/*.slang")
    
    set(SPV_OUTPUTS "")

    # 创建输出目录
    file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")

    foreach(SHADER_SRC ${SHADER_SOURCES})
        # 获取文件名 (e.g., "triangle.slang")
        get_filename_component(FILENAME ${SHADER_SRC} NAME)
        # 构造输出文件名 (e.g., "triangle.slang.spv")
        set(SPV_OUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${FILENAME}.spv")
        
        # 添加自定义编译命令
        # 注意：这里假设你的 slang 文件里有 entry point 或者是一个 module
        # -target spirv: 输出 SPIR-V
        # -profile glsl_460: 兼容 GLSL 4.6 环境
        add_custom_command(
            OUTPUT ${SPV_OUT}
            COMMAND ${SLANG_COMPILER} ${SHADER_SRC} -target spirv -profile glsl_460 -o ${SPV_OUT}
            DEPENDS ${SHADER_SRC}
            COMMENT "Compiling Slang shader: ${FILENAME} -> ${FILENAME}.spv"
            VERBATIM
        )
        
        list(APPEND SPV_OUTPUTS ${SPV_OUT})
    endforeach()

    # 创建一个自定义 Target，确保 shader 在编译 C++ 之前被编译
    add_custom_target(${TARGET_NAME}_Shaders ALL DEPENDS ${SPV_OUTPUTS})
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_Shaders)
endfunction()

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
    external                  # 假设你有外部库头文件在这里
    ${Vulkan_INCLUDE_DIRS}    # Vulkan 头文件
    ${GLM_INCLUDE_DIRS}       # GLM 头文件
)

# 链接库 (Linking)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    # Linux 下可能还需要链接多线程库，虽然 find_package 通常会自动处理
    ${CMAKE_DL_LIBS}
)

# ==============================================================================
# 6. 调用 Shader 编译函数
# ==============================================================================

# 告诉 CMake 我们的 Shader 在哪里
compile_slang_shaders(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/shaders")

# ==============================================================================
# 7. 复制 Assets (资源文件) 到输出目录
# ==============================================================================

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
    COMMENT "Copying assets to binary directory..."
)