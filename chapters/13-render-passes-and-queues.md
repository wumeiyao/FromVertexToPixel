# 第 13 章：Render Pass 与渲染队列

<!-- nav -->
> [上一章：方向光阴影贴图](12-shadow-mapping.md) ｜ [下一章：透明、天空与实例化](14-transparency-sky-instancing.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/06_frame_renderer/13-render-passes-and-queues.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

到目前为止，一帧已经不只做一次绘制：阴影要先写 Depth Texture，主场景要写颜色与深度，之后还会有天空、透明、后处理和 UI。若把所有代码堆进一个巨大的 `render()`，任何状态遗漏都会影响后续步骤。

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 说明为什么一帧不能写成一个巨大的 `render()` 函数。
2. 用“输入、输出、GPU 状态”三项写出一个 Pass 的合同。
3. 区分 Draw Call、Render Item、Render Queue、Pass 和 Frame 五个层级。
4. 解释 Clear、Load、Store 分别对 Attachment 做了什么。
5. 说明 OpenGL 是持续状态机，以及为什么要在 Pass 入口显式设置状态。
6. 给 Opaque 队列设计 Sort Key，并解释 Transparent 为什么不能照搬它。
7. 写出一个能逐个 Pass 开关、逐步排查的帧入口。

本章不追求复杂 Render Graph，而是建立最小、明确的帧组织：每个 Pass 写清输入、输出和 GPU 状态；每个 Queue 写清对象分类与排序规则。

本章分三部分：先定义每个 Pass 的合同和资源依赖，再分类、排序 Draw，最后形成一个可逐步检查的帧入口。

本章会先画出完整一帧的结构，但不要求你立即实现所有方框。现在已经学过的 Shadow 和 Opaque 是本章的实际改造对象；Sky、Transparent、Post Process 和 UI 先理解它们为什么需要独立位置，随后三章再逐项实现。这样做不是跳过练习，而是先把现有代码整理成能继续扩展的形状。

## 第一部分：Pass 合同与资源依赖

### 13.1 为什么一帧不能只看成“把所有物体画一遍”

不同绘制任务需要不同观察者、不同输出和不同规则。

~~~text
Shadow：从光源看，只写 Depth Texture
Opaque：从 Camera 看，写 Scene Color 和 Scene Depth
Sky：只填充没有不透明物体的最远背景
Transparent：读取已有 Scene Color，与它混合
Post Process：读取整张 Scene Color，输出最终显示颜色
UI：在最终场景上方画文字、按钮和 HUD
~~~

若这些工作混在同一个循环中，就很难回答：

- 当前写到哪个 Framebuffer？
- Depth Test 开着吗？
- Depth Write 开着吗？
- Blend 还开着吗？
- Viewport 是 Shadow Map 尺寸还是窗口尺寸？
- 当前 Shader 读取的 Texture 是谁先产生的？

Render Pass 的价值就是把这些问题变成显式合同。

---

### 13.2 先定义五个核心名词

#### 13.2.1 Draw Call

Draw Call 是 CPU 发给 GPU 的一次绘制命令，例如 `glDrawElements`。它通常绘制一个 Mesh Primitive 或一批实例。

#### 13.2.2 Render Queue

Render Queue 是“等待按照某种规则绘制的一组 RenderItem”。它只是 CPU 容器，不一定是线程队列。

~~~text
Opaque Queue
Transparent Queue
UI Queue
~~~

Queue 负责对象属于哪类、按什么顺序排列。

#### 13.2.3 Render Target

Render Target 是当前绘制结果写入的位置，例如：

- 默认窗口 Framebuffer。
- Shadow Depth Texture。
- Scene HDR Color Texture。
- Bloom 中间 Texture。

#### 13.2.4 Render Pass

Render Pass 是一段有明确输入、输出、状态和 Draw 列表的渲染工作。

可以把它理解为一份合同：

~~~text
这个 Pass 读取什么？
写入什么？
开始时清除还是保留旧内容？
使用哪个 Viewport？
深度、混合、剔除怎样设置？
绘制哪些 Queue？
~~~

一个 Pass 可以包含许多 Draw Call；一个 Queue 通常会被某个 Pass 消费。

#### 13.2.5 FBO

FBO 是 Framebuffer Object。它把 Color Texture、Depth Texture 等 Attachment 组合起来，成为一个可以被绘制的目标。Pass 是一段逻辑工作，FBO 是它可能绑定的具体 OpenGL 对象；第 13.15 节会再次对比。

---

### 13.3 Resource、Attachment、Input 与 Output

Pass 之间通过 GPU 资源连接。

#### 13.3.1 Resource

本章中的 Resource 主要是 Texture 或 Buffer，例如 `shadowDepth`、`sceneColor`、`sceneDepth`。

#### 13.3.2 Attachment

Attachment 是绑定到 Framebuffer、被本 Pass 写入的 Texture 或 Renderbuffer。

~~~text
Color Attachment -> Shader 输出颜色写入
Depth Attachment -> 深度测试和深度写入使用
~~~

#### 13.3.3 Input

Input 是 Pass 读取的已有资源。例如 Main Opaque Pass 采样前一 Pass 生成的 `shadowDepth`。

#### 13.3.4 Output

Output 是 Pass 产生或更新的资源。例如 Opaque Pass 输出 `sceneColor` 和 `sceneDepth`。

大多数 Pass 连接并不发生 CPU 拷贝：前一 Pass 写好的 GPU Texture，后一个 Shader 直接采样同一资源。

---

### 13.4 完整示例：用数据依赖决定 Pass 顺序

本书使用下面的基础帧：

~~~text
Shadow Pass        输出 shadowDepth
    |
    v
Opaque Pass        输出 sceneColor + sceneDepth
    |
    v
Sky Pass           补 sceneColor 背景
    |
    v
Transparent Pass   混合进 sceneColor
    |
    v
Post Process       输出 displayColor
    |
    v
UI Pass            叠加界面
    |
    v
Present            显示
~~~

顺序不是个人偏好，而是数据依赖：

- Opaque 要读取 `shadowDepth`，所以 Shadow 必须先完成。
- Transparent 要与已有场景颜色混合，所以 Opaque 与 Sky 要先完成。
- Post Process 要读取完整场景，所以 Transparent 要先完成。
- UI 通常不希望被 Bloom 或 Tone Mapping 改变，所以放在场景后处理之后。
- Present 只能显示最终完成的默认 Framebuffer。

这里的 Present 指把画好的 Window Backbuffer 交给窗口系统显示。用 GLFW 时，它通常就是 `glfwSwapBuffers`。

若需求不同，顺序可以改变。例如想让 UI 也产生 Bloom，就必须在 Bloom 前画 UI。但改变时应明确新的输入输出关系。

---

### 13.5 每个基础 Pass 的合同

#### 13.5.1 Shadow Pass

~~~text
观察者：Directional Light
读取：Caster Mesh、Model Matrix
写入：shadowDepth
清除：Depth 清为 1
深度测试：开
深度写入：开
颜色写入：无
混合：关
Viewport：Shadow Map 尺寸
~~~

#### 13.5.2 Opaque Pass

~~~text
观察者：Main Camera
读取：Mesh、Material、shadowDepth、Light 数据
写入：sceneColor、sceneDepth
清除：Color 与 Depth
深度测试：开
深度写入：开
混合：关
Viewport：Scene Target 尺寸
~~~

#### 13.5.3 Sky Pass

~~~text
观察者：Main Camera 旋转，不使用 Camera 平移
读取：Sky Texture 或程序化天空参数、sceneDepth
写入：sceneColor
清除：不清
深度测试：只在最远背景处通过
深度写入：关
混合：关
~~~

#### 13.5.4 Transparent Pass

~~~text
观察者：Main Camera
读取：Transparent Queue、已有 sceneColor、sceneDepth
写入：sceneColor
清除：不清
深度测试：开
深度写入：通常关
混合：开
顺序：通常从远到近
~~~

#### 13.5.5 Post Process Pass

~~~text
观察者：无 3D Camera；绘制 Fullscreen Triangle
读取：sceneColor，以及可选 Bloom Texture
写入：默认 Framebuffer 或 displayColor
清除：按目标需要
深度测试：关
深度写入：关
混合：关
Viewport：输出目标尺寸
~~~

#### 13.5.6 UI Pass

~~~text
观察者：UI 正交投影或屏幕坐标
读取：字体、图标、UI Queue
写入：最终颜色
深度测试：通常关，或使用独立 UI 规则
深度写入：关
混合：开
顺序：按 UI 层级
~~~

---

### 13.6 Clear、Load 与 Store 的直觉

现代显式 API 常用 Load/Store 术语。OpenGL 里通常通过 `glClear` 和后续是否继续使用附件表达同样意图。

#### Clear

本 Pass 不需要旧内容，开始时清成已知值。

~~~text
Opaque Pass 的 sceneDepth -> 清为最远深度
Opaque Pass 的 sceneColor -> 清为背景色或黑色
~~~

#### Load

保留前面 Pass 已有内容继续使用。

~~~text
Transparent Pass 必须保留 Opaque 已写入的 sceneColor 与 sceneDepth
~~~

#### Store

Pass 结束后，输出还要供后续使用。

~~~text
Shadow Pass 的 depth 要给 Main Pass 采样
Opaque/Transparent 的 sceneColor 要给 Post Process 采样
~~~

OpenGL 不要求你写“Store”命令，但设计 Pass 时必须知道资源是否还活着、是否会被后续读取。

错误地在 Transparent Pass 开头 `glClear(GL_COLOR_BUFFER_BIT)`，会把不透明场景清掉，只留下透明物体。

---

#### 本部分检查点

**能做出来**：为现有的 Shadow Pass 和 Opaque Pass 各写出一份合同：输入、输出、需要设置的 GPU 状态。

**能回答**：

- Draw Call、Render Item、Queue、Pass、Frame 五个层级各是什么？
- Clear、Load、Store 分别对 Attachment 做什么？
- 为什么 Render Pass 不等于 OpenGL FBO？

**卡在这里就先不要往下读**：合同写不出来，说明当前 `render()` 里还有隐式状态。先把它们找出来。

## 第二部分：让状态与队列变成明确数据

### 13.7 OpenGL 是持续状态机

OpenGL 状态会一直保留，直到显式修改。

~~~cpp
glEnable(GL_BLEND);
// Blend 不会在函数返回时自动关闭。
~~~

这意味着：Transparent Pass 打开的 Blend，如果下一帧 Opaque Pass 忘记关闭，不透明物体可能错误混合。Shadow Pass 把 Viewport 改为 2048 x 2048，若 Main Pass 不恢复，窗口只会在错误区域绘制。

常见持续状态包括：

| 状态 | 作用 |
| --- | --- |
| Framebuffer Binding | 当前写到哪个目标 |
| Viewport | NDC 映射到目标哪个矩形 |
| Scissor | 限制允许写入的矩形 |
| Program | 当前使用哪个 Shader Program |
| VAO | 当前顶点布局与 Buffer |
| Depth Test | 是否进行深度比较 |
| Depth Function | 哪种深度关系通过 |
| Depth Mask | 是否写入 Depth Buffer |
| Blend | 是否与已有颜色混合 |
| Cull Face | 是否跳过背面或正面 Triangle |
| Color Mask | 哪些颜色通道允许写入 |

Pass 开头必须设置自己依赖的状态，不应假定上一 Pass 恰好留下正确值。

---

### 13.8 用结构描述 Pass 状态

教学项目可以先定义小型状态结构：

~~~cpp
enum class CompareOp {
    Less,
    LessEqual,
    Always
};

struct DepthState {
    bool testEnabled = true;
    bool writeEnabled = true;
    CompareOp compare = CompareOp::Less;
};

struct BlendState {
    bool enabled = false;
    BlendFactor source = BlendFactor::One;
    BlendFactor destination = BlendFactor::Zero;
};

struct PassState {
    FramebufferHandle target;
    Rect viewport;
    DepthState depth;
    BlendState blend;
    CullMode cull = CullMode::Back;
};
~~~

`FramebufferHandle`、`Rect`、`BlendFactor` 和 `CullMode` 都是本项目自己定义的类型，用来把职责写清楚。它们不是 C++ 或 OpenGL 自带的名字，不要去查文档。第一版可以先用已有的 `GLuint`、宽高和枚举实现相同信息，再逐步整理成这些结构。

应用状态：

~~~cpp
void applyPassState(const PassState& state) {
    bindFramebuffer(state.target);
    glViewport(
        state.viewport.x,
        state.viewport.y,
        state.viewport.width,
        state.viewport.height
    );

    setDepthTest(state.depth.testEnabled);
    setDepthWrite(state.depth.writeEnabled);
    setDepthCompare(state.depth.compare);

    setBlend(state.blend);
    setCullMode(state.cull);
}
~~~

第一版可以每次都调用对应 OpenGL API。后续 Profile 证明状态调用有成本时，再在 `setDepthTest` 等函数里缓存“当前状态”，只对变化项调用 API。

注意：状态缓存是一项优化，它不能改变“每个 Pass 描述完整期望状态”的设计。

---

### 13.9 Render Queue 怎样分类对象

Renderer 收集可见 RenderItem 后，根据 Material 的 Alpha Mode 和用途分类。

~~~cpp
enum class RenderQueueType {
    Opaque,
    AlphaCutout,
    Transparent,
    Overlay
};
~~~

#### Opaque

表面完全不透明，例如墙、地面、普通角色。

#### Alpha Cutout

根据 Alpha 进行“完全保留或完全丢弃”，例如铁丝网、树叶卡片。它没有半透明混合，通常仍写 Depth，但 Shader 中会 `discard` 透明部分。

#### Transparent

需要与背景混合，例如玻璃、烟雾、半透明粒子。通常不写 Depth，且要排序。

#### Overlay

始终按特殊层级覆盖，例如 UI 或调试文字。

分类应该在提交前完成，不要在每个 Draw 中用大量 `if` 临时猜材质类型。

---

### 13.10 Opaque 为什么可以重排

Opaque Pass 开启 Depth Test 与 Depth Write。最终每个 Pixel 保存最近通过测试的表面，因此大多数不透明物体的提交顺序不会改变最终颜色。

这允许 Renderer 为性能重排。

#### 13.10.1 按状态排序

可按以下键排序：

~~~text
Shader Program
Material
Texture Set
Mesh/VAO
~~~

Texture Set 指某个 Draw 需要同时绑定的一组 Texture，例如 Base Color、Normal 和 Shadow Map。

连续对象共享状态时，可以减少 Program、Texture 和 VAO 切换。

#### 13.10.2 Front-to-Back

从近到远绘制可让近处表面先写 Depth。Early Depth Test 是指 GPU 在运行完整片段 Shader 之前就先做一次深度检查。后面到来的远处 Fragment 可能在这一步就被丢弃，从而省下整段片段 Shader 的工作。

#### 13.10.3 两种目标的交换

完全按材质排序可能增加 Overdraw，也就是同一个 Pixel 被多个后来又遮住的 Fragment 反复着色；完全按深度排序可能增加状态切换。第一版可以：

~~~text
先按 Program/Material 分组
组内按 Camera Depth 从近到远
~~~

是否值得更复杂，必须由第 19 章的 Profile 数据决定。

---

### 13.11 Sort Key：把排序规则变成明确数据

可为每个 Draw Item 构造 Sort Key：

~~~cpp
struct DrawItem {
    const RenderItem* item;
    std::uint64_t sortKey;
    float cameraDepth;
};
~~~

一个示意 64 位 Opaque Key：

~~~text
[ queue: 4 bits ][ program: 16 ][ material: 20 ][ mesh: 24 ]
~~~

真正位数取决于资源数量。重点是排序依据要可观察、可打印，而不是埋在 Draw Loop 的条件判断里。

初学阶段也可以直接使用结构化比较器：

~~~cpp
std::sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) {
    if (a.program != b.program) return a.program < b.program;
    if (a.material != b.material) return a.material < b.material;
    return a.cameraDepth < b.cameraDepth;
});
~~~

