# 从顶点到像素 · From Vertex to Pixel

**一本面向初学者的计算机图形学实践教程：不使用游戏引擎，从第一个 C++ 程序开始，亲手构建渲染器与一个简单的 3D 游戏。**

一个模型里到底存了什么？矩阵中的每个数有什么作用？三角形怎样变成像素？为什么有些物体挡住了另一些物体？

本教程沿着“数据如何变成画面”这条线展开。先在 CPU 上亲手计算像素，再使用 OpenGL 理解 GPU 渲染，最后把输入、碰撞、声音和渲染连接成可玩的关卡。

[开始阅读](chapters/00-reading-guide.md) · [完整目录](chapters/README.md) · [运行示例](exercises/README.md) · [学习路线](plan/00-roadmap.md)

## 适合谁

- 刚接触图形学，希望通过具体数字、示意图和代码理解原理的学习者。
- 用过 Unity 等引擎，希望弄清模型提交之后到底发生了什么的开发者。
- 想通过一个连续的小项目学习 C++、图形 API 和游戏循环的人。

不要求预先学过图形学。阅读代码需要逐步掌握变量、函数、条件、循环、结构体和 `std::vector`；可以从[零基础前置知识](references/beginner-prerequisites.md)与[第一个 C++ 工程](plan/03-first-cpp-project-in-vscode.md)开始补齐。

## 你会学到什么

全书分为三部分，共 22 章（第 00–21 章）。每章配有独立的完整 C++ 参考实现与练习说明。

| 阶段 | 章节 | 主要内容 | 实践结果 |
| --- | --- | --- | --- |
| 建立直觉 | 00–01 | 学习方法、模型、顶点与一帧的流程 | 描述模型变成像素的过程 |
| 数学基础 | 02–03 | 向量、矩阵、空间变换、相机与投影 | 手算并用程序验证坐标变换 |
| CPU 渲染 | 04–06 | 像素缓冲、三角形光栅化、深度、贴图与裁剪 | 把自己计算的画面输出为图片 |
| GPU 入门 | 07–08 | OpenGL 上下文、GPU 资源、着色器与绘制调用 | 在窗口中绘制网格 |
| 场景与资产 | 09–10 | 模型数据、场景组织、相机与可见性 | 加载模型、观察场景 |
| 光照与阴影 | 11–12 | 颜色空间、材质、基础光照与阴影贴图 | 为画面添加明暗与阴影 |
| 组织一帧 | 13–16 | 渲染队列、透明、实例化、后处理与调试 UI | 理解多个绘制阶段怎样协作 |
| 游戏系统 | 17–18 | 游戏循环、渲染快照、简单物理、音频与资源生命周期 | 连接输入、逻辑和画面 |
| 性能与进阶 | 19–20 | 性能测量、Vulkan/Metal 的显式 API 思维 | 用数据分析性能，理解迁移方向 |
| 毕业实践 | 21 | 整合一个最小游戏闭环 | 可操作的 3D 收集关卡 |

逐章链接见[教材目录](chapters/README.md)，代码与运行效果索引见[练习目录](exercises/README.md)。

“不使用引擎”不意味着从操作系统或显卡驱动开始写。这里使用 GLFW 创建窗口和处理输入，使用 GLM 提供数学类型；顶点数据、着色器、相机变换、渲染队列和帧流程由自己的代码组织。

## 快速开始

### 1. 准备环境

当前可运行示例面向 **macOS，使用 C++17 和 OpenGL 4.1 Core**。图形代码直接引用 macOS 的 OpenGL 头文件，音频使用系统框架；Windows/Linux 需要适配，不能直接照搬下面的构建流程。

需要 Apple Clang、CMake 3.20 或更新版本、Ninja、GLFW 和 GLM。若尚未安装 Apple 命令行开发工具，先执行：

```zsh
xcode-select --install
```

