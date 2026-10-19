# 第 8 章：GLSL 与第一条 Draw Call

<!-- nav -->
> [上一章：OpenGL Context 与 GPU 资源](07-opengl-context-resources.md) ｜ [下一章：网格、贴图与 glTF 资产](09-mesh-model-assets.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/03_opengl_basics/08-shaders-first-draw.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

本章把第 7 章创建的 Buffer、VAO 和 Context 连成一条真正的 GPU 渲染管线。最终结果是一个由 GPU 顶点 Shader 定位、由光栅器填充、由片段 Shader 着色的三角形。

先只画一个不需要相机的彩色三角形。它虽然简单，却能验证窗口、Context、函数加载、顶点字节、VAO、Shader 编译链接、Draw Call 和 Framebuffer 整条链路。

## 本章导读

### 8.1 本章学习目标

完成本章后，应能：

1. 解释 Shader 在 CPU 还是 GPU 上运行，以及每种 Shader 运行多少次。
2. 读懂最小 GLSL 顶点和片段 Shader 的每个词。
3. 正确检查 Shader 编译与 Program 链接日志。
4. 将 VAO Attribute Location 与 Shader `in` 对齐。
5. 解释一次 `glDrawArrays` 从哪些当前状态读取数据。
6. 用 Uniform 传入颜色和 MVP 矩阵。
7. 按层定位第一帧黑屏，而不是随机修改代码。

本章分三部分：先理解和编译两个 Shader 阶段，再提交第一条 Draw，最后加入 Uniform、MVP、Index 和渲染状态。

本章有两个完成点：

- 第一次完成：做到 `8.14`，窗口中出现稳定的彩色 Triangle，并能解释它的数据路径。
- 第二次完成：继续到 `8.18`，用 MVP、Index 和 Depth 画出 3D Cube。

如果第一个 Triangle 还是黑屏，不要继续加入 Camera 和 Cube。

## 第一部分：Shader 的输入、输出与编译

### 8.2 Shader 是 GPU 执行的小程序

Shader 是运行在 GPU 某个管线阶段的程序。C++ 代码运行在 CPU，负责创建资源、准备数据和提交命令；GLSL Shader 由驱动在运行时编译，之后由 GPU 大量执行。

本章只需要两个阶段：

| 阶段 | 一次 Draw 中大致执行次数 | 主要职责 |
| --- | --- | --- |
| Vertex Shader | 每个提交的 Vertex 一次 | 计算 `gl_Position`，输出待插值属性 |
| Fragment Shader | 每个被三角形覆盖的 Fragment 一次 | 根据插值属性计算候选颜色 |

一个由 3 个顶点组成的大三角形可能只运行 3 次 Vertex Shader，却运行几十万次 Fragment Shader。Shader 代码相同，但每次 invocation（一次独立执行实例）接收的数据不同。

你不需要在 C++ 中逐 Vertex 或逐 Fragment 调用 Shader。一次 Draw Call 指定顶点范围，GPU 自动安排 invocation 和并行执行。

### 8.3 GPU 管线中可编程与固定功能部分

第一条管线可以画成：

~~~text
Vertex Buffer + VAO
  -> Vertex Shader（你写）
  -> Primitive Assembly（GPU 按每 3 个顶点组 Triangle）
  -> Clipping（GPU 固定规则）
  -> Perspective Divide / Viewport（GPU 固定规则）
  -> Rasterization（GPU 固定规则，产生 Fragment）
  -> Fragment Shader（你写）
  -> Depth / Stencil / Blend（GPU 可配置的固定规则）
  -> Framebuffer
~~~

“固定功能”不是不可控制，而是不用自己写每个 Pixel 循环；你通过 OpenGL 状态选择是否开启深度、混合、剔除等规则。

这里暂时出现了两个后续名词：Stencil Test（模板测试）用每个 Pixel 附带的小整数标记限制哪些区域允许写入；Blend（混合）按公式把新 Fragment 颜色与 Framebuffer 旧颜色组合，常用于透明。第一条不透明三角形不需要配置它们。

### 8.4 GLSL 是什么

GLSL 是 OpenGL Shading Language。它的语法类似 C，但运行环境和类型规则不同。

常用类型：

| GLSL 类型 | 含义 | 示例 |
| --- | --- | --- |
| `float` | 单个浮点数 | `0.5` |
| `int` | 有符号整数 | `3` |
| `bool` | 布尔值 | `true` |
| `vec2` | 两个 float | UV、二维位置 |
| `vec3` | 三个 float | 三维位置、RGB、法线 |
| `vec4` | 四个 float | 齐次位置、RGBA |
| `mat4` | 4×4 float 矩阵 | Model/View/Projection |

构造向量：

~~~glsl
vec3 position = vec3(1.0, 2.0, 3.0);
vec4 point = vec4(position, 1.0); // 组合 vec3 和一个 w
~~~

Swizzle（分量重排/选择）允许：

~~~glsl
vec3 rgb = color.rgb;
vec2 xy = position.xy;
vec3 bgr = color.bgr;
~~~

它只是读取或重排分量，不会自动做颜色空间转换。

### 8.5 `#version 410 core`

每个 Shader 第一行通常为：

~~~glsl
#version 410 core
~~~

含义：

- `410`：使用 GLSL 4.10，对应本课程的 OpenGL 4.1。
- `core`：采用 Core Profile 语言规则。

版本行必须位于最前面，前面不要有其他普通字符。若请求的 Context 版本低于 Shader 要求，编译会失败。

### 8.6 最小顶点 Shader 逐行解释

~~~glsl
#version 410 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

out vec3 vColor;

void main() {
    gl_Position = vec4(aPosition, 1.0);
    vColor = aColor;
}
~~~

#### `layout(location=0) in vec3 aPosition`

- `in`：这个 Shader invocation 的输入。
- `vec3`：输入由三个 float 组成。
- `aPosition`：变量名，前缀 a 表示 attribute，只是命名习惯。
- `location=0`：输入槽编号，必须与 VAO 的 Attribute 0 配置对应。

第 7 章的 `glVertexAttribPointer(0,3,GL_FLOAT,...)` 告诉 OpenGL怎样从 Buffer 提供 Location 0；Shader 这行告诉 GPU Location 0 要进入 `aPosition`。两边共同组成数据契约。

#### `out vec3 vColor`

`out` 表示顶点 Shader 交给后续阶段的数据。三个顶点各输出一份颜色，Rasterizer 会在三角形内部插值，片段 Shader 接收插值结果。前缀 v 表示 varying，也是命名习惯。

#### `void main()`

每个 Shader 都从 `main` 入口执行。GPU 自动为每个 Vertex 调用，不由 C++ 直接调用。

#### `gl_Position`

`gl_Position` 是顶点 Shader 必须写的内建输出，类型为 `vec4`，含义是 Clip Space Position。

本例直接把 `aPosition` 扩展成 w=1，因此输入数字被当作 Clip 坐标。因为 w=1，透视除法后 NDC 与 xyz 相同。只要位置在 `[-1,1]` 附近，三角形可见。

真实 3D 顶点会写：

~~~glsl
gl_Position = projection * view * model * vec4(aPosition, 1.0);
~~~

### 8.7 最小片段 Shader 逐行解释

~~~glsl
#version 410 core

in vec3 vColor;
out vec4 outColor;

void main() {
    outColor = vec4(vColor, 1.0);
}
~~~

- `in vColor` 必须与顶点 Shader 的输出类型和接口匹配。
- `outColor` 是写给当前颜色附件的候选 RGBA。
- Alpha 固定为 1，表示完全不透明输入；若未开启 Blend，它仍只是作为颜色第四分量保存。

片段 Shader 输出后还可能被 Depth、Stencil 等测试丢弃，所以 Fragment Shader 的运行不保证最终 Pixel 被修改。

### 8.8 Varying 插值对应 CPU 重心坐标

顶点 Shader 对三个顶点分别输出：

~~~text
A: vColor=(1,0,0)
B: vColor=(0,1,0)
C: vColor=(0,0,1)
~~~

某 Fragment 的屏幕重心权重若为 `(0.5,0.25,0.25)`，默认 `smooth` 插值会进行透视正确计算，概念上得到接近：

~~~text
vColor = 0.5*red + 0.25*green + 0.25*blue
       = (0.5,0.25,0.25)
~~~

这正是第 5、6 章自己实现的属性插值。GPU 自动完成计算，但概念没有改变。

GLSL 还可声明 `flat`，表示不插值，整个 Triangle 使用某个 provoking vertex（规定的代表顶点）的值。整数 varying 通常必须使用 `flat`。第一条三角形使用默认 `smooth`。

### 8.9 编译 Shader：从文本到单阶段结果

Compile（编译）把 GLSL 源代码文本检查并转换为驱动可使用的单个 Shader 阶段结果。它发生在程序运行时，不是 C++ 编译器完成的。

~~~cpp
GLuint compileShader(GLenum type, const char* source) {
    // 输入：阶段类型（如 GL_VERTEX_SHADER）和以 \0 结尾的源码文本。
    // 输出：成功时返回 Shader Object；失败时打印日志、删除对象并抛错。
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

        std::string log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());

        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed:\n" + log);
    }
    return shader;
}
~~~