先正确表达规则，再考虑压成整数。

---

### 13.12 Transparent 为什么不能照搬 Opaque 排序

透明混合要读取 Framebuffer 中已有颜色，因此“先画谁”会改变后一个对象的 Destination Color。

常见规则是：

~~~text
Opaque：可重排，常近到远或按状态
Transparent：通常远到近
~~~

Transparent 仍保持 Depth Test，以免显示在不透明墙前；但通常关闭 Depth Write，让后面的透明表面仍有机会参与混合。

透明排序并不能完美处理互相穿插或单个 Mesh 内部前后交叉的 Triangle。第 14 章会详细分析限制。

---

#### 本部分检查点

**能做出来**：Opaque 队列有明确的 Sort Key，能打印出排序后的绘制顺序。

**能回答**：

- Opaque 为什么可以重排，Transparent 为什么不行？
- 按材质排序和按深度排序各改善什么、各恶化什么？
- Pass 级数据和 Draw 级数据怎样区分？

**卡在这里就先不要往下读**：排序改了画面就变，说明有对象被错分到了 Opaque 队列。

## 第三部分：组织并调试完整一帧

### 13.13 一个清晰的帧入口

~~~cpp
void Renderer::renderFrame(const RenderSnapshot& snapshot) {
    FrameQueues queues = buildQueues(snapshot);

    sortOpaqueQueue(queues.opaque);
    sortTransparentQueue(queues.transparent);

    renderShadowPass(queues.shadowCasters);
    renderOpaquePass(queues.opaque);
    renderSkyPass(snapshot.sky);
    renderTransparentPass(queues.transparent);
    renderPostProcessPass();
    renderUiPass(snapshot.ui);
    present();
}
~~~

