# VS Code 中的第一个 C++ 工程

本章的目标是从一个空文件夹得到一个可运行的 Hello, World!，并理解每个工具负责什么。示例工程使用当前的 CodeA 目录。

## 1. 先理解与 Visual Studio 的区别

Visual Studio 是完整 IDE。创建“控制台应用”模板时，它替你创建项目文件、选定 MSVC 编译器、提供 Build 和 F5 调试按钮。

VS Code 是编辑器加扩展。它不会默认替你决定项目结构或第三方库。对 C++ 项目，本章使用的分工是：

~~~text
VS Code        编辑代码、显示错误、提供命令面板和调试界面
CMake Tools    读取 CMakeLists.txt，调用 CMake
CMake          将工程描述转换为具体构建规则
Ninja          按构建规则调用编译器
clang++        编译 C++ 并链接 C++ 标准库
macOS          加载可执行文件，从 main() 开始运行
~~~

因此，CMakeLists.txt 对应 Visual Studio 中的 .vcxproj；它不是每次都改的配置，而是工程长期保存的“编译说明书”。

## 2. 最小工程长什么样

~~~text
CodeA/
  CMakeLists.txt       工程描述，手写并提交
  src/
    main.cpp            C++ 源码，手写并提交
  build/                自动生成，可删除，不编辑
  .vscode/              VS Code 可选配置；本教程不需要它
~~~

文件名大小写有意义。CMake 只会按 CMakeLists.txt 中写出的路径寻找源文件；src/Main.cpp 和 src/main.cpp 是不同文件。

## 3. 写第一个 C++ 源文件

创建 src/main.cpp：

~~~cpp
#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
~~~

逐行解释：

- #include <iostream> 把标准输入输出库的声明提供给当前源文件。它让编译器知道 std::cout 是什么。
- int main() 是程序入口。macOS 启动程序后会调用它。
- 花括号中的内容是 main 函数的函数体。
- std::cout 表示标准输出流，通常连接到 VS Code 的 Terminal。
- << 把右侧内容写到输出流。
- std::endl 输出换行并刷新输出流。
- return 0 告诉操作系统程序正常结束。

源文件只是文本，保存 main.cpp 并不会产生可运行程序。

## 4. 写 CMakeLists.txt

在 CodeA 根目录创建 CMakeLists.txt：

~~~cmake
cmake_minimum_required(VERSION 3.20)