必须检查 `GL_COMPILE_STATUS`，不能用“Shader Object id 非零”判断成功。对象能创建不代表源码合法。

常见编译错误：漏分号、类型不匹配、变量未声明、`#version` 不支持、括号不配对。日志通常包含行号，应打印完整日志和 Shader 文件名/阶段。

### 8.10 链接 Program：检查阶段之间能否合作

Link（链接）把 Vertex Shader 与 Fragment Shader 等阶段组合成一个 Program，并验证阶段接口是否匹配。

~~~cpp
GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader) {
    // 输入：两个已经成功编译的 Shader Object。
    // 输出：成功链接的 Program Object。
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error("Program link failed:\n" + log);
    }
    return program;
}
~~~

链接失败常见原因：Vertex `out` 与 Fragment `in` 名称/类型不匹配，或阶段组合不合法。

Program 成功链接后，可以删除原 Shader Object；已链接的 Program 保留所需结果：

~~~cpp
GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
GLuint program = linkProgram(vertexShader, fragmentShader);

glDeleteShader(vertexShader);
glDeleteShader(fragmentShader);
~~~

删除 Shader Object 不等于删除 Program。退出时仍要 `glDeleteProgram(program)`。

#### 本部分检查点

**能做出来**：Shader 编译和 Program 链接都成功，失败时能打印出带行号的错误信息。