`RenderSnapshot` 是游戏逻辑在本帧交给 Renderer 的只读数据集合。里面装着渲染需要的全部信息：Camera、Light、RenderItem 和 UI。第 17 章会详细实现它；本章只使用这项边界。

不要为了照抄这段代码而创建尚未使用的空系统。当前项目只实现了 Shadow 和 Opaque 时，真实入口可以先是：

~~~cpp
void Renderer::renderFrame(const Scene& scene, const Camera& camera) {
    FrameQueues queues = buildQueues(scene, camera);
    sortOpaqueQueue(queues.opaque);

    renderShadowPass(queues.shadowCasters);
    renderOpaquePass(queues.opaque, camera);
    present();
}
~~~

第 17 章建立 `RenderSnapshot` 后，再替换这里暂时使用的 `Scene` 和 `Camera` 参数。重点是 Pass 边界，不是提前拥有最终接口。

每个函数的名字已经表达阶段。它们内部应：

1. 绑定 Target。
2. 设置 Viewport。
3. 设置完整状态。
4. Clear 或保留附件。
5. 绑定 Pass 级数据，例如 Camera、Light。
6. 遍历自己的 Queue。
7. 输出统计和调试标记。

---

### 13.14 Pass 级数据与 Draw 级数据

不是所有 Uniform 都要每个 Draw 更新。