安装好 [Homebrew](https://brew.sh/) 后，安装构建工具与依赖：

```zsh
brew install cmake ninja glfw glm
```

OpenGL 由 macOS 提供，无需另外安装。VS Code 是可选编辑器；具体配置见 [macOS 上用 VS Code 开发](plan/02-vscode-on-macos.md)。

### 2. 配置、构建、运行

下载或克隆仓库，在终端进入**包含本 README 和 `CMakeLists.txt` 的根目录**，执行：

```zsh
# 配置工程，生成 Ninja 使用的构建文件。
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# 编译第 0 章，生成可执行程序。
cmake --build build --target chapter00

# 运行程序，在终端查看输出。
./build/exercises/chapter00
```

配置、构建、运行是三个不同动作：CMake 生成构建规则，构建工具调用编译器生成程序，最后一条命令才真正执行程序。修改已有 `.cpp` 后，通常只需重新构建并运行。

想看第一个 GPU 绘制示例：

```zsh
cmake --build build --target chapter08
./build/exercises/chapter08
```

它会打开图形窗口，按 `Esc` 退出。其他章节只需替换目标编号，例如 `chapter02`、`chapter12`，不需要为每章改写 `CMakeLists.txt` 或 `tasks.json`。

使用 VS Code 时，用 **Open Folder** 打开仓库根目录，安装 C/C++ 和 CMake Tools 扩展。在命令面板选择 `CMake: Select a Kit`，选 Apple Clang，再执行 `CMake: Configure`；之后可在 CMake Tools 中选择目标并构建、运行。初次接触这套流程，请读[详细入门指南](plan/03-first-cpp-project-in-vscode.md)。

### 3. 查看输出与毕业小样

| 章节 | 输出位置或形式 |
| --- | --- |
| 00–03 | 终端中的说明与计算结果 |
| 04–06 | `build/exercise_output/` 中的 PPM 图片 |
| 07–18、21 | GLFW 图形窗口 |
| 19 | 性能数据与 `build/exercise_output/` 中的 CSV |
| 20 | 终端中的显式 API 命令状态模拟 |

也可以先体验最终的小关卡：

```zsh
cmake --build build --target chapter21
./build/exercises/chapter21
```

按 `Enter` 开始，使用 `WASD` 移动，在障碍之间收集 5 个橙色方块。`P` 暂停或恢复，`R` 重置，`Esc` 退出。具体实现见[毕业小样说明](exercises/10_capstone/21-capstone.md)。

## 如何使用这本教程

1. **先读正文。** 知道当前概念解决什么问题，认清输入与输出。
2. **手算一个例子。** 尤其是向量、矩阵和投影，先跟着小数字算一遍。
3. **运行对应示例。** 对照相邻的 `.cpp` 文件，找到公式实际执行的位置。
4. **只改一个条件。** 先预测画面或数值会怎么变，再运行验证。
5. **记录自己的解释。** 能说明为什么得到这个结果，再进入下一章。

建议按章节顺序学习，尤其不要跳过 CPU 光栅化部分。练习目录提供可直接运行的完整实现，可用来对照或修改，也可以在理解后自己重新实现。

## 仓库结构

```text
.
├── chapters/        教材正文与章节目录
├── exercises/       逐章 C++ 示例、练习说明与公共辅助代码
├── plan/            学习路线、进度记录与 VS Code 开发指南
├── references/      前置知识、术语表与排错索引
├── src/             最初的 Hello World 程序
├── CMakeLists.txt   统一构建入口
└── README.md        教程首页
```

构建后生成的 `build/` 存放可执行程序、缓存和示例输出，不属于教材源码。

## 示例的实现范围

正文会介绍完整渲染系统的设计，随章代码则以便于阅读的最小实现为主：

- 第 09 章讨论 glTF 等资产组织方式，当前样例使用小型 OBJ 加载器。
- 第 16 章使用自写的简易调试界面，第 18 章使用简单物理实现；不需要预先安装 Dear ImGui 或 Jolt。
- 第 20 章是显式 API 的概念与命令状态模拟，**不是实际 Vulkan 或 Metal 渲染后端**。
- 第 21 章整合一个可玩的最小关卡，没有把前面所有独立渲染效果都合并进去。

这是一套学习材料与参考实现，适合逐步理解和扩展；章节中的进阶目标不等于样例已经实现的全部功能。

## 检查与排错

构建全部目标并运行已有的冒烟检查：

```zsh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

这些检查用于快速发现基本问题，不等同于图形效果验收。图形章节的测试使用 `--check` 无窗口路径，仍需实际打开示例，检查画面与交互。

- [零基础前置知识](references/beginner-prerequisites.md)：补齐阅读代码所需的基础。
- [术语表](references/glossary.md)：查找陌生名词。
- [排错索引](references/troubleshooting.md)：从报错或画面现象定位原因。
- [学习进度](plan/progress.md)：记录已完成内容与待解决问题。

如果发现讲解错误、代码问题，或某个步骤仍不够清楚，欢迎通过 Issue 提供章节位置和问题描述；运行问题请同时附上系统环境与完整报错。