**能回答**：

- Vertex Shader 和 Fragment Shader 各自的必需输出是什么？
- `in`、`out`、`uniform` 三种限定符分别表示数据从哪来？
- 链接失败和编译失败分别说明什么问题？

**卡在这里就先不要往下读**：编译错误日志没打出来就不要往下写。图形编程里“看不到错误信息”比“有错误”更耗时间。

## 第二部分：提交第一条 Draw Call

### 8.11 第一组三角形顶点数据

使用已经位于可见 NDC/Clip 范围的坐标：

~~~cpp
struct Vertex {
    float position[3];
    float color[3];
};

Vertex vertices[] = {
    {{-0.6f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.6f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.0f,  0.6f, 0.0f}, {0.0f, 0.0f, 1.0f}}
};
~~~

每行是一份 Vertex：前三个 float 是 position，后三个是 color。数组顺序为逆时针：左下 -> 右下 -> 上方。

它们不是 Pixel 坐标。Vertex Shader 写 w=1 后，x/y 直接成为 NDC；Viewport 再自动映射到窗口像素。

### 8.12 创建 VAO 与 VBO

~~~cpp
GLuint vao = 0;
GLuint vbo = 0;

glGenVertexArrays(1, &vao);
glGenBuffers(1, &vbo);

glBindVertexArray(vao);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

glEnableVertexAttribArray(0);
glVertexAttribPointer(
    0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    reinterpret_cast<void*>(offsetof(Vertex, position)));

glEnableVertexAttribArray(1);
glVertexAttribPointer(
    1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    reinterpret_cast<void*>(offsetof(Vertex, color)));

glBindVertexArray(0);
~~~

此后数据契约是：

| Buffer 字段 | VAO Location | Shader 输入 |
| --- | ---: | --- |
| `position[3]` | 0 | `layout(location=0) in vec3 aPosition` |
| `color[3]` | 1 | `layout(location=1) in vec3 aColor` |

任一侧 component count、type、stride、offset 或 location 错误，GPU 都会读取错误字节。

### 8.13 第一条 Draw Call

每帧：

~~~cpp
glClearColor(0.08f, 0.12f, 0.18f, 1.0f);
glClear(GL_COLOR_BUFFER_BIT);

glUseProgram(program);
glBindVertexArray(vao);
glDrawArrays(GL_TRIANGLES, 0, 3);

glfwSwapBuffers(window);
~~~

`glDrawArrays(GL_TRIANGLES,0,3)` 参数逐项解释：