#### Pass 级数据

在同一 Pass 中通常不变：

~~~text
Camera View/Projection
Camera Position
Light Direction/Color
Shadow Map
时间、曝光
~~~

#### Draw 级数据

每个对象可能不同：

~~~text
Model Matrix
Normal Matrix
Material 参数
Mesh Buffer
~~~

组织提交：

~~~cpp
bindPassData(camera, lights, shadowMap);

for (const DrawItem& draw : queue) {
    bindMaterial(draw.material);
    bindObjectData(draw.modelMatrix, draw.normalMatrix);
    bindMesh(draw.mesh);
    issueDraw(draw.mesh);
}
~~~

这样能避免在循环里重复上传完全相同的 Camera 数据，也让 RenderDoc 中资源变化更容易理解。

---

### 13.15 不要把 Render Pass 与 OpenGL FBO 混为一谈

一个 FBO 是 OpenGL 中的资源绑定对象；一个 Render Pass 是你设计的一段工作。

- Shadow Pass 通常使用 Shadow FBO。
- Opaque、Sky、Transparent 可以连续使用同一个 Scene FBO，但仍是三个 Pass，因为状态和 Queue 不同。
- Post Process 的多个阶段可能轮流使用两个 FBO。

因此：

~~~text
更换 FBO 通常意味着 Pass 边界
但不更换 FBO 也可能进入新的 Pass
~~~

