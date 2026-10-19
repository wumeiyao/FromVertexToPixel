# macOS 上用 VS Code 开发

本机已具备 Apple Clang、CMake 和 Homebrew。按本章配置后，VS Code 负责编辑、配置、构建和 LLDB 调试；CMake 负责生成真实构建系统。

若你刚从 Visual Studio 转来，先阅读 [第一个 C++ 工程](03-first-cpp-project-in-vscode.md)。本文件只说明 macOS 环境和 CMake Tools 操作。

## 1. 安装 VS Code 扩展

在 Extensions 中安装：

1. C/C++，发布者 Microsoft。
2. CMake Tools，发布者 Microsoft。
3. CodeLLDB，发布者 Vadim Chugunov，可选但推荐。

不要同时启用 clangd 和 Microsoft C/C++ 的 IntelliSense，二者会产生重复诊断。第一阶段只使用 Microsoft C/C++。

不要安装 CMake，发布者 twxs。CMake Tools 已提供对应的语言服务，安装两者会出现重复功能和卸载提示。

## 2. 安装项目依赖

在 VS Code 的 Terminal 中执行：

~~~zsh
brew install glfw glm ninja
~~~

这三个就是本书全部的外部依赖：

| 依赖 | 作用 | 备注 |
| --- | --- | --- |
| glfw | 创建窗口和 OpenGL Context，处理键盘鼠标 | 第 7 章开始使用 |
| glm | 现成的向量和矩阵类型 | 第 2、3 章自己实现一遍后改用它 |
| ninja | 构建工具，让 CMake 编译得更快 | 也可以用默认的 make |

OpenGL Framework 由 macOS 提供，无需安装。**本书不需要 GLAD**：macOS 直接提供固定的 OpenGL 4.1，`#include <OpenGL/gl3.h>` 就能拿到全部函数。只有 Windows/Linux 才需要函数加载器，原因见[第 7 章 7.4](../chapters/07-opengl-context-resources.md)。

### 教材里提到、但不需要现在安装的库

正文在讲到资产、UI 和物理时会点名几个工业界常用库。它们**不是本书可运行路线的一部分**，`exercises/` 已经提供了只依赖 GLFW/GLM 的最小替代实现：

| 教材提到 | 出现章节 | 本书可运行路线用什么 |
| --- | --- | --- |
| fastgltf / cgltf | 第 9 章 | 自带的小型 OBJ 解析器 |
| Dear ImGui | 第 16 章 | 自写的最小 Immediate-mode Overlay |
| Jolt Physics | 第 18 章 | 自写的 `fixedPhysicsStep` |
| stb_image | 第 9 章 | 自带的 `image.hpp` |

第一遍学习时不要去装它们。等 OpenGL 版本跑通、进入毕业工程时再逐个替换，那时你已经知道自己要它们解决什么问题。

## 3. 两种开发方式

### 单文件练习：先不用 CMake

刚学 C++、只写一个 main.cpp 时，在 Terminal 执行下面两条命令即可：

~~~zsh
mkdir -p build
clang++ -std=c++17 src/main.cpp -o build/hello
./build/hello
~~~

第一条 clang++ 命令是“编译并链接”，只会生成 build/hello；第二条才是“运行程序”，因此 Hello, World! 会在第二条命令后出现。此阶段不需要 tasks.json、launch.json 或 CMake。

### 多文件项目：使用 CMake

当代码开始拆为多个 .cpp、需要 GLFW/OpenGL、第三方库或测试时，再使用 CMakeLists.txt。CMakeLists.txt 是跨平台的工程描述，作用相当于 Visual Studio 的 .vcxproj；它只需在创建工程和增加源文件/依赖时修改，不是每次写代码都改。

## 4. 用 CMake 打开和运行项目

使用 File > Open Folder 打开 CodeA 根目录，而不是只打开某个 .cpp 文件。随后按 Cmd+Shift+P：

1. 执行 CMake: Select a Kit，选择 AppleClang arm64。
2. 执行 CMake: Select Variant，选择 Debug。
3. 执行 CMake: Configure。
4. 执行 CMake: Build，只生成可执行文件。
5. 执行 CMake: Run Without Debugging，运行目标程序；或执行 CMake: Debug，使用断点和 LLDB 运行目标程序。

第 0 章创建 CMakeLists.txt 后，CMake Tools 会自动在 build/ 下生成构建文件。不要直接编辑 build/ 内任何文件。

不要使用 C/C++: clang 生成活动文件。这是单文件临时任务，不理解 CMake 工程，也不应创建 tasks.json。

## 5. 日常工作流

~~~text
修改 .cpp -> CMake Build -> CMake Run Without Debugging 或 CMake Debug
-> 截图、中间 Texture 或日志验证 -> 记录到 plan/progress.md
~~~

推荐始终使用 Debug 构建来学习和排错；测量性能时再切到 Release。每次加入一个渲染状态或 Pass，都用断点、Shader 日志、能力检查、中间 Texture 或 OpenGL 错误检查验证。RenderDoc 不原生支持当前 macOS OpenGL 4.1 捕获，因此不是本课程在 Mac 上的硬性工具。

只有修改 CMakeLists.txt、新增 .cpp 文件或新增第三方库后，才重新执行 CMake: Configure。

## 6. VS Code 命令行

当前机器没有 code 命令，但 VS Code 已可从图形界面使用。若想在终端通过 code . 打开工程：

1. 在 VS Code 按 Cmd+Shift+P。
2. 执行 Shell Command: Install 'code' command in PATH。
3. 重开终端后执行：

~~~zsh
cd /Users/wumeiyao/CodeA
code .
~~~

## 7. macOS 图形 API 限制

macOS 原生 OpenGL 的最高版本是 4.1 Core，且 API 已被 Apple 标记为 deprecated。第一版应使用 GLSL 410，避免 OpenGL 4.3+ 的 Compute Shader、SSBO 和 4.5 的 Direct State Access。

这不妨碍完成本课程：模型、贴图、光照、阴影、实例化、Framebuffer 和后处理都可实现。完成 OpenGL 版后，再选择 Vulkan + MoltenVK 或 Metal 进行第二阶段迁移。

### 不用 macOS 怎么办

本书的可运行参考实现目前**只在 macOS 上编译**：`exercises/common/gl_support.hpp` 对非 Apple 平台会直接 `#error`。

- **教材正文（chapters/）与平台无关。** 第 0–6 章是纯 CPU 代码，任何平台都能跑；第 7 章之后的概念、数据流和 Shader 在三大平台上完全一致。
- **Windows/Linux 想跑 exercises**，需要自己做两处改动：把 `gl_support.hpp` 里的 `#include <OpenGL/gl3.h>` 换成 GLAD（`#include <glad/glad.h>`，并在 `glfwMakeContextCurrent` 之后调用一次 `gladLoadGLLoader`），然后把 `CMakeLists.txt` 里的 glad 源码加进构建。请求的版本可以从 4.1 提到 4.5。
- 这属于移植工作，不属于第一遍学习内容。如果你是零基础又不用 macOS，最省事的做法是先完整读完第 0–6 章（不需要任何图形 API），同时准备环境。
