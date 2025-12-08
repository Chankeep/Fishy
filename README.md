# 🐟 Fishy Engine

一个基于现代 Vulkan 的轻量级渲染引擎。

## ✨ 特性

- **Vulkan 1.4** - 现代图形 API
- **C++20** - 利用最新语言特性
- **Slang** - 作为主要着色器语言
- **GLFW** - 跨平台窗口管理
- **GLM** - 数学计算库

## 🛠️ 构建

```bash
mkdir build && cd build
cmake ..
make
```

## 🚀 运行

```bash
./build/bin/Fishy
```

## 📁 项目结构

```
Fishy/
├── src/          # 源代码
├── shaders/      # Slang 着色器
├── assets/       # 资源文件
└── external/     # 第三方库
```

## 📋 依赖

- Vulkan SDK
- GLFW 3.3+
- GLM
- Slang 编译器

## 📄 License

MIT