Pass 是逻辑与数据合同，FBO 是具体 OpenGL 对象。

---

### 13.16 Render Graph 是什么，现在为什么不实现

Render Graph 会把 Pass 和 Resource 读写声明成图，自动计算执行顺序、资源生命周期和屏障。它对大型渲染器很有价值。

本章的 Mermaid 图已经具备 Render Graph 的思维：

~~~text
节点 = Pass
边 = Resource 依赖
~~~

但现在手写固定 Pass 顺序更适合学习，因为你能亲自看到每次 Bind、Clear 和状态设置。等固定流程出现大量可选效果和临时 Texture 时，再考虑抽象 Render Graph。

不要为了“像现代引擎”而先写一个复杂框架，再用两个月调资源别名和依赖排序。

---

### 13.17 调试一帧的办法

#### 13.17.1 Pass 开关

允许分别关闭：

~~~text
Shadow
Sky
Transparent
Post Process
UI
~~~

关闭后应能预测：

- 关 Shadow：直接光仍在，但没有遮挡阴影。
- 关 Sky：背景保持 Clear Color。
- 关 Transparent：玻璃和粒子消失。
- 关 Post：显示线性/HDR 中间结果可能不适合最终观看。
- 关 UI：只剩场景。

#### 13.17.2 中间 Target 预览