project(CodeA LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(hello src/main.cpp)
~~~

逐行解释：

- cmake_minimum_required：要求能理解本文件语法的最低 CMake 版本。
- project：声明工程名为 CodeA，并声明只需要 C++，CXX 是 CMake 对 C++ 的缩写。
- CMAKE_CXX_STANDARD 17：要求编译器用 C++17 语言规则。
- CMAKE_CXX_STANDARD_REQUIRED ON：若编译器无法满足 C++17，则报错，不悄悄退回旧标准。
- add_executable：创建一个名为 hello 的可执行目标，输入源码是 src/main.cpp。

hello 是“构建目标名”，不是源文件名。CMake 会在 build 目录中生成 build/hello。

## 5. 从源码到程序，计算机实际做了什么

~~~text
src/main.cpp
  -> 预处理器展开 #include <iostream>
  -> C++ 编译器把代码翻译为机器目标文件 main.cpp.o
  -> 链接器把目标文件和 C++ 标准库组合
  -> build/hello，一个 macOS 可执行文件
  -> macOS 加载它并调用 main()
  -> std::cout 写入 Terminal
~~~

“编译”常被宽泛地指整个过程，严格来说它至少有两个阶段：

1. 编译：每个 .cpp 独立翻译为 .o 目标文件。
2. 链接：把多个 .o 与库组合为最终程序。

这就是为什么调用 clang 会出现 std::cout 找不到：它能将 .cpp 编译为目标文件，但作为 C 驱动链接时不会自动加入 C++ 标准库。clang++ 是 C++ 驱动，会正确链接标准库。

## 6. 先用终端理解最短路径

在 VS Code 中打开 Terminal，确认当前目录是 CodeA，然后执行：

~~~zsh
mkdir -p build
clang++ -std=c++17 src/main.cpp -o build/hello
./build/hello
~~~

三条命令分别是：

1. mkdir -p build：创建输出目录。目录已存在也不会报错。
2. clang++：把 src/main.cpp 编译并链接为 build/hello。-o 的意思是 output，指定输出位置和名称。
3. ./build/hello：运行当前目录下 build 目录中的 hello。前缀 ./ 的意思是“从当前目录开始找”，避免 macOS 从系统命令路径中查找。

第二条命令成功时通常不打印 Hello, World!，因为它只构建；第三条命令才执行程序和打印文字。

如果只想临时用一次命令完成构建加运行，可以写：

~~~zsh
clang++ -std=c++17 src/main.cpp -o build/hello && ./build/hello
~~~

&& 表示仅当前一条命令成功时才执行后一条。

## 7. 用 CMake Tools 点击按钮完成同一件事

当项目有多个 .cpp 文件、GLFW、OpenGL 或测试时，用 CMake 替代手写长命令。先通过 File > Open Folder 打开 CodeA 根目录，不能只打开 main.cpp。

首次创建或修改 CMakeLists.txt 后：

1. 按 Cmd+Shift+P，执行 CMake: Select a Kit。
2. 选择 Clang 21.0.0 arm64-apple-darwin 开头的选项。
3. 执行 CMake: Configure。

Configure 读取 CMakeLists.txt，并在 build/ 创建 build.ninja。此时 CMake 知道 hello 由哪个源文件、用什么编译器和标准生成。

日常只修改 .cpp 文件时：

1. 执行 CMake: Build。它调用 Ninja，Ninja 调用 clang++，并更新 build/hello。
2. 执行 CMake: Run Without Debugging。若询问目标，选择 hello。

需要断点时，点击 main.cpp 行号左侧设置红点，再执行 CMake: Debug。它相当于 Visual Studio 的 F5：确保目标最新，然后在 LLDB 中启动。

## 8. 哪些文件现在不需要

- tasks.json：不要用。它通常只是“编译当前活动文件”的临时任务，不知道 CMake 的目标、库和多个源文件。
- launch.json：不要用。CMake: Debug 可以启动当前 hello 目标；复杂的多个启动配置以后再加。
- c_cpp_properties.json：不要用。CMake 生成的 compile_commands.json 会提供大多数 IntelliSense 所需的编译信息。
- .sln：不要用。它是 Visual Studio / C# 解决方案格式，不是此 CMake 工程的一部分。
- build/ 内文件：不要编辑。它们是 CMake 和 Ninja 生成的临时规则和产物，删除后可由 Configure 重建。

若底部看到 No Solution，说明 C# 或 Solution 类扩展在寻找 .sln；它与本项目无关，可以忽略或对当前工作区禁用。

## 9. 什么时候需要重新 Configure

只改 main.cpp 的内容：CMake Build。

发生以下变化时：CMake Configure，然后 CMake Build。

- 修改 CMakeLists.txt。
- 新增或删除 .cpp、.h 文件并改变了 add_executable 列表。
- 新增 GLFW、OpenGL、GLM 等依赖。
- 切换编译器、Debug/Release 或目标架构。
- 删除 build/ 目录后重新开始。

## 10. 常见现象不是错误

| 现象 | 含义 | 正确动作 |
| --- | --- | --- |
| ninja: no work to do | 源码没有变化，已有程序是最新的 | 直接 Run |
| Build 成功但没有 Hello, World! | 程序已生成但没有执行 | Run Without Debugging 或 ./build/hello |
| Undefined symbols: std::cout | 使用 clang 而不是 clang++ 链接 C++ | 使用 CMake 或 clang++ |
| No Solution | C# 扩展在寻找 .sln | 忽略或禁用该扩展 |
| CMake 没有目标 | CMakeLists.txt 空白或缺少 add_executable | 添加正确的 add_executable |

## 11. 新项目的固定模板

以后创建一个普通 C++ 工程，只需复制下面两个文件，再将 hello 和 src/main.cpp 替换为你的项目名称和源码列表：

~~~cmake
cmake_minimum_required(VERSION 3.20)
project(MyGame LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(my_game src/main.cpp)
~~~

随后只需一次 Select a Kit 和 Configure。之后的循环永远是：

~~~text
修改 C++ 源码 -> CMake Build -> CMake Run Without Debugging
~~~

当开始第一个 OpenGL 窗口时，只是在 CMakeLists.txt 中增加 GLFW 和 OpenGL 依赖，核心流程不变。
