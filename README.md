# 🐟 Fishy Engine

一个基于现代 Vulkan 的轻量级渲染引擎。

## 🎯 项目目标

构建一个现代 Vulkan 渲染器原型，验证以下核心架构：

- **多帧并行渲染** - Frames in Flight 架构，最大化 GPU 利用率
- **模块化资源管理** - 统一的 Buffer、纹理、描述符管理
- **基础 PBR 渲染流程** - 物理基础渲染管线
- **GPU-Driven 预留** - 为后续 Mesh Shader / GPU Culling 打基础

## ✨ 特性

- **Vulkan 1.4** - 现代图形 API，支持 Dynamic Rendering
- **C++20** - 利用最新语言特性
- **vulkan-hpp RAII** - 自动资源管理，代码更安全
- **vk-bootstrap** - 简化的 Vulkan 初始化与设备选择
- **volk** - Meta loader，动态加载函数指针，提升扩展兼容性与调用性能
- **Slang** - 现代着色器语言
- **GLFW** - 跨平台窗口管理
- **GLM** - 数学计算库

**已完成:**
- ✅ 窗口化 (GLFW 集成)
- ✅ Vulkan 初始化 (vk-bootstrap + volk)
- ✅ 交换链管理 (创建/重建/窗口 Resize)
- ✅ 多帧并行架构 (Frames in Flight)
- ✅ 动态状态 (Viewport/Scissor)
- ✅ Shader 管理 (SPIR-V 加载)

**进行中:**
- 🔄 前向渲染 Pass (深度测试)
- 🔄 Buffer 封装 (VMA 集成)

## 🛠️ 构建

```bash
mkdir build && cd build
cmake ..
ninja  # 或 make
```

## 🚀 运行

```bash
cd build/bin
./Fishy
```

## 📁 项目结构

```
Fishy/
├── src/
│   ├── core/         # 核心模块 (Device, Window, SwapChain)
│   └── renderer/     # 渲染器 (Pipeline, Renderer)
├── shaders/          # Slang 着色器
├── docs/             # 文档 (PRD, 设计文档)
└── external/         # 第三方库
```

## 📋 依赖

- Vulkan SDK 1.3+
- GLFW 3.3+
- GLM
- Slang 编译器
- vk-bootstrap (FetchContent)
- volk (FetchContent)

## 📄 License

MIT