在调试面板显示：

- Shadow Depth。
- Scene Color。
- Scene Depth。
- Bloom Bright Texture。
- 最终输出。

#### 13.17.3 Pass 与 Draw 统计

~~~text
Shadow: 120 draws, 420k triangles
Opaque: 85 draws, 310k triangles
Transparent: 14 draws, 22k triangles
UI: 6 draws
~~~

#### 13.17.4 Debug Group

只有运行环境提供 `KHR_debug` 或等价的 Debug Group 函数时，才能用 OpenGL Debug Group 包围 Pass：

~~~cpp
glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Opaque Pass");
renderOpaquePass();
glPopDebugGroup();
~~~

不要只根据 OpenGL 版本字符串就直接调用这些函数；应先由函数加载器确认入口可用。RenderDoc 或支持的 GPU Debugger 中会按名字显示事件层级。

macOS 的 OpenGL 4.1 环境不应把 Debug Group 当成必备能力。最可靠的替代方案是在 CPU 日志中打印 Pass 名、绑定的 FBO、Viewport 和 Draw 数量。它不如 GPU 截帧直观，但仍能回答“程序执行到了哪个 Pass、当时使用了什么状态”。

---

### 13.18 常见错误

| 现象 | 常见原因 | 首先检查 |
| --- | --- | --- |
| 主场景只占窗口一角 | Shadow Viewport 泄漏到 Opaque Pass | 每 Pass 开头设置 Viewport |
| 不透明物体像半透明 | Blend 状态未关闭 | Opaque 合同中显式禁用 Blend |
| 透明出现但场景消失 | Transparent Pass 清了 Color | 检查 Clear 调用位置 |
| 天空覆盖模型 | Depth Function 或 Depth Write 错 | 只运行 Opaque + Sky |
| 后处理读取黑图 | Scene FBO 输出未完成或绑定 Texture 错 | 直接预览 sceneColor |
| UI 被 Bloom 模糊 | UI 在 Post Process 前绘制 | 检查数据依赖顺序 |
| 阴影时有时无 | Shadow Pass Queue 不完整或资源被覆盖 | 统计 Caster，标记 Shadow Target |
| 同一帧结果依赖调用顺序 | 某 Pass 借用了上个 Pass 状态 | 完整记录并应用 Pass State |
| 排序后画面变化 | 非 Opaque 材质被错误放入 Opaque Queue | 打印 Material Alpha Mode |