- `GL_TRIANGLES`：每连续三个 Vertex 组成独立三角形。
- `0`：从顶点编号 0 开始。
- `3`：读取三个 Vertex，不是三个 float，也不是三个 Triangle。

Draw Call 会读取当时的：

- 当前 Program。
- 当前 VAO 及其 Attribute/Buffer 关系。
- 当前 Framebuffer 和 Viewport。
- 当前深度、剔除、混合等状态。
- Uniform、Texture 等参数。

它不会自动寻找名字叫 `vertices` 的 C++ 数组；数组内容早已通过 VBO 上传，VAO 告诉 Draw 怎样读取。

### 8.14 一份顶点怎样走到屏幕

以左下 Vertex 为例：

~~~text
CPU Vertex.position = (-0.6,-0.5,0)
-> VBO 保存对应字节
-> VAO Location 0 解释三个 float
-> Vertex Shader aPosition=(-0.6,-0.5,0)
-> gl_Position=(-0.6,-0.5,0,1)
-> Clip 条件全部通过
-> 透视除法，NDC=(-0.6,-0.5,0)
-> Viewport 映射到窗口左下区域
~~~

三个 Vertex 都完成后，Primitive Assembly 组成一个 Triangle。Rasterizer 使用与 CPU 边函数/重心坐标相同的几何意义生成 Fragment；颜色被插值；Fragment Shader 输出候选颜色；结果写入默认 Framebuffer；Swap 后才可见。

#### 本部分检查点

**能做出来**：屏幕上出现第一个三角形，顶点颜色平滑过渡。

**能回答**：

- 从 VAO 到屏幕像素，中间经过哪几个阶段？
- 为什么改了 VBO 里的数据但不重新 bind，画面也会变？
- 三角形消失时，最先该检查哪三件事？

**卡在这里就先不要往下读**：黑屏是本章最常见的结果。按 8.14 的顺序反向查：Framebuffer -> Program -> VAO -> 顶点坐标范围，一次只改一处。

## 第三部分：从固定三角形扩展到 3D Mesh

### 8.15 Uniform：一次 Draw 共享的参数

Uniform 是由 CPU 设置、对同一 Draw 中所有 Shader invocation 保持相同的参数。

片段 Shader：

~~~glsl
uniform vec4 uTint;

void main() {
    outColor = vec4(vColor, 1.0) * uTint;
}
~~~

C++ 在链接后查询位置并设置：

~~~cpp
GLint tintLocation = glGetUniformLocation(program, "uTint");
if (tintLocation < 0) {
    throw std::runtime_error("uTint was not found or was optimized out");
}

glUseProgram(program);
glUniform4f(tintLocation, 1.0f, 0.8f, 0.8f, 1.0f);
~~~

Uniform Location 属于特定 Program，应在 Program 成功链接后查询并缓存，不要为每个 Fragment或每帧重复按名字查找。

Location 为 -1 可能是拼写错误，也可能是变量声明后未影响最终输出，被编译器优化掉。设置 Uniform 前要确保正确 Program 已 Current。

Uniform 适合：

- MVP 矩阵。
- 每个对象颜色。
- 相机位置。
- 少量灯光参数。

它不适合通过成千上万次零散调用传输海量对象数据，后续会学习 Uniform Buffer 和实例数据。

### 8.16 MVP Uniform：从固定三角形走向 3D

顶点 Shader 改为：

~~~glsl
#version 410 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

uniform mat4 uMvp;
out vec3 vColor;

void main() {
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vColor = aColor;
}
~~~

CPU 计算：

~~~text
uMvp = projection * view * model
~~~

然后上传 16 个连续 float。OpenGL 的 `glUniformMatrix4fv` 需要你明确内存布局：

~~~cpp
glUseProgram(program);
glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, columnMajorData);
~~~

- `1`：上传一个 mat4。
- `GL_FALSE`：提供的数据已经按 OpenGL 期望的列主序解释。
- `columnMajorData`：指向 16 个连续 float。

第 2 章示例 `float m[4][4]` 按 C++ 行连续存储，不能因为数学采用列向量就直接假设它与上传布局一致。必须选择一种明确方案：

1. 在数学库内部就保存为 OpenGL 所需列主序。
2. 上传前转置/转换到列主序数组。
3. 使用已验证约定的数学库，并为已知矩阵写测试。

症状为“平移出现在错误行/列”时，优先检查数学约定与内存布局，不要修改 Shader 中乘法顺序碰运气。

### 8.17 Indexed Draw：按索引组装三角形

