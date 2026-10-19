# 第 16 章：UI 与图形调试工具

<!-- nav -->
> [上一章：离屏 Framebuffer 与后处理](15-framebuffers-postprocess.md) ｜ [下一章：游戏循环、时间与渲染快照](17-game-loop-render-snapshot.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/07_postprocess_ui/16-ui-debug-tools.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

这一章有两个目标：第一，让文字、按钮和图标稳定地覆盖在 3D 画面上；第二，让渲染器内部原本看不见的数据变得可观察。

## 本章导读

### 本章学习目标

完成本章后，应能回答：

- UI 为什么通常最后绘制？
- 一段文字如何变成屏幕上的许多小四边形？
- Alpha、混合、Scissor 和 DPI 分别解决什么问题？
- Dear ImGui 为什么适合调试，却不一定适合最终游戏界面？
- 遇到黑屏、模型消失、贴图错误时，应该按什么顺序查？
- GPU 截帧中的 Event、Resource、Pipeline State 和 Pixel History 是什么？当前 macOS OpenGL 不支持 RenderDoc 时如何取得等价证据？

本章建立在第 13 章的 Pass、第 14 章的透明混合和第 15 章的离屏 Framebuffer 之上。

本章分三部分：先理解 UI 怎样进入最终画面，再接入调试面板与 GPU 工具，最后用固定流程定位一个真实错误。

本章同样有两个独立完成点。先做出一个能显示文字、开关和中间 Texture 的调试面板；再用它和终端日志完成一次“制造错误 -> 找到第一处错误数据 -> 修复”的排查。自写完整游戏 UI、OpenGL Debug Callback 和 GPU 截帧工具都不应阻塞这两个完成点。

**关于 Dear ImGui**：本章会用它的代码风格举例，因为它是业界调试面板的事实标准，而且这种“一行代码画一个控件”的写法最容易读懂。但**它不是本书的可运行依赖，第一遍不需要安装它**。[本章的参考实现](../exercises/07_postprocess_ui/16-ui-debug-tools.md)只用 Triangle、Alpha Blend 和 Scissor 自己写了一个最小 Overlay，这样你能看清 UI 最终仍然走同一条 GPU 管线——这正是 16.1～16.9 要讲的重点。等毕业工程需要更丰富的面板时再换成 Dear ImGui，Renderer 侧的诊断数据结构不用改。

所以本章第一个完成点的准确说法是：**面板里的数字和开关能正确反映渲染器状态**，而不是“装上了某个库”。

## 第一部分：UI 仍是一条渲染路径

### 16.1 先建立直觉：成品画面和仪表盘

玩家看到的是成品画面，而开发者还需要一块仪表盘。

汽车只显示道路而不显示速度、油量和故障灯，驾驶会很困难。渲染器也是一样：最终画面只告诉你“结果不对”，并不会自动告诉你错误发生在哪个 Pass、哪次 Draw 或哪张 Texture。

~~~text
给玩家看的画面：
3D 场景 -> 后处理 -> 血条、按钮、文字 -> 显示器

给开发者看的信息：
帧时间、Draw Call、相机参数、中间纹理、开关、错误日志
~~~

这两类界面需要区分：

- **游戏 UI（User Interface）**：玩家用来读取状态和操作游戏的界面，例如生命值、暂停菜单和结算画面。
- **调试 UI（Debug UI）**：开发阶段查看内部数据和修改参数的界面，例如 Framebuffer 缩略图、光源方向和性能曲线。

它们都可能在 3D 场景之后绘制，但用户、目的和工程要求不同。

---

### 16.2 UI 最终仍然只是 Triangle 和 Pixel

UI 看起来是“2D 的”，但 GPU 并没有一套神秘的按钮绘制器。最常见的 UI 元素最终仍会被拆成 Triangle。

一个矩形按钮通常由两个 Triangle 组成：

~~~text
(0)------------- (1)
 |             / |
 |           /   |
 |         /     |
 |       /       |
 |     /         |
 |   /           |
 | /             |
(2)------------- (3)

Triangle A: 0, 2, 1
Triangle B: 1, 2, 3
~~~

每个顶点通常至少包含：

~~~cpp
struct UiVertex {
    Vec2 position; // 在屏幕或 UI 画布中的二维位置
    Vec2 uv;       // 在字体图集或图标纹理中的采样位置
    Color color;   // 控件颜色，通常包含 alpha
};
~~~

UI Draw 的数据流是：

~~~text
按钮/文字的逻辑描述
-> 生成矩形的 Vertex 和 Index
-> Vertex Shader 把二维坐标变为 Clip Space
-> Fragment Shader 采样字体或图标 Texture
-> Alpha Blending 与已有场景颜色混合
-> 写入最终 Framebuffer
~~~

所以“先画 3D，再画 2D”只是 Pass 顺序。底层仍然是 Vertex、Triangle、Fragment 和 Framebuffer。

---

### 16.3 屏幕坐标、UI 坐标与正交投影

3D 模型通常使用透视投影：远处看起来更小。UI 不希望按钮因为“离相机远”而缩小，所以通常使用**正交投影（Orthographic Projection）**。

正交投影可以把 UI 坐标直接映射到屏幕：

~~~text
UI 坐标原点（常见设计）
(0, 0) --------------------> x
  |
  |
  |
  v y

右下角约为 (windowWidth, windowHeight)
~~~

OpenGL 的 NDC 却通常是：左下角 `(-1, -1)`，右上角 `(1, 1)`。因此 UI Vertex Shader 或正交投影矩阵需要完成两件事：

1. 把 `0..windowWidth` 映射到 `-1..1`。
2. 按 UI 约定决定是否翻转 y 轴。

若使用左上角为原点，可以用下面的直觉公式：

~~~text
ndcX =  2 * pixelX / width  - 1
ndcY =  1 - 2 * pixelY / height
~~~

例如窗口是 `800 x 600`，UI 点在 `(400, 300)`：

~~~text
ndcX = 2 * 400 / 800 - 1 = 0
ndcY = 1 - 2 * 300 / 600 = 0
~~~

它正好落在屏幕中心。

需要注意三个容易混淆的尺寸：

- **窗口逻辑尺寸**：操作系统和 UI 布局使用的尺寸。
- **Framebuffer 像素尺寸**：GPU 真正渲染的 Pixel 数。
- **Texture 尺寸**：某张离屏纹理自己的宽高。

**DPI（Dots Per Inch）**原本表示每英寸有多少显示点，常用来描述屏幕像素密度。UI 编程中更直接使用 **Content Scale / Device Pixel Ratio（内容缩放比例）**：一个窗口逻辑单位对应多少实际 Framebuffer Pixel。在 Retina 屏幕上，这个比例常大于 1。若只用窗口逻辑尺寸设置 `glViewport`，画面可能只占左下角或变得模糊。

---

### 16.4 UI 为什么通常最后绘制

UI 通常要覆盖在场景上，所以常见顺序是：

~~~text
Shadow Pass
    -> Opaque Pass
    -> 可选 Transparent Pass
    -> 可选 Post-process Pass
    -> Game UI Pass
    -> Debug UI Pass
    -> Present
~~~

UI Pass 常使用这些状态：

- 关闭深度写入：UI 不应把自己的深度留给后续 3D 对象。
- 常关闭深度测试：UI 的前后关系由绘制顺序决定，而不是 3D 深度。
- 开启 Alpha Blending：文字边缘和半透明面板需要与背景混合。
- 开启 Scissor Test：滚动区域中的内容不能画出矩形边界。
- 通常关闭 Back-face Culling，或者统一保证 UI Triangle 的绕序。

一个简化的 OpenGL 状态设置如下：

~~~cpp
glDisable(GL_DEPTH_TEST);                 // UI 不与 3D 深度比较
glDepthMask(GL_FALSE);                    // UI 不修改深度缓冲
glEnable(GL_BLEND);                       // 开启颜色混合
glBlendFunc(GL_SRC_ALPHA,
            GL_ONE_MINUS_SRC_ALPHA);      // 常见的非预乘 alpha 混合
glEnable(GL_SCISSOR_TEST);                // 允许裁剪滚动区域
~~~

这段代码不是“永远正确的 UI 配置”。如果图像使用**预乘 Alpha（Premultiplied Alpha）**，混合因子会不同。重要的是：UI Pass 必须显式声明自己依赖的状态。

---

### 16.5 Alpha 和混合到底在算什么

Alpha 常表示新颜色覆盖旧颜色的比例。最基础的混合公式是：

~~~text
output = source * sourceAlpha + destination * (1 - sourceAlpha)
~~~

其中：

- `source`：本次 UI Fragment 输出的颜色。
- `destination`：Framebuffer 中原来已有的场景颜色。
- `sourceAlpha`：UI Fragment 的透明度。
- `output`：混合后写回 Framebuffer 的颜色。

假设白色面板 `source = 1.0`，背景是黑色 `destination = 0.0`，Alpha 为 `0.25`：

~~~text
output = 1.0 * 0.25 + 0.0 * 0.75 = 0.25
~~~

结果是较暗的灰色。

常见错误：

- 未开启 blending：文字透明区域显示成黑色方块。
- 混合因子与贴图 Alpha 类型不匹配：边缘出现黑边或亮边。
- 在错误的颜色空间中混合：半透明层的亮度不自然。
- UI 先画、后处理再画：UI 也被模糊、灰度或 Tone Mapping。

---

### 16.6 文字是怎样画出来的

字体文件不是“每个字号一张完整图片”。字体通常保存字形轮廓和排版信息。程序需要把字形变成可由 GPU 采样的数据。

几个术语：

- **Character（字符）**：文字中的抽象符号，例如 `A` 或“你”。
- **Glyph（字形）**：某个字体中字符的具体外观。
- **Font Atlas（字体图集）**：把许多字形的小图片排在一张大 Texture 中。
- **Baseline（基线）**：多数文字放置时对齐的参考线。
- **Advance（步进）**：画完一个字形后，光标向右移动多少。
- **Kerning（字偶距）**：某些字符组合之间的额外间距修正，例如 `A` 和 `V`。

绘制字符串 `GPU` 的简化过程：

~~~text
读取 G 的字形尺寸和 UV -> 生成一个矩形
光标按 G.advance 向右移动
读取 P 的字形尺寸和 UV -> 生成一个矩形
光标按 P.advance 向右移动
读取 U 的字形尺寸和 UV -> 生成一个矩形
一次或少量 Draw 绘制这些矩形
~~~

字体图集中的字形一般只保存覆盖率。Fragment Shader 用覆盖率控制 Alpha，再乘以文字颜色。

~~~glsl
float coverage = texture(fontAtlas, uv).r;
outColor = vec4(textColor.rgb, textColor.a * coverage);
~~~

若字体边缘锯齿明显，先检查 Texture Filter、分辨率和缩放比例，不要立刻加入复杂字体技术。

---

### 16.7 Scissor：只允许在一个矩形内写 Pixel

**Scissor Test（剪裁测试）**会丢弃指定矩形之外的 Fragment。它常用于：

- Scroll View 的内容区域。
- 文本输入框。
- 窗口或面板相互遮挡的区域。

~~~text
+---------------------------+
| Scroll View               |
| +-----------------------+ |
| | 可见内容              | |
| | 可见内容              | |
| +-----------------------+ |
|   下面还有内容，但被裁掉   |
+---------------------------+
~~~

OpenGL 的 Scissor 原点通常在左下角，而许多 UI 系统的原点在左上角。坐标不转换会导致“裁剪框上下颠倒”。

---

### 16.8 Immediate Mode 和 Retained Mode

UI 系统常见两种思路。

**Immediate Mode（即时模式）**每帧用代码描述当前界面：

~~~cpp
ImGui::Begin("Renderer");
ImGui::Text("Draw calls: %d", stats.drawCalls);
ImGui::Checkbox("Shadows", &settings.enableShadows);
ImGui::End();
~~~

代码每帧都会执行，但 ImGui 会根据输入和内部 ID 保留必要的交互状态。优点是添加调试控件很快，界面与调试变量靠得很近。

**Retained Mode（保留模式）**先创建一棵 UI 对象树，之后修改节点属性：

~~~text
Canvas
└── PausePanel
    ├── TitleText
    ├── ResumeButton
    └── QuitButton
~~~

它更适合复杂布局、动画、皮肤、本地化和设计师工作流。

Dear ImGui 适合开发者工具；最终游戏 UI 可以继续自写，也可采用独立 UI 库。不要因为它们都画矩形，就把两个系统的职责强行合并。

---

### 16.9 Dear ImGui 一帧的接入位置

Dear ImGui 不替你创建游戏循环。每一帧仍由应用负责开始、构建控件、生成绘制数据并提交。

~~~cpp
while (!windowShouldClose()) {
    pollWindowEvents();

    ImGui_ImplOpenGL3_NewFrame(); // 准备 OpenGL 后端本帧数据
    ImGui_ImplGlfw_NewFrame();    // 读取窗口、鼠标和键盘状态
    ImGui::NewFrame();            // 开始描述这一帧的 UI

    ImGui::Begin("Renderer");
    ImGui::Text("Frame: %.2f ms", frameMs);
    ImGui::Checkbox("Depth test", &enableDepthTest);
    ImGui::End();

    renderer.render(snapshot);    // 先绘制场景和后处理

    ImGui::Render();              // 完成 UI Vertex/Index 数据生成
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    swapBuffers();                // 展示包含 UI 的最终画面
}
~~~

代码意图逐段解释：

1. `NewFrame` 系列函数告诉 ImGui 新的一帧开始了，并提供输入。
2. `Begin/Text/Checkbox/End` 描述这一帧有哪些控件。
3. Renderer 先完成场景。
4. `ImGui::Render` 把控件转换为 Draw Data。
5. OpenGL 后端真正提交 UI Draw。
6. `swapBuffers` 将最终 Back Buffer 展示到窗口。

输入还有一个关键边界：当 `ImGui::GetIO().WantCaptureMouse` 为真时，鼠标正在操作调试 UI，此时不要同时旋转游戏相机。键盘也有对应的 `WantCaptureKeyboard`。

---

#### 本部分检查点

**能做出来**：一段文字和一个开关稳定显示在 3D 画面上方，窗口缩放后位置和大小仍然正确。

**能回答**：

- UI 为什么通常最后画？
- 一段文字是怎么变成一批小四边形的？
- Scissor 和 Viewport 有什么区别？

**卡在这里就先不要往下读**：UI 被 3D 物体挡住或者位置漂移，通常是深度状态或投影用错了（UI 用正交，不是透视）。

## 第二部分：让 Renderer 内部状态可观察

### 16.10 调试面板最先显示什么

第一版只需要显示能回答具体问题的数据。

| 数据 | 它回答的问题 |
| --- | --- |
| CPU frame time | CPU 准备一帧用了多久？ |
| GPU frame time | GPU 执行这一帧用了多久？ |
| Draw Call 数 | 本帧提交了多少次绘制？ |
| Triangle 数 | 提交了多少几何图元？ |
| 可见/被裁剪对象数 | 视锥裁剪是否工作？ |
| 窗口和 Framebuffer 尺寸 | Retina 或 resize 是否处理正确？ |
| 当前相机位置、方向、FOV | 能否重现当前视角？ |
| Pass 开关和参数 | 能否隔离某个效果？ |
| 中间 Render Target 缩略图 | 哪个 Pass 首先产生错误？ |

不要只显示 FPS。FPS 只说明整体快慢，无法解释根因。第 19 章会系统讲性能测量。

---

### 16.11 把中间 Texture 显示出来

第 12 章的 Shadow Map、第 15 章的 Scene Color 和 Bloom Texture 都是排错证据。调试 UI 可以把它们缩小显示。

~~~cpp
ImGui::Text("Scene color");
ImGui::Image(
    reinterpret_cast<ImTextureID>(static_cast<intptr_t>(sceneColorTexture)),
    ImVec2(320.0f, 180.0f),
    ImVec2(0.0f, 1.0f), // OpenGL Texture 与 ImGui 的 y 方向可能不同
    ImVec2(1.0f, 0.0f)
);
~~~

这里的重点不是强记类型转换，而是理解：ImGui 的 OpenGL 后端最终需要知道要绑定哪一个 Texture Handle。Dear ImGui 不同版本对 Texture ID 的包装接口可能不同，接入时应以当前版本的 OpenGL 后端示例为准。

深度纹理不能总按普通颜色直接显示。它的数值可能集中在很窄范围，需要线性化或重新映射到 `0..1` 后才能看清。

---

### 16.12 OpenGL Debug Output：让驱动主动报告问题

OpenGL 的很多调用没有返回值。若当前驱动和 Context 支持 `KHR_debug` 或相应调试扩展，开启 Debug Context 和 Debug Callback 后，驱动可以报告非法枚举、无效操作、性能警告等。

~~~cpp
void APIENTRY onGlMessage(GLenum source,
                          GLenum type,
                          GLuint id,
                          GLenum severity,
                          GLsizei length,
                          const GLchar* message,
                          const void* userData) {
    std::cerr << "OpenGL: " << message << '\n';
}

// 仅在函数加载器确认 KHR_debug/等价扩展和函数地址都可用后执行：
// glEnable(GL_DEBUG_OUTPUT);
// glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
// glDebugMessageCallback(onGlMessage, nullptr);
~~~

具体能力变量和函数指针名称取决于你生成的 GLAD 版本，因此这里不伪造一个通用 `if`。若 macOS 当前 OpenGL 4.1 Context 没有暴露所需调试扩展，就保留 Shader 编译/链接日志，并在开发阶段于关键边界使用 `glGetError`。不要在未检查能力时直接取消上面三行的注释。

这些消息是线索，不是自动修复。支持对象标签时，还应给 GPU 对象加可读名称，例如 SceneColor、MainOpaqueProgram，这样在支持的 GPU 捕获工具或自己的调试 UI 中不会只看到数字 Handle。

开发构建中可以保留错误检查；发布构建则避免每次 Draw 都调用会强制同步的检查方式。

---

### 16.13 GPU 截帧工具是“一帧画面的时间冻结器”

RenderDoc 等 GPU 截帧工具会捕获一帧 GPU 命令和资源，让你在程序结束后逐项查看。但必须先说明平台限制：

- RenderDoc 官方工具不原生支持当前课程使用的 macOS OpenGL 4.1 捕获。
- 下面的 Event、Pipeline State、Pixel History 方法仍是重要的通用调试思路，可在 Windows/Linux OpenGL、后续 Vulkan 环境或其他支持的捕获工具中使用。
- 当前 Mac 阶段不能把 RenderDoc 当作必需条件，应使用 Shader 编译/链接日志、能力检查、glGetError、中间 Texture 预览、Pass 开关、统计计数和颜色替换实验完成同样的数据链排查。
- Xcode 的 GPU 捕获主要面向 Metal，具体 OpenGL 支持随 Xcode/macOS 版本变化，不能在课程中假设一定可用。

基础术语：

- **Capture（截帧）**：保存某一帧的 GPU 工作。
- **Event（事件）**：截帧中的一条命令或标记，例如 Clear、Draw、Blit。
- **Event List / Event Browser**：按执行顺序列出整帧命令。
- **Resource（资源）**：Texture、Buffer、Shader 等 GPU 数据。
- **Pipeline State（管线状态）**：某个 Draw 当时使用的 Shader、输入布局、Texture、深度和混合设置。
- **Mesh Viewer**：查看某次 Draw 的输入顶点和变换后顶点。
- **Texture Viewer**：查看 Render Target、Texture、Mipmap 和通道。
- **Pixel History**：列出某个 Pixel 经历过哪些写入、深度失败或混合。

RenderDoc 不会告诉你“第 86 行错了”。它提供证据，让你验证假设。

---

#### 本部分检查点

**能做出来**：面板里能实时看到 Draw Call 数、可见对象数、各 Pass 耗时，并且能把 Shadow Map 缩略显示出来。

**能回答**：

- 面板里的数字是从哪一层采集的？
- 为什么中间 Texture 要能显示出来？
- macOS 上没有 RenderDoc 时，用什么取代等价证据？

**卡在这里就先不要往下读**：数字对不上（比如裁剪开了但 Draw Call 没降）先修数据采集，别修渲染。

## 第三部分：按证据定位画面错误

### 16.14 GPU 截帧的固定排查路线

在支持 GPU 截帧的平台上，第一次打开工具很容易在数百条 Event 中迷路。使用固定路线：

1. 找到最终显示的 Back Buffer。
2. 找到最后一次向它写入的 Pass。
3. 检查这个 Pass 的输入 Texture 是否正确。
4. 若输入已经错误，沿资源来源退回上一个 Pass。
5. 找到第一个从“正确”变成“错误”的 Event。
6. 再检查该 Event 的 Pipeline State、Mesh 和 Shader 输入。

~~~text
反向排查方向（从结果倒推原因）

最终错误 Pixel
    ^
    |
最后一次写入它的 Event
    ^
    |
该 Event 的 Render Target 与输入 Texture
    ^
    |
输入首次出错的上一个 Pass
    ^
    |
Vertex / Shader / State / Resource
~~~

这叫从结果反向追踪数据，不是盯着所有 Shader 逐个猜。

---

### 16.15 完整例子：一个模型为什么是黑色

现象：背景正常，模型轮廓存在，但整个模型是黑色。

先提出可能原因：

- Base Color Texture 没绑定。
- Normal 错误导致光照为 0。
- 光源方向或强度错误。
- Shader 输出错误。
- 最后颜色被后处理压成黑色。

然后按证据排查：

1. 在支持的 GPU 截帧工具中找到绘制该模型的 Draw；当前 macOS OpenGL 环境则通过 Pass/Draw 日志定位。
2. 在 Mesh Viewer 看变换后顶点。若位置正常，几何阶段大致正确。
3. 在 Pipeline State 检查 Fragment Shader 和 Texture 绑定。
4. 打开 Base Color Texture。若图像正确，继续检查 UV。
5. 临时让 Fragment Shader 输出 `vec4(1,0,1,1)`。若模型变成紫色，说明 Draw、深度和 Render Target 基本正常。
6. 再分别输出法线可视化、Base Color、光照项，定位哪一项为 0。

这里使用了**二分排查**思想：把长链条分段，找到第一段错误，而不是一次修改五个参数。

---

### 16.16 症状到检查项

| 症状 | 优先检查 |
| --- | --- |
| 完全黑屏 | 当前 FBO、Viewport、Clear、相机、是否有 Draw、Shader 是否链接 |
| 只有清屏颜色 | Draw 是否执行、VAO/EBO、顶点是否被裁剪、面剔除 |
| 模型忽隐忽现 | 深度缓冲、Near/Far、Bounds、裁剪、资源生命周期 |
| 贴图全白或全黑 | Texture 是否上传、绑定单元、Sampler Uniform、UV |
| 贴图上下颠倒 | 图片原点与 UV 约定 |
| 透明边缘发黑 | Alpha 类型和 Blend Func |
| UI 被场景挡住 | Depth Test、UI Pass 顺序 |
| UI 裁剪位置错误 | Scissor 原点、Framebuffer Scale |
| Resize 后拉伸 | Viewport 和离屏附件是否重建 |
| 只在第二帧出错 | 状态泄漏、资源被过早释放、双缓冲数据覆盖 |

表格是起点，不是结论。最终仍要用截帧、日志或可视化输出验证。

---

### 16.17 OpenGL 状态泄漏

OpenGL 是状态机。`glEnable(GL_BLEND)` 会一直生效，直到显式关闭。函数结束不会自动恢复状态。

例如 UI Pass 开启 blending、关闭 depth test。如果下一帧 Shadow Pass 假设 depth test 已开启，却没有自己设置，就会依赖上一帧留下的偶然状态。

正确原则是：

> 每个 Pass 在开始时设置自己需要的状态，不依赖调用前“应该是什么”。

可以把 Pass 状态写成明确结构：

~~~cpp
struct RenderState {
    bool depthTest;
    bool depthWrite;
    bool blend;
    bool cullBackFaces;
};

void beginUiPass() {
    applyRenderState({false, false, true, false});
}
~~~

即使内部仍调用 OpenGL，这种写法也让第 20 章迁移到显式 API 更自然。

---

### 16.18 截图与可重复性

“刚才好像是对的”不是可靠证据。每完成一个功能，至少保存：

- 一张最终画面截图。
- 相机位置、方向和 FOV。
- 窗口或 Framebuffer 尺寸。
- 关键开关与参数。
- Git 提交或代码版本标识。
- 日期和运行平台。

同一个场景、同一个相机和同一组参数才能做有意义的前后比较。

更进一步可以做**图像回归测试**：程序自动渲染固定场景并保存截图，与基准图比较。浮点误差和不同 GPU 会产生微小差异，因此通常允许少量误差，而不是要求每个 Pixel 完全相同。

---

### 16.19 本章常见误解

#### 误解一：UI 不是 3D，所以不走 GPU 管线

UI 通常仍由 GPU 绘制 Triangle，只是坐标、深度和 Pass 规则更简单。

#### 误解二：FPS 高就说明 Renderer 没问题

错误画面也可以跑得很快。正确性、稳定性和性能是三个不同维度。

#### 误解三：RenderDoc 会直接指出 Bug

它显示事实。你仍需提出能被验证的问题。

#### 误解四：调用 ImGui 后状态会自动回到原样

后端可能保存和恢复一部分状态，但你的 Renderer 不应依赖库的偶然实现。每个 Pass 都要建立自己的状态。

#### 误解五：调试 UI 越多越好

无组织的数字同样会制造噪声。每个指标都应回答一个问题。

---

### 16.20 建议实现顺序

1. 确保每个 Shader 的编译/链接日志都完整输出，并在初始化与 Pass 边界提供可开关的 `glGetError` 检查。
2. 为每个 Pass 输出名称、FBO、Viewport、Draw 数和关键 Texture Handle。
3. 接入 Dear ImGui，只显示一个固定文本窗口。
4. 显示窗口尺寸、Framebuffer 尺寸和 CPU frame time。
5. 加入 Depth、Culling、Shadow、Post-process 开关。
6. 显示 Shadow Map，以及当前项目实际实现的 Scene Color/Bloom 等中间纹理。
7. 加入相机与光照参数控件。
8. 故意绑定一张错误 Texture，使用日志、中间纹理和纯色替换 Shader 沿数据链定位。
9. 若函数加载器确认支持，再接入 OpenGL Debug Output 和对象/事件标签。
10. 若当前平台与图形 API 有可用 GPU 截帧工具，再捕获同一错误并练习反向追踪。

每完成一步都保留可运行版本，不要一次加入所有工具。

---

### 16.21 练习

#### 练习 A：UI 坐标换算

窗口 Framebuffer 为 `1280 x 720`。手算以下 Pixel 坐标对应的 NDC：

- `(0, 0)`
- `(640, 360)`
- `(1280, 720)`

写出你的 y 轴约定。

#### 练习 B：故意出错实验（状态故障）

在调试面板中分别关闭：

- Depth Test
- Back-face Culling
- Shadow Pass
- Tone Mapping
- UI Blending

操作前先写下你预计看到的变化，操作后截图比较。

#### 练习 C：中间纹理观察器

显示 Shadow Map，以及当前项目实际拥有的 Scene Color、Scene Depth 或 Bloom Texture。每张已实现的图都标出资源名称与尺寸，不为未实现效果创建假资源。

#### 练习 D：错误资源反向追踪

故意把模型的 Base Color Texture 绑定成空 Texture。

- 在支持 GPU 截帧的平台上：捕获一帧，从错误 Pixel 追踪到对应 Draw 和错误资源绑定。
- 在当前 macOS OpenGL 环境：给 Draw 输出对象名、Program、Texture Handle 和目标 FBO；在 ImGui 中预览绑定 Texture；用纯紫色 Shader 验证 Draw 与深度链，再定位错误绑定。

#### 练习 E：文本

显示一行动态文字：相机位置和 FPS。调整窗口大小，确认文字不被拉伸，Retina 显示下位置正确。

---

### 16.22 本章验收标准
进入第 17 章前，最低应满足：

- UI 在后处理之后绘制，不被场景深度错误遮挡。
- 文字边缘透明正常，没有黑色方块。
- 调试 UI 能显示关键帧数据、Shadow Depth，以及当前项目实际拥有的其他中间 Texture。没有实现 Bloom 时不要求凑满三张。
- UI 获取鼠标时，游戏相机不会同时响应。
- 每个 Pass 显式建立所需的深度、混合、剔除和 Viewport 状态。
- Shader 编译/链接失败会输出完整日志；关键 Pass 能输出 FBO、Viewport、Draw 数和资源 Handle。
- 在当前 macOS OpenGL 环境中，能用日志、中间纹理、Pass 开关和替换 Shader 找到一个错误绑定。
- 能从最终错误结果反向定位到第一个错误 Pass，而不是依赖随机修改参数。

平台支持时再完成扩展验收：OpenGL Debug Output 能显示带上下文的消息；GPU 截帧工具中能找到一个模型 Draw，并解释其 Vertex、Texture、Shader 与 Render Target。macOS OpenGL 4.1 不具备这些能力时，不影响本章完成。

你还应能用自己的话回答：

1. 为什么 UI 最终仍然是 Triangle？
2. Alpha Blending 的 source 和 destination 分别是什么？
3. 字符、字形和字体图集有什么区别？
4. Event List 与 Pipeline State 分别告诉你什么？
5. 为什么“每个 Pass 自己设置状态”比“记得恢复状态”更可靠？

完成这些以后，你拥有的不只是一个可显示 UI 的 Renderer，而是一套能够观察和解释画面的工具。