---

### 13.19 练习

#### 练习 A：画出自己的帧图

为当前项目画出所有 Pass，标出每个 Input 和 Output。不要只写名字，要写具体资源，例如 `shadowDepth`、`sceneColor`。

#### 练习 B：拆分 Render 函数

先将原来的单一 `render()` 拆成当前确实存在的：

~~~text
renderShadowPass
renderOpaquePass
~~~

每个 Pass 开头完整设置 FBO、Viewport、Depth、Blend、Cull。

第 14 章实现透明后，再加入 `renderTransparentPass`。不要写一个没有内容、只为满足目录结构而存在的函数。

#### 练习 C：故意出错实验（制造状态泄漏）

在 Transparent Pass 后不关闭 Blend，观察下一帧 Opaque；在 Shadow Pass 后不恢复 Viewport，观察主画面。然后用 Pass State 修复，并记录现象。

#### 练习 D：排序统计

对同一场景分别使用：

1. 原始场景顺序。
2. 按 Material 排序。
3. 按 Material 分组且组内近到远。

记录 Program、Texture、VAO 切换次数。此练习只观察，不要求立刻得出所有平台通用的性能结论。

#### 练习 E：逐 Pass 禁用

为每个 Pass 增加运行时开关，并在关闭前写下你预测的画面，再对照实际结果。

---

### 13.20 本章验收标准
本章分两层验收。现在必须完成的是：

1. 一帧有明确的 Pass 顺序图和 Resource 依赖。
2. 已实现的 Shadow 和 Opaque 各自拥有状态合同。
3. 这两个 Pass 显式设置 FBO、Viewport、Depth、Blend 和 Cull，不依赖遗留状态。
4. Opaque RenderItem 在提交前完成收集与排序。
5. Shadow 和 Opaque 可以单独关闭，并能打印各自的 Draw 数量。
6. Shadow Depth 能单独显示；Opaque 可用纯色替换 Shader 或截图确认。第 15 章创建 `sceneColor` Texture 后，再把它加入中间资源预览。
7. 你能解释 Pass 与 FBO、Pass 与 Queue 的区别。

读完第 14～16 章后，再回来完成扩展验收：

1. Transparent 使用独立 Queue 和远到近排序，Sky、Post 与 UI 也有明确 Pass 合同。
2. 所有已实现 Pass 都能单独关闭，且关闭后的结果符合预测。
3. 每个 Pass 都有 Draw/Triangle 统计；可用 Debug Group 时再加 GPU 事件标记。

进入下一章前，请确认自己能回答：

- 为什么 Transparent 必须在 Opaque 之后？
- Clear 与保留已有附件内容有什么区别？
- 为什么 OpenGL 状态会产生跨 Pass Bug？
- Opaque 为什么通常可以重排，而 Transparent 不行？
- 一个 Scene FBO 为什么可以被多个 Pass 连续使用？
- 帧图中的箭头表示 CPU 拷贝吗？

下一章将深入三个常见特殊路径：透明混合、无限远天空，以及大量相同 Mesh 的实例化绘制。