立方体等 Mesh 会复用顶点，使用 Index Buffer 和：

~~~cpp
glBindVertexArray(cubeVao);
glDrawElements(GL_TRIANGLES,
               indexCount,
               GL_UNSIGNED_INT,
               nullptr);
~~~

- `indexCount` 是索引个数；12 个 Triangle 共有 36 个索引。
- `GL_UNSIGNED_INT` 必须与 Index Buffer 元素类型匹配。
- `nullptr` 表示从当前 Element Array Buffer 的字节偏移 0 开始。

Index Buffer 绑定记录在 VAO 中。若在 VAO 未绑定时错误修改 Element Array Binding，Draw 可能读不到预期索引。

### 8.18 深度与背面状态映射到 CPU 章节

画 3D 立方体前启用：

~~~cpp
glEnable(GL_DEPTH_TEST);
glDepthFunc(GL_LESS);

glEnable(GL_CULL_FACE);
glCullFace(GL_BACK);
glFrontFace(GL_CCW);
~~~

每帧同时清：

~~~cpp
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
~~~

这些就是 CPU 光栅器中的：

- Depth Buffer 初始化为最远值。
- 新 Fragment 使用 Less 比较。
- CCW 视为正面，背面跳过。

若窗口创建时没有请求深度缓冲位数，默认 Framebuffer 可能没有可用深度附件。GLFW 可在创建前用 `GLFW_DEPTH_BITS` 提示；创建后也应查询实际能力。

### 8.19 Shader 源码文件与错误上下文

最初可把 Shader 写成 C++ 字符串以减少文件路径问题；很快应拆成 `.vert` 和 `.frag` 文件，并让加载器报告：

- 文件绝对或相对路径。
- Shader 阶段。
- 完整编译日志。
- 最好带行号的源代码片段。

工作目录取决于 VS Code/CMake 的启动设置。若文件打不开，应明确报“无法读取路径”，不能把空字符串继续交给编译器再误认为 GLSL 语法错。

Shader 热重载可以稍后实现。第一版失败时保留旧 Program 比直接替换成无效 Program 更安全。

### 8.20 Draw Call 与性能的初步认识

一次 Draw Call 有 CPU/驱动提交成本，也有 GPU 顶点、片段和带宽成本。不能只说“Draw Call 越少越快”：

- 一万个极小 Draw 可能受 CPU 提交限制。
- 一个覆盖全屏、片段 Shader 极复杂的 Draw 可能受 GPU 限制。
- 大量不可见三角形可能浪费顶点和光栅工作。

当前阶段只记录 Draw 数量，不做过早批处理。先让每条 Draw 的输入、状态和输出完全可解释。

### 8.21 macOS 上的调试现实

RenderDoc 不原生支持 macOS OpenGL 捕获，因此本课程不能把它作为必需验收工具。基础调试依赖：

- GLFW 错误回调。
- Shader/Program 完整日志。
- `glGetError` 或可用的 Debug Output。
- 把中间值编码为颜色。
- 把复杂场景缩成一个 Triangle。
- 根据当前 Xcode/macOS 能力选择可用的 GPU 捕获工具，但不依赖它完成基本练习。

最可靠的第一原则仍是逐层验证数据契约。

### 8.22 黑屏检查清单

按顺序检查，不要同时修改多处：

1. Clear Color 能否显示？若不能，问题在 Draw 之前。
2. Context/GLSL 版本是否正确打印？
3. Vertex 和 Fragment Shader 是否都编译成功？
4. Program 是否链接成功并 `glUseProgram`？
5. VAO 是否在 Draw 前绑定？
6. Location 0/1 是否与 Shader 对应？
7. Stride、Offset、Component Count 是否匹配 Vertex 字节？
8. Draw Count 是否是 3，模式是否 `GL_TRIANGLES`？
9. `gl_Position` 是否在合理 Clip 范围，w 是否为正且不接近 0？
10. Viewport 是否使用实际 Framebuffer Size？
11. 是否被错误的剔除或深度状态丢弃？可暂时分别关闭验证。
12. Fragment 输出 Alpha/RGB 是否为可见值？

可以临时把 Vertex Shader 固定输出已知位置，把 Fragment Shader固定输出洋红 `(1,0,1,1)`，逐步排除输入和插值问题。

### 8.23 常见错误

