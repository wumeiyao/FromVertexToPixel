# 第 7 章：OpenGL Context 与 GPU 资源

<!-- nav -->
> [上一章：深度、纹理、透视校正与裁剪](06-cpu-depth-texture-clipping.md) ｜ [下一章：GLSL 与第一条 Draw Call](08-shaders-first-draw.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/03_opengl_basics/07-opengl-context-resources.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

第 4 到第 6 章由 CPU 循环处理每个候选 Pixel。现在改用 OpenGL，让 GPU 并行执行顶点变换、三角形光栅化、插值和深度测试。

OpenGL 不是游戏引擎。它不会提供场景、相机对象、模型加载器或游戏循环；它是一套图形 API。API 是 Application Programming Interface（应用程序编程接口）的缩写，可以理解为程序调用另一套系统能力时必须遵守的函数和数据规则。OpenGL API 规定 C++ 程序怎样创建 GPU 资源、设置渲染状态并提交绘制命令。

## 本章导读

### 7.1 本章学习目标

完成本章后，应能：

1. 区分操作系统窗口、OpenGL Context 和默认 Framebuffer。
2. 解释 GLFW、函数加载器、OpenGL 驱动各自负责什么。
3. 创建窗口与 OpenGL 4.1 Core Context，持续清屏。
4. 解释 Buffer、VAO、Texture、Shader 和 Program 的职责。
5. 逐项说明一次顶点数据上传改变了什么状态和内存。
6. 建立明确的 GPU 资源所有权与错误检查习惯。

本章分三部分：先创建可清屏的 Window 和 Context，再认识 GPU Resource，最后把状态、生命周期与调试接成一帧。

> **不用 macOS 的读者**：教材正文与平台无关，第 0–6 章的 CPU 代码任何平台都能跑。只有 `exercises/` 里的可运行样例目前限定 macOS。Windows/Linux 需要改两处（换成 GLAD、调整 CMake），步骤见[环境搭建的“不用 macOS 怎么办”](../plan/02-vscode-on-macos.md)。

第一次阅读只要求完成 `7.2～7.9`：创建窗口、改变 Clear Color、正确响应 Resize。`7.10` 以后先建立资源词汇，真正创建并使用 VAO、VBO 和 Program 会在第 8 章连成完整示例。

## 第一部分：窗口、Context 与最小循环

### 7.2 OpenGL 位于程序和驱动之间

数据与命令的大致路径：

~~~text
你的 C++ 程序
  -> 调用 OpenGL 函数
  -> 图形驱动检查、记录或转换命令
  -> GPU 执行命令
  -> 结果写入 GPU Framebuffer
  -> 窗口系统展示完成画面
~~~

OpenGL 是规范，不是某一个固定库文件里的完整渲染器。显卡厂商或操作系统提供驱动（driver）实现；驱动是把通用 OpenGL 命令翻译并安排到具体硬件上的系统软件。相同 API 调用可能在不同 GPU 上用不同内部方式执行，但可观察结果应符合规范。

CPU 调用 Draw 时，GPU 不一定已经画完。驱动常把命令排队，让 CPU 与 GPU 并行工作。因此“函数返回”通常只表示命令已被接受，不表示屏幕此刻已更新。

### 7.3 四个容易混淆的对象

#### Window：操作系统窗口

窗口是可见矩形，拥有标题、尺寸、关闭按钮，并接收键盘鼠标和缩放事件。它由 macOS 窗口系统管理，不是 OpenGL 自己创建的概念。

#### OpenGL Context：OpenGL 运行环境

Context 保存一组 OpenGL 状态和资源编号集合，并把当前线程的 OpenGL 调用连接到驱动。线程可以先理解为 CPU 正在独立执行的一条代码流程；“Context 在当前线程 Current”表示这条流程里的 OpenGL 调用知道应使用哪套状态和资源。没有 Current Context 时调用大多数 OpenGL 函数是无效的。

Context 包含或关联的状态例如：

- 当前绑定的 Program。
- 当前绑定的 VAO。
- 是否开启深度测试、混合、剔除。
- 当前 Viewport。
- 由该 Context 创建的 Buffer、Texture 等对象。

Context 不是窗口。一个窗口通常在创建时附带一个 Context，但多个 Context 可共享资源，离屏 Context 也可能没有普通可见窗口。

#### Default Framebuffer：窗口对应的画布

窗口系统提供的默认 Framebuffer 通常有前后颜色缓冲，也可能有深度/模板缓冲。OpenGL 向默认 Framebuffer 绘制后，通过 Swap Buffers 展示。

#### GPU Resources：被绘制使用的数据与程序

Buffer、Texture、Vertex Array、Shader Program 等资源由应用显式创建和删除。窗口存在不代表这些资源自动存在。

### 7.4 GLFW、GLAD 与驱动各自做什么

课程常用三层工具：

| 层 | 职责 | 不负责什么 |
| --- | --- | --- |
| GLFW | 跨平台创建窗口/Context，处理输入与事件 | 不加载模型，不写 Shader，不渲染场景 |
| GLAD 等函数加载器 | 从当前 Context/驱动取得 OpenGL 函数地址 | 不创建 Context，不替你管理资源 |
| OpenGL 驱动 | 实现 API 并把命令交给 GPU | 不知道你的游戏对象和关卡规则 |

为什么需要函数加载器？在 Windows 和 Linux 上，许多 OpenGL 函数地址由当前平台与驱动在运行时提供，C++ 编译器只看到函数声明还不够。必须先创建并设为 Current 的 Context，再让加载器查询函数地址，顺序不能反。

**macOS 是例外，也是本书的路线**：系统直接提供固定的 OpenGL 4.1，`#include <OpenGL/gl3.h>` 就拿到了全部函数声明和地址，不需要 GLAD。所以上表中间那一层在本书的可运行代码里是空的。列出它是为了让你读别人的教程时知道那一步在做什么。

本书采用的顺序（macOS）：

~~~text
初始化 GLFW
-> 设置版本和 Profile Hint
-> 创建 Window + Context
-> 让 Context 在当前线程 Current
-> 才能正常创建资源和绘制
~~~

Windows/Linux 只需在最后一步之前插入"加载 OpenGL 函数"。

### 7.5 OpenGL 版本与 Core Profile

课程面向 macOS，系统 OpenGL 最高为 4.1。创建时应请求 OpenGL 4.1 Core Profile，并在 macOS 上启用 forward-compatible：

~~~cpp
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
~~~

Profile 是同一 OpenGL 版本下选择哪组功能规则的配置。Core Profile 表示只使用现代核心 API，不依赖旧的立即模式 `glBegin/glEnd` 等兼容功能。课程选择它是为了让顶点数据、Shader 和状态都显式可见。

若创建失败，应输出 GLFW 错误，而不是盲目更换 Shader。

### 7.6 完整示例：最小窗口与清屏程序

开始前先确认依赖已安装。macOS 上只需要两个库，OpenGL 本身由系统提供：

~~~zsh
brew install glfw glm ninja
~~~

完整的环境搭建步骤见 [macOS 上用 VS Code 开发](../plan/02-vscode-on-macos.md)。第一次从空文件夹建立 C++ 工程见 [VS Code 中的第一个 C++ 工程](../plan/03-first-cpp-project-in-vscode.md)。

关于头文件，这里要先解决一个初学时几乎必然踩到的问题：**OpenGL 的函数地址在不同平台上获取方式不同**。

- macOS 由系统提供固定的 OpenGL 4.1 框架，直接 `#include <OpenGL/gl3.h>` 就能拿到全部 4.1 函数，不需要函数加载器。
- Windows 和 Linux 需要一个加载器（常用 GLAD）在运行时查询函数地址，代码里会多出 `#include <glad/glad.h>` 和一次 `gladLoadGLLoader`。

本书的可运行路线是 macOS，因此**下面的示例不使用 GLAD**，与 [exercises/common/gl_support.hpp](../exercises/common/gl_support.hpp) 完全一致。看到别处的教程带 GLAD 时，知道它在替你做 macOS 上不需要的那一步就够了。

`GLFW_INCLUDE_NONE` 的作用是让 GLFW 不要自己带一份 OpenGL 头，避免和下一行的系统头冲突。

~~~cpp
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>   // macOS 提供的 OpenGL 4.1 声明

#include <iostream>

void onGlfwError(int code, const char* description) {
    std::cerr << "GLFW error " << code << ": " << description << '\n';
}

void onFramebufferSize(GLFWwindow*, int width, int height) {
    // Retina 屏幕中 framebuffer 像素尺寸可能不同于窗口逻辑尺寸。
    // glViewport 告诉 OpenGL 把 NDC 映射到哪个像素矩形。
    glViewport(0, 0, width, height);
}

int main() {
    glfwSetErrorCallback(onGlfwError);
    if (glfwInit() != GLFW_TRUE) {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "CodeA", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, onFramebufferSize);
    glfwSwapInterval(1); // 请求垂直同步；实际行为由平台/驱动决定。

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n';

    while (!glfwWindowShouldClose(window)) {
        // 把当前 Draw Framebuffer 的颜色缓冲填成固定颜色。
        glClearColor(0.08f, 0.12f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
~~~

#### 逐段解释

这是全书第一个 OpenGL 程序，值得逐段读清楚。每一段只做一件事：

**准备阶段**

1. `glfwSetErrorCallback` 先注册错误回调。放在 `glfwInit` 之前，这样连初始化失败的原因也能打印出来。
2. `glfwInit` 初始化 GLFW 自己（窗口系统连接、输入、计时器）。此时还没有窗口，也还没有 OpenGL。

**描述想要什么样的 Context**

3. 四个 `glfwWindowHint` 只是"填表"，不创建任何东西。它们告诉 GLFW：下一个窗口要配 OpenGL 4.1、Core Profile、forward-compatible。写在 `glfwCreateWindow` 之后就完全无效。

**创建窗口和 Context**

4. `glfwCreateWindow` 一次做两件事：向 macOS 申请一个窗口，并按上面的表创建一个 OpenGL Context。返回 `nullptr` 说明这组要求无法满足，此时应该看错误回调的输出，而不是去改 Shader——这里还没有 Shader。
5. `glfwMakeContextCurrent` 把这个 Context 设为**当前线程的** Current Context。这一行之前调用任何 `gl*` 函数都是无效的。

**设置视口**

6. `glfwSetFramebufferSizeCallback` 注册尺寸变化回调。回调里调用 `glViewport`，窗口被拖动缩放时视口才会跟着变。
7. `glfwSwapInterval(1)` 请求垂直同步，见 7.8。
8. `glfwGetFramebufferSize` + `glViewport` 是**给第一帧用的**。回调只在尺寸"改变"时触发，程序刚启动时不会触发，所以这里必须手动设一次初始视口。漏掉这两行的典型现象是：画面只占窗口左下角，拖动一下窗口才正常。
9. `glGetString(GL_VERSION)` 打印驱动实际给的版本。这是确认"我真的拿到 4.1 Core"的唯一可靠方式，不要凭 Hint 就下结论。

**每帧循环**

10. `glfwWindowShouldClose` 读取关闭标志，点窗口关闭按钮后变为 true。
11. `glClearColor` 只**记录**清屏用什么颜色，它自己不改变任何像素。这是 OpenGL 典型的"先设状态、后执行命令"风格。
12. `glClear(GL_COLOR_BUFFER_BIT)` 才真正把颜色缓冲填成上一行记录的颜色。
13. `glfwSwapBuffers` 把画好的 Back Buffer 呈现出来，见 7.8。
14. `glfwPollEvents` 处理操作系统事件，见 7.9。少了它窗口会"未响应"。

**收尾**

15. `glfwDestroyWindow` 销毁窗口和它的 Context，`glfwTerminate` 释放 GLFW 自己的资源。

这段代码里只有第 11、12 两行是 OpenGL 命令，其余全是"搭好环境"和"维持循环"。这个比例在图形编程里很常见：真正下命令的代码往往只占一小部分。

`exercises/` 里的 [chapter07.cpp](../exercises/03_opengl_basics/chapter07.cpp) 把第 1–9 步和第 15 步封装进了 `codea::Window`，所以看起来短得多。它做的事和上面完全一样，第一次学习时应先读这个展开版。

第一次验收只应出现稳定背景色和可正常关闭的窗口。尚未创建顶点 Buffer，也没有 Draw Call。

### 7.7 Window Size 与 Framebuffer Size

在 Retina 等高 DPI 屏幕上，窗口逻辑尺寸 800×600 不一定等于实际 Framebuffer Pixel 尺寸。可能一个逻辑点对应多个物理 Pixel。

Projection 的 aspect 和 `glViewport` 应根据实际 Framebuffer Size 更新：

~~~text
aspect = framebufferWidth / framebufferHeight
~~~

若只读取窗口尺寸，画面可能比例错误或只占默认 Framebuffer 一部分。高度为 0 时窗口可能最小化，应避免除以零，并暂时跳过依赖尺寸的绘制。

### 7.8 双缓冲与 Swap Buffers

默认 Framebuffer 通常使用双缓冲：

- Front Buffer：显示器当前读取的已完成画面。
- Back Buffer：OpenGL 正在绘制的新画面。

一帧完成后 `glfwSwapBuffers` 让完成的 Back Buffer 被展示。这样用户不会在普通情况下看到同一缓冲被逐步改写的中间状态。

双缓冲本身不保证绝对没有 tearing（画面撕裂）。若交换发生在显示器扫描途中，仍可能看到上下来自不同帧。VSync（垂直同步）尝试让交换等待显示刷新边界，`glfwSwapInterval(1)` 是请求开启它。

Swap 不会替你 Clear 或 Draw，只负责呈现/交换缓冲。

### 7.9 事件循环为什么每帧都要 Poll

`glfwPollEvents` 让操作系统事件进入程序，更新关闭标志、键盘、鼠标和窗口尺寸回调。若长时间不处理事件，窗口会表现为无响应。

典型循环骨架：

~~~text
读取/处理事件
更新游戏状态
设置本帧渲染状态
Clear
Draw
Swap
重复
~~~

Poll 放在循环开头或结尾都可，但要采用固定顺序，并理解输入状态对应哪一帧。

#### 本部分检查点

**能做出来**：一个能稳定显示背景色、可以拖动缩放、点关闭按钮能正常退出的窗口。

**能回答**：

- Window、Context、Default Framebuffer 三者什么关系？
- 为什么 `glfwWindowHint` 必须写在 `glfwCreateWindow` 之前？
- 为什么初始视口要手动设一次，不能只靠回调？

**卡在这里就先不要往下读**：这是第 7.2～7.9 的全部要求，也是本章的最低完成点。窗口跑不起来就不要读资源部分——那些 API 在没有 Current Context 时全部无效。

## 第二部分：GPU 资源及其布局

### 7.10 OpenGL 对象名称只是句柄

OpenGL 常用 `GLuint` 保存对象名称：

~~~cpp
GLuint vertexBuffer = 0;
glGenBuffers(1, &vertexBuffer);
~~~

`vertexBuffer` 是驱动分配的非零整数标识（handle/name），不是 GPU 内存地址，也不包含顶点数据。后续把该名称绑定到目标，OpenGL 才知道在操作哪个对象。

对象通常经历：

~~~text
生成名称 -> 绑定 -> 分配/上传数据 -> 多帧重复使用 -> 删除
~~~

不要根据整数大小推测资源位置或顺序。

### 7.11 Buffer：GPU 可读取的一段数据存储

Buffer Object 是一块由 OpenGL 管理的数据存储，可保存顶点、索引、Uniform 等。它本身不知道字节代表 position 还是 color。

上传一个 float 数组：

~~~cpp
float positions[] = {
    -0.5f, -0.5f, 0.0f,
     0.5f, -0.5f, 0.0f,
     0.0f,  0.5f, 0.0f
};

GLuint vertexBuffer = 0;
glGenBuffers(1, &vertexBuffer);
glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
glBufferData(GL_ARRAY_BUFFER,
             sizeof(positions),
             positions,
             GL_STATIC_DRAW);
~~~

逐行含义：

1. `glGenBuffers` 创建一个 Buffer 名称。
2. `glBindBuffer(GL_ARRAY_BUFFER,...)` 让它成为当前 Array Buffer。
3. `glBufferData` 为当前绑定对象分配 `sizeof(positions)` 字节并复制 CPU 数据。
4. `GL_STATIC_DRAW` 是使用提示：数据很少改、会被绘制多次。它不禁止以后更新。

调用完成后，CPU 数组仍属于 CPU；OpenGL Buffer 有自己的数据副本。局部数组离开作用域不会删除 GPU Buffer。

不用时：

~~~cpp
glDeleteBuffers(1, &vertexBuffer);
vertexBuffer = 0;
~~~

必须在相关 Context 仍有效时删除。

### 7.12 Vertex 与 Attribute Layout

GPU Buffer 只是一串字节。要让顶点 Shader 知道怎样读取，需要定义 Vertex Attribute Layout（顶点属性布局）。例如：

~~~cpp
struct Vertex {
    float position[3]; // 12 字节
    float color[3];    // 12 字节
};
~~~

若结构紧密排列，一条 Vertex 共 24 字节：

~~~text
Vertex 0: px py pz cr cg cb
Vertex 1: px py pz cr cg cb
Vertex 2: px py pz cr cg cb
~~~

关键术语：

- Attribute：一个顶点字段，例如 position 或 color。
- Location：Shader 输入槽编号，例如 position 在 0、color 在 1。
- Component Count：字段有几个数，vec3 是 3。
- Type：每个数的数据类型，例如 `GL_FLOAT`。
- Stride：相邻两个 Vertex 起点相隔多少字节。
- Offset：字段相对当前 Vertex 起点偏移多少字节。

### 7.13 VAO 保存“怎样解释顶点输入”

VAO 是 Vertex Array Object。它不保存 Vertex 字节，而是保存顶点输入配置以及相关 Buffer 绑定。

~~~cpp
GLuint vao = 0;
glGenVertexArrays(1, &vao);
glBindVertexArray(vao);

glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

glEnableVertexAttribArray(0);
glVertexAttribPointer(
    0,                              // Shader location
    3,                              // 三个 float
    GL_FLOAT,
    GL_FALSE,                       // 不做整数归一化
    sizeof(Vertex),                 // stride
    reinterpret_cast<void*>(offsetof(Vertex, position)));

glEnableVertexAttribArray(1);
glVertexAttribPointer(
    1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    reinterpret_cast<void*>(offsetof(Vertex, color)));
~~~

`glVertexAttribPointer` 调用时会记录当时绑定的 Array Buffer 与读取规则。之后绘制只需绑定该 VAO，OpenGL 就能恢复这套顶点输入说明。

使用 `offsetof` 比手写 `12` 更可靠，因为 C++ 结构可能有 padding（为对齐插入的空字节）。对用于直接上传的 Vertex 结构仍应确认它是标准布局类型。

### 7.14 Index Buffer 与顶点复用

Index Buffer 保存顶点编号。一个四边形拆成两个三角形时：

~~~cpp
unsigned int indices[] = {
    0, 1, 2,
    2, 3, 0
};
~~~

上传目标为 `GL_ELEMENT_ARRAY_BUFFER`。它的绑定是 VAO 状态的一部分，因此应在目标 VAO 已绑定时设置：

~~~cpp
glBindVertexArray(vao);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
~~~

绘制时 `glDrawElements` 读取索引，再通过 VAO 布局从 Vertex Buffer 读取属性。Index Buffer 只保存整数，不保存重复 Vertex 内容。

### 7.15 Texture 与 Program 也是 GPU 资源

本章先建立分类，具体 Shader 在第 8 章实现：

| 资源 | 保存什么 | 常见创建/销毁函数 |
| --- | --- | --- |
| Buffer | 原始字节：顶点、索引、参数等 | `glGenBuffers/glDeleteBuffers` |
| Vertex Array | 顶点输入解释规则 | `glGenVertexArrays/glDeleteVertexArrays` |
| Texture | texel、尺寸、格式与采样设置 | `glGenTextures/glDeleteTextures` |
| Shader | 单个 Shader 阶段编译结果 | `glCreateShader/glDeleteShader` |
| Program | 多个 Shader 阶段链接后的可执行组合 | `glCreateProgram/glDeleteProgram` |
| Framebuffer Object | 一组可作为渲染目标的附件关系 | 后续章节学习 |

“GPU 资源”不一定物理上全部只在独立显存；统一内存架构下 CPU/GPU 可能共享物理内存。对 API 使用者而言，关键仍是由谁拥有、何时上传、何时可用和何时删除。

#### 本部分检查点

**能做出来**：在纸上按字节画出一条 `position(3) + color(3)` 的 Vertex 记录，标出 stride 和每个属性的 offset。

**能回答**：

- `GLuint` 保存的是 GPU 地址还是一个编号？
- Buffer 自己知道里面的字节是 position 还是 color 吗？
- VAO 记录的是数据还是“怎样读数据”？

**卡在这里就先不要往下读**：stride 和 offset 是第 8、9 章出错最多的地方。宁可多画两个例子。

## 第三部分：状态、生命周期与调试

### 7.16 OpenGL 是状态机

State Machine（状态机）表示许多调用修改“当前状态”，之后的 Draw 使用那一刻的状态。

~~~cpp
glUseProgram(program);          // 当前 Program
glBindVertexArray(vao);         // 当前 VAO
glEnable(GL_DEPTH_TEST);        // 当前开启深度测试
glDrawElements(...);            // 读取以上状态并提交绘制
~~~

Bind 通常不等于 Draw。它只是选择后续操作和绘制要使用的对象。

状态错误的难点是：遗漏一次绑定时，程序可能继续使用上一个对象，结果取决于之前画过什么。初学阶段应把每次 Draw 所需状态集中设置，避免依赖遥远代码留下的“恰好正确”状态。

### 7.17 CPU/GPU 生命周期与资源所有权

模型资源通常这样管理：

~~~text
加载阶段：读取文件 -> 创建 Buffer/Texture -> 上传一次
每帧：绑定并重复绘制
卸载阶段：删除 Buffer/Texture/VAO/Program
~~~

不要每帧重新 `glGenBuffers`、上传不变 Mesh、再删除。这会增加分配、传输和驱动管理成本。

一个资源应有一个明确所有者。当前练习先在 `main` 退出前按创建的反顺序调用 `glDelete*`，把顺序看清楚。等资源数量变多后，再用不可复制、可移动的 C++ RAII 类自动调用对应 `glDelete*`。RAII 指“资源获取即初始化”：拥有资源的 C++ 对象结束生命周期时自动释放资源。

无论手动删除还是使用 RAII，删除发生时都必须仍有有效 OpenGL Context。顺序始终是：先释放 GPU 资源，再销毁 Window/Context。

### 7.18 错误报告：先确认能力再启用回调

较新的 OpenGL 提供 Debug Output，可由驱动回调报告错误和性能提示。`glDebugMessageCallback` 在 OpenGL 4.3 成为核心；本课程请求 4.1，因此必须检查加载器是否暴露相应扩展/函数，不能假设所有 macOS 环境都可用。

创建 Context 前可请求 Debug Context：

~~~cpp
glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
~~~

加载函数后，若 Debug Output 可用，再注册回调。不可用时使用 `glGetError` 做阶段性检查，并始终检查 Shader 编译/链接日志。

~~~cpp
void checkGlError(const char* label) {
    for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {
        std::cerr << label << ": OpenGL error 0x"
                  << std::hex << error << std::dec << '\n';
    }
}
~~~

`glGetError` 只给错误码，信息较少，而且读取会清掉错误标志；应在小范围操作后调用，才能定位来源。不要每个 Fragment 调用，它是 CPU API 检查，不属于 Shader。

### 7.19 第一帧应该按什么顺序发生

~~~text
程序启动
  创建 Window/Context
  加载 OpenGL 函数
  查询并打印版本
  创建 Program、VAO、Buffer、Texture

每帧
  Poll Events
  更新 CPU 游戏数据
  设置 Viewport/Clear/深度等状态
  绑定 Program 与 VAO
  提交 Draw Call
  Swap Buffers

程序退出
  删除所有 GPU 资源
  销毁 Window/Context
  终止 GLFW
~~~

资源创建失败时应停止并报告原因，不要进入循环显示一个无法解释的黑屏。

### 7.20 黑屏分层排查

在画第一个三角形前，先按层验证：

1. Window 能否出现、关闭、缩放？
2. `glGetString(GL_VERSION)` 是否为预期版本？
3. `glClearColor` 是否能每秒改变颜色？
4. Framebuffer Size 与 Viewport 是否非零？
5. Shader 是否编译链接成功？
6. VAO Attribute 是否与 Shader Location 一致？
7. Draw Count、Index 类型、Buffer 大小是否一致？
8. 三角形坐标是否在 Clip/NDC 可见范围？

从第一层失败时，不要跳去调整相机矩阵。

### 7.21 常见错误

| 现象 | 原因 | 修正 |
| --- | --- | --- |
| 加载 OpenGL 函数失败 | Context 尚未 Current | 先 `glfwMakeContextCurrent` |
| macOS 创建窗口失败 | 未设置 4.1 Core/forward-compatible | 检查 Window Hint 与错误回调 |
| 缩放后只画左下角 | 未更新 `glViewport` | 使用 Framebuffer Size 回调 |
| VAO 绑定后仍读错数据 | stride、offset、location 错 | 画出字节布局并与 Shader 对照 |
| Index Draw 随机越界 | Index 类型或数量与 Buffer 不符 | `GL_UNSIGNED_INT` 对应 uint32，count 是索引数 |
| 程序退出时崩溃 | Context 先销毁，资源析构后调用 GL | 先释放 GPU 对象 |
| 每帧明显卡顿 | 每帧重建并上传静态资源 | 加载一次、重复绑定绘制 |
| 只看到旧错误 | 没有在小范围及时读取错误/日志 | 每个初始化阶段立即验证 |

### 7.22 随读自检

1. GLFW 与 OpenGL 驱动的职责有什么不同？
2. 为什么必须先让 Context Current，再加载函数？
3. Buffer 是否知道内部前三个 float 是 position？
4. VAO 保存 Vertex 数据本身吗？
5. 为什么 `glfwSwapBuffers` 不能替代 Draw Call？

参考答案：

1. GLFW 管窗口/Context/输入；驱动实现图形 API 并提交 GPU 命令。
2. 函数地址和能力与当前 Context/驱动相关。
3. 不知道，Buffer 只保存字节。
4. 不保存，它记录怎样从相关 Buffer 解释 Attribute。
5. Swap 只呈现已绘制缓冲，不产生几何和颜色。

### 7.23 练习

#### 练习 A：稳定清屏

创建 800×600 窗口，每秒在三种背景色间切换。缩放和最小化后恢复时 Viewport 仍正确。

#### 练习 B：环境报告

启动时打印 OpenGL Version、GLSL Version、Renderer 和 Vendor。把结果记录到学习日志，避免之后混淆平台能力。

#### 练习 C：Buffer 生命周期

创建一个 Buffer，上传 9 个 float，检查无错误后删除。用日志明确记录创建、上传、删除发生在哪个阶段。

#### 练习 D：手画 Vertex 字节布局

为 `position+color+uv` 的 Vertex 结构列出每个字段的 component count、type、stride、offset 和 Shader location，再用 `sizeof/offsetof` 打印验证。

#### 练习 E：故意出错实验（破坏资源与状态）

分别在独立运行中：请求不可用版本、把 Attribute stride 写错、在无有效 Context 时尝试初始化加载器。记录每种错误最早能从哪里观察到；恢复后再进入下一步。

### 7.24 本章验收标准

第一次阅读的最低验收：

- 能画图说明 Window、Context、默认 Framebuffer 和 GPU Resource 的关系。
- 程序可稳定创建 OpenGL 4.1 Core Context、清屏、缩放、关闭。
- 能解释 GLFW、函数加载器和驱动各自负责什么。

继续阅读资源部分后的验收：

- 能解释每一行 Buffer 上传代码改变的是绑定状态、GPU 存储还是 CPU 数据。
- 能为一个交错 Vertex 结构正确计算 stride 和 offset。
- GPU 资源只在初始化/卸载阶段创建和删除，静态资源不每帧重建。
- 程序退出前释放 GPU 资源，且错误日志中没有未处理的初始化失败。

下一章会创建 Shader Program，并把这里的 Buffer 与 VAO 真正交给一条 Draw Call，画出第一个 GPU 三角形。