| 现象 | 原因 | 修正 |
| --- | --- | --- |
| Shader id 非零却黑屏 | 从未检查 Compile Status | 每次编译立即读状态和日志 |
| Link 失败 | Vertex out 与 Fragment in 不匹配 | 对照名称、类型和修饰符 |
| Triangle 位置随机 | VAO stride/offset/location 错 | 用表格逐字段对齐 |
| Triangle 完全不可见 | gl_Position 超出 Clip 或 w 错 | 先用固定 NDC 位置、w=1 |
| Uniform 不生效 | 未绑定 Program、位置为 -1 或设错 Program | 缓存每个 Program 自己的位置 |
| MVP 旋转/平移怪异 | 行列向量与内存布局混用 | 用单位和平移矩阵做上传测试 |
| 立方体面互相覆盖错误 | 没开/没清 Depth | 启用测试并每帧清深度 |
| 所有面被剔除 | CCW/CW 或 y 翻转约定不一致 | 关闭剔除验证，再修正正面约定 |
| 改 Shader 后仍是旧结果 | 文件路径错、未重新编译链接或链接失败仍继续 | 打印路径与新 Program 日志 |

### 8.24 随读自检

1. Draw 一个大三角形时，Vertex Shader 与 Fragment Shader 谁通常运行更多次？
2. `gl_Position` 属于 World Space 还是 Clip Space？
3. 为什么 VBO 和 Shader 之间还需要 VAO？
4. 编译成功是否保证 Program 能链接成功？
5. Uniform 与 Vertex Attribute 的更新范围有何不同？

参考答案：

1. Fragment Shader，因为一个大三角形覆盖许多采样位置。
2. Clip Space，类型是 vec4。
3. Buffer 只有字节，VAO 说明各 Location 怎样按 stride/offset 解释这些字节。
4. 不保证，阶段接口仍可能不匹配。
5. Attribute 每个 Vertex 可不同；Uniform 对同一 Draw 的所有 invocation 相同。

### 8.25 编程练习

#### 练习 A：固定纯色三角形

先让 Fragment Shader 总输出洋红色，不传顶点颜色。验收整条基础管线能稳定显示。

#### 练习 B：顶点颜色插值

加入 Location 1 颜色，三个角设为红绿蓝。把某个顶点颜色改为白色，先预测再观察渐变变化。

#### 练习 C：故意出错实验（破坏数据契约）

分别把 stride、color offset、Shader location 改错一次。记录画面和日志，再恢复。目标是学会从症状回到字节布局。

#### 练习 D：Uniform Tint

用键盘在红、绿、蓝三个 Tint 间切换。Uniform 应每次 Draw 前按当前状态设置，不重编译 Shader。

#### 练习 E：MVP

加入 `uMvp`，用单位矩阵验证画面不变，再依次只加入 Model 平移、View、Projection。每一步打印一个测试顶点的 CPU 预期 Clip Position。

#### 练习 F：Indexed Cube

上传立方体顶点和 36 个索引，开启深度与背面剔除。让 Model Matrix 随时间绕 y 旋转，并在窗口变化时更新 Projection Aspect。

#### 练习 G：Shader 错误日志

故意删除一个分号、制造 Vertex/Fragment 类型不匹配、写一个未使用 Uniform。分别说明 Compile、Link 和 Location 查询阶段会看到什么。

### 8.26 本章验收标准

第一次完成点：

- 彩色三角形可稳定显示、缩放窗口后比例与 Viewport 正确。
- 能逐行解释两个 Shader，尤其是 `in/out/uniform/location/gl_Position`。
- Shader 编译和 Program 链接失败会输出完整日志并停止使用无效对象。
- 能从 C++ Vertex 字节画出到 Shader Attribute 的一一映射。
- 能口头从 `glDrawArrays` 一直讲到 Framebuffer Pixel，并指出哪些阶段由自写 Shader、哪些由 GPU 固定规则完成。

第二次完成点：

- Uniform Tint 和 MVP 能在不重新上传 Mesh 的情况下改变画面。
- Indexed Cube 深度遮挡正确，旋转时没有随机缺面。
- 能口头从 `glDrawArrays/glDrawElements` 一直讲到 Framebuffer Pixel，并指出哪些阶段由自写 Shader、哪些由 GPU 固定规则完成。

完成本章后，你已经用 CPU 和 GPU 两种方式走过同一条“顶点到 Pixel”路径。后续加载 glTF、组织场景、计算光照时，增加的是输入规模与着色规则，核心流水线仍是这里建立的这一条。
