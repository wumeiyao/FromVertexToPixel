# 第 20 章：从 OpenGL 过渡到 Vulkan 或 Metal

<!-- nav -->
> [上一章：性能分析与有证据的优化](19-profiling-optimization.md) ｜ [下一章：毕业项目、交付与技术报告](21-capstone.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/09_profiling_vulkan/20-vulkan-bridge.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

这一章不是要求你立刻用 Vulkan 重写整个项目，而是把此前已经理解的 Renderer 概念，映射到更显式的图形 API。

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 说出无论用哪个 API 都不变的那条 Triangle 到 Pixel 主线。
2. 解释 Command Buffer 为什么出现，以及它和 OpenGL 立即提交的区别。
3. 说明 Pipeline 对象把哪些原本分散的 OpenGL 状态打包在一起。
4. 用 OpenGL 概念对照 Vulkan 的 Device Memory、Image、Descriptor。
5. 解释在途帧（Frame in Flight）以及为什么资源不能过早复用。
6. 区分 Barrier、Semaphore 和 Fence 三种同步各解决什么问题。
7. 在 macOS 上选一条路线（MoltenVK 或 Metal），做到 Clear Color 和一个三角形。

OpenGL、Vulkan 和 Metal 最终都让 GPU 做同样四件事：读取 Vertex 和 Texture，执行 Shader，光栅化 Triangle，把 Fragment 写入 Render Target。主要差异不是“渲染原理变了”，而是谁负责描述状态、资源生命周期和工作先后。

先按四部分建立映射：比较 API 思路、理解命令提交、理解资源与同步，最后迁移一个最小例子。不需要第一次就记住全部 Vulkan 名词。

这一章是“桥梁和地图”，不是 OpenGL 毕业项目的必做功能。建议分两遍阅读：

**第一遍只抓住主干**

1. `20.1～20.8`：哪些渲染原理不变，Command Buffer 和 Pipeline 为什么出现。
2. `20.16～20.18`：窗口图片、在途帧，以及为什么资源不能过早复用。
3. `20.22～20.24`：Barrier、Semaphore、Fence 的职责和三种 API 的概念映射。
4. `20.27` 与 `20.31`：在 macOS 上选一条路线，只做到 Clear Color 和 Triangle。

**第二遍在真正写 Vulkan/Metal 时再细读**

Device Memory、Staging、Descriptor、Image Layout、Hazard、Validation 和完整迁移阶段都属于边写边查的内容。第一次读不需要背对象名称或初始化顺序，只需知道它们分别在表达“资源在哪里、怎样上传、谁能读取、何时可以复用”。

## 第一部分：哪些原理不变，哪些责任改变

### 20.1 不变的核心：Triangle 到 Pixel

无论使用哪个 API，一次基本 Draw 仍需要：

~~~text
Vertex/Index Buffer
+ Shader
+ Texture/Uniform
+ Raster/Depth/Blend State
+ Render Target
             |
             v
          Draw Command
             |
             v
GPU: Vertex -> Rasterization -> Fragment -> Tests/Blend
             |
             v
      Color/Depth Attachment
~~~

第 1–19 章学到的 Model/View/Projection、Depth、Texture、Shadow、Pass、Queue、Framebuffer 和性能分析都不会失效。

如果换 API 后这些概念突然完全陌生，通常说明原有 Renderer 把状态和资源藏得太深，而不是图形原理改变了。

---

### 20.2 API、Driver 和 GPU

- **Graphics API（图形 API）**：程序调用图形功能的接口和规则，例如 OpenGL、Vulkan、Metal。
- **Driver（驱动）**：硬件厂商/系统提供的软件，把 API 命令转换为具体 GPU 能执行的工作。
- **GPU**：实际执行大量图形和计算任务的处理器。

~~~text
你的 Renderer
     |
     | OpenGL / Vulkan / Metal API
     v
Driver / 系统图形运行时
     |
     v
GPU Hardware
~~~

OpenGL Driver 往往替程序推断和整理更多事情。Vulkan 把更多责任交给程序。Metal 也较显式，但与 Apple 平台和工具结合更紧密。

显式不等于“绕过 Driver”或“直接写 GPU 汇编”。Driver 仍然存在，只是它需要猜测的内容更少。

---

### 20.3 OpenGL 是一台有当前状态的机器

OpenGL 常通过“修改当前状态，再发 Draw”工作：

~~~cpp
glUseProgram(program);
glBindVertexArray(vao);
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, baseColorTexture); // macOS OpenGL 4.1 可用写法
glEnable(GL_DEPTH_TEST);
glDisable(GL_BLEND);
glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
~~~

Draw 的含义取决于此前留下的当前 Program、VAO、Texture、FBO 和状态。代码中漏掉一次绑定，可能错误地使用上一个 Pass 的资源。

Driver 还可能在 Draw 时才发现：

- 当前 Shader 和 Vertex Layout 是否兼容。
- 当前 Blend/Depth/Render Target 组合需要哪个内部 Pipeline。
- 某个 Resource 前面被写过，现在是否要等待。
- 命令如何转换并提交到硬件。

这使简单程序很短，也使部分成本和错误在运行时才暴露。

---

### 20.4 显式 API 的核心思想

Vulkan/Metal 更倾向于提前创建明确对象、提前声明兼容关系，并把命令录到 Command Buffer。

~~~text
创建阶段：
Shader + Vertex Layout + Depth/Blend/Raster State + Attachment Format
                              |
                              v
                           Pipeline

每帧：
开始 Command Buffer
-> 开始 Render Pass
-> 绑定 Pipeline
-> 绑定 Buffer/Texture 参数
-> Draw
-> 结束 Pass
-> 提交 Queue
~~~

代码量增加的原因不是多了一种光照，而是程序需要显式表达 OpenGL Driver 曾经替你保存或推断的关系。

收益包括：

- Pipeline 和状态更容易缓存与复用。
- 多核 CPU 可以更好地准备命令，但这不是初学版必须实现的目标。
- 资源读写依赖更可控。
- Driver 运行时猜测与隐藏同步更少。
- 调试工具更容易展示明确对象。

代价包括更复杂的初始化、内存、同步、错误处理和平台代码。

---

#### 本部分检查点

**能做出来**：用一页纸写出“哪些渲染原理不变、哪些责任从驱动转给了你”。

**能回答**：

- 显式 API 的核心思想是什么？
- OpenGL 的隐式状态机在 Vulkan 里变成了什么？
- 为什么说迁移不是“渲染原理变了”？

**卡在这里就先不要往下读**：这一部分是全章的地图。没建立起来就去看 Descriptor 和 Image Layout，只会变成背名词。

## 第二部分：GPU 工作怎样被描述和提交

### 20.5 先理解异步：CPU 和 GPU 是两个执行者

假设小林写任务单，小王按任务单画图：

~~~text
小林（CPU）：写“画三角形 A”，继续写“画三角形 B”
小王（GPU）：稍后读取任务单，依次真正画 A、B
~~~

小林写完一句话，不代表小王已经画完。CPU 提交命令后通常继续运行；GPU 按自己的节奏执行。

这叫 **Asynchronous Execution（异步执行）**：两个执行者不必每一步同时开始、同时结束。

异步不要求你先会多线程。即使你的应用只有一个 CPU 主线程，GPU 本身仍是另一个独立执行设备。

问题出现在二者共用资源时：

~~~text
CPU 想重写 Buffer
但 GPU 还在读取上一帧的同一 Buffer
~~~

或者：

~~~text
Shadow Pass 正在写 Shadow Map
Main Pass 想读取 Shadow Map
~~~

同步就是明确这些先后关系。

---

### 20.6 Queue：GPU 的工作入口

**Queue（队列）**是程序向 GPU 提交批次工作的入口。命令按 API 规则排队执行。

常见能力：

- Graphics：Draw、Rasterization、Render Pass。
- Compute：Compute Shader。
- Transfer/Copy：Buffer 与 Image 复制。

某个硬件可能提供一个同时支持多种能力的 Queue Family，也可能有多个不同 Queue。初学版不需要多队列：选择一个支持 Graphics 和 Present 的 Queue，就足以画三角形和完整小项目。

多 Queue 并不自动更快，还会增加资源所有权和 Semaphore 同步。先建立单 Queue 正确实现。

---

### 20.7 Command Buffer：预先录好的命令清单

**Command Buffer** 保存即将交给 GPU 的命令。

~~~text
Command Buffer #42
1. 将 sceneColor 转为颜色附件写入状态
2. 开始 Main Pass
3. 绑定 Opaque Pipeline
4. 绑定 Camera/Material Resource
5. Draw Mesh A
6. Draw Mesh B
7. 结束 Main Pass
8. 将 sceneColor 转为 Shader 读取状态
9. 开始 Post-process Pass
10. Draw Fullscreen Triangle
~~~

“录制”不等于 GPU 已执行。通常流程是：

~~~text
CPU begin/record/end Command Buffer
-> submit 到 Queue
-> GPU 稍后执行
-> Fence/Semaphore 反映完成与依赖
~~~

Vulkan 的 Command Pool 用于分配/重置 Command Buffer；Metal 使用 `MTLCommandQueue` 创建 `MTLCommandBuffer`。名字不同，思路相近。

---

### 20.8 Pipeline State Object

在 OpenGL 中，Program、Blend、Depth、Cull、Vertex Layout 等是分散的当前状态。Vulkan/Metal 通常将大量状态提前组合成 **Pipeline**。

图形 Pipeline 可能包含：

- Vertex/Fragment Shader。
- Vertex Attribute Layout。
- Triangle 拓扑。
- Rasterization 和 Back-face Culling。
- Depth/Stencil Test。
- Blend State。
- Render Target Format 和 Sample Count 的兼容信息。

~~~text
Opaque Pipeline：Depth Test on, Depth Write on, Blend off
Transparent Pipeline：Depth Test on, Depth Write off, Blend on
UI Pipeline：Depth off, Blend on, UI Vertex Layout
~~~

Pipeline 创建可能昂贵，因此通常在加载或缓存阶段创建，不应每个对象每帧重新创建。

“换一个布尔状态”在显式 API 中可能意味着选择另一个已创建 Pipeline，而不是临时修改全局状态。

---

### 20.9 Shader 不再靠字符串名字随意查参数

OpenGL 学习代码常用：

~~~cpp
glUniformMatrix4fv(glGetUniformLocation(program, "uView"), ...);
~~~

Vulkan 通常使用 Descriptor Set/Layout 和 Push Constant；Metal 使用 Buffer/Texture/Sampler 绑定索引，较新方案也可用 Argument Buffer。

它们共同解决：

> 这个 Shader 本次 Draw 可以从哪些固定位置读取哪些资源？

例如把资源按更新频率分组：

~~~text
Set 0 / Frame：Camera、时间、全局灯光
Set 1 / Material：Base Color、Normal、Sampler、参数
Set 2 / Object：Model Matrix 或实例数据
~~~

**Descriptor** 可以理解为“对 Buffer/Image/Sampler 的结构化引用”。Descriptor 不等于 Texture 数据本身，它告诉 GPU 到哪里找资源、以什么方式使用。

**Descriptor Set Layout** 描述一组槽位的类型；**Pipeline Layout** 描述 Pipeline 能访问哪些 Descriptor Set 和 Push Constant。

---

### 20.10 Push Constant 与小量高频数据

**Push Constant** 是命令中携带的一小块快速参数，适合 Model ID、少量 Draw 参数等。容量有限，不适合塞整张 Texture 或巨大数组。

Metal 常通过 `setVertexBytes` 等方式提供小量数据，但大量或频繁数据仍应使用 Buffer。

选择原则不是“哪个 API 调用短”，而是数据大小和更新频率：

~~~text
几乎不变：Texture、Mesh、Material Pipeline
每帧变化：Camera Buffer、Light Buffer
每对象变化：Model Matrix、Object ID
大量重复对象：Instance Buffer
~~~

这种分组也能反过来改善 OpenGL Renderer 的设计。

---

#### 本部分检查点

**能做出来**：对照写出一张表：OpenGL 里的一次 Draw，对应 Vulkan 要准备哪些对象。

**能回答**：

- Command Buffer 为什么要预先录制？
- Pipeline 打包了哪些原本分散的状态？
- Push Constant 适合放什么数据？

**卡在这里就先不要往下读**：不必记住函数名。能说出“这个对象在表达什么”就可以继续。

## 第三部分：资源、状态与同步

### 20.11 Buffer/Image 与 Device Memory

OpenGL 中 `glBufferData`、`glTexImage2D` 同时完成了不少隐式存储工作。Vulkan 更明确地区分：

- **Buffer/Image Object**：资源的类型、大小、格式、用途描述。
- **Device Memory**：实际分配的 GPU 可访问内存。
- **Binding**：把资源对象绑定到一段合适内存。

~~~text
创建 Buffer（需要 1 MB，作为 Vertex）
-> 查询内存要求
-> 选择兼容 Memory Type
-> 分配/取得一段 Device Memory
-> bind Buffer 到这段 Memory
~~~

频繁为每个小资源单独做底层分配会昂贵并产生**内存碎片**：空闲空间被切成许多不连续小块，虽然总量够，却难以满足较大连续分配。因此真实项目常使用 Memory Allocator，例如 Vulkan Memory Allocator。使用库并不会隐藏“资源必须有存储”这个原理，只是封装重复分配策略。

Metal 的 `MTLBuffer`/`MTLTexture` 创建接口更集成，但仍需选择 Storage Mode，表达 CPU/GPU 可见性和存储位置。

---

### 20.12 Staging Buffer：怎样把 CPU 数据上传到 GPU

GPU 使用的高速本地资源不一定能让 CPU 直接高效写入。常见上传流程：

~~~text
CPU Vertex/Image Data
-> 写入 CPU 可见 Staging Buffer
-> 录制 Copy Command
-> GPU Copy 到 Device-local Buffer/Image
-> Barrier 转为 Vertex Read 或 Shader Sample 状态
-> Staging Buffer 在复制完成后复用/释放
~~~

**Staging** 就是中转站。

不要在 GPU 尚未复制完成前销毁或覆盖 Staging Memory。上传系统也需要 Fence 或帧生命周期跟踪。

小型 Metal 程序在统一内存设备上可能直接使用 Shared Storage，但仍应理解 CPU 写入和 GPU 读取的先后关系。

---

### 20.13 Image、Image View 和 Sampler

显式 API 常把 OpenGL Texture 的几个角色拆开：

- **Image / Texture**：真正保存 Texel 的资源。
- **Image View / Texture View**：以某种格式、Mipmap 范围或数组层查看 Image。
- **Sampler**：过滤、Wrap、LOD 等采样规则。

同一个 Image 可以有不同 View，例如只看某个 Mipmap 或只看 Depth Aspect。Sampler 也可被多个 Texture 共用。

这解释了为什么“图片数据”和“怎样读取图片”是两个概念。

---

### 20.14 Render Attachment、Render Pass 与 Dynamic Rendering

第 15 章的 FBO 把 Color/Depth Texture 设为写入目标。Vulkan 中常称它们为 **Attachment**。

一个 Pass 需要描述：

- 写哪些 Color/Depth Attachment。
- 开始时是 Clear、Load 旧内容，还是不关心旧内容。
- 结束后内容是否要 Store。
- Attachment Format 和 Sample Count。
- 前后怎样使用资源。

传统 Vulkan 使用 Render Pass 和 Framebuffer 对象；现代 Vulkan 也可使用 **Dynamic Rendering** 在录制时明确附件，减少部分对象组合。两者不改变“Pass 有输入、输出和状态”的原理。

Metal 先用 `MTLRenderPassDescriptor` 描述附件和 Load/Store Action，再由它创建 Render Command Encoder。**Encoder** 是把某一类命令（例如 Render、Compute 或 Blit）写入 Metal Command Buffer 的记录接口。

---

### 20.15 Load、Clear、Store 为什么要明确

Attachment 开始和结束行为会影响正确性与带宽：

- **Load**：保留并读取之前内容。
- **Clear**：开始时填充固定值。
- **Don't Care**：旧内容不重要，允许实现不保留。
- **Store**：结束后结果还要被后续 Pass 或 Present 使用。
- **Discard/Don't Care**：结束后内容不再需要。

例如 Shadow Depth 每帧重画：开始可 Clear，结束必须 Store，因为 Main Pass 要采样。临时 Depth 若后续不再读取，结束时可以不要求 Store。

显式说明“不需要旧内容”能避免不必要的内存读写。

---

### 20.16 Surface、Swapchain 和 Present

窗口系统提供可显示的 **Surface**。Vulkan 的 **Swapchain** 管理若干张可呈现 Image；Metal 通常从 `CAMetalLayer` 取得一个 Drawable。

一帧的呈现过程：

~~~text
Acquire：取得一张目前可用于绘制的 Swapchain Image
-> 把它作为最终 Color Attachment
-> 录制并提交 Draw
-> Present：请求窗口系统显示这张 Image
~~~

Swapchain Image 不是永远固定对应“第 0 帧、第 1 帧”。每帧应使用 Acquire 返回的 Image Index。

窗口 resize、最小化或显示模式变化时，Swapchain 可能失效。此时要重建所有与它尺寸或格式相关的资源：Image View、Depth 附件，以及依赖这些格式的 Framebuffer 和 Pipeline。

这与 OpenGL 中 resize 后重建离屏附件是同一种生命周期问题，只是 Vulkan 要表达得更明确。

---

### 20.17 Frames in Flight

如果 CPU 每次提交后都等 GPU 完成，二者无法重叠：

~~~text
CPU: 准备 -> 等待 GPU -> 准备 -> 等待 GPU
GPU:       执行 -> 空闲 -> 执行
~~~

通常允许 2 或 3 个 **Frames in Flight（在途帧）**：CPU 可以准备较新的帧，而 GPU 执行较旧的帧。

~~~text
CPU: [Frame 0][Frame 1][Frame 2]
GPU:         [Frame 0][Frame 1][Frame 2]
~~~

但每个在途帧需要自己的一组会被改写的数据：

- Command Buffer。
- Camera/Frame Uniform Buffer 区域。
- Descriptor Set 或参数区。
- Fence 和必要 Semaphore。

若所有帧都写同一 Camera Buffer，CPU 可能在 GPU 读取 Frame 0 数据时覆盖成 Frame 2 相机。

更多在途帧可以提高吞吐，也可能增加输入延迟和内存。第一次画 Clear Color 或 Triangle 时，可以先只使用 1 个 Frame Slot：每帧等待它的 Fence，再复用它的命令和参数，优先理解正确生命周期。闭环稳定后再改成 2 个 Frame Slot，观察 CPU/GPU 怎样重叠；不要一开始复制多帧模板却不知道每个数组索引保护什么。

---

### 20.18 同步的三个问题：顺序、可见、可复用

同步不是一个单独按钮。先问具体要解决什么：

1. **Execution Order（执行顺序）**：B 是否必须在 A 执行后开始相关阶段？
2. **Memory Visibility（内存可见性）**：A 写的数据，B 是否保证能看到？
3. **Lifetime/Reuse（生命周期/复用）**：CPU 是否可以重新使用该 Command Buffer 或 Buffer？

一个生活例子：

~~~text
A 在纸上写答案
B 要读答案
~~~

仅保证 B 晚一点走进房间还不够，还要保证 A 已经把答案写完，并且那张纸正是 B 读取的版本。显式 API 的 Barrier 会同时表达相关执行阶段和内存访问关系。

---

### 20.19 三种资源冲突（Hazard）

假设 A、B 是按时间发生的 GPU 工作。

#### Read After Write（RAW，写后读）

A 写 Shadow Map，B 随后读它。B 必须等 A 写完，并看到写入结果。这是渲染中最常见依赖。

#### Write After Read（WAR，读后写）

A 正在读一张 Texture，B 想覆盖它。B 不能在 A 读完前开始破坏数据。

#### Write After Write（WAW，写后写）

A、B 都写同一资源，需要明确哪次写最终生效，不能无序重叠。

只读和只读通常可以同时发生，因为彼此不修改数据。

---

### 20.20 Pipeline Stage 和 Access

GPU Draw 内部也有阶段，例如 Vertex Input、Vertex Shader、Fragment Shader、Color Output。Barrier 需要大致说明：

- 前面的哪类阶段做了什么访问。
- 后面的哪类阶段准备做什么访问。

例如：

~~~text
前：Depth/Attachment 阶段写 Shadow Depth
后：Fragment Shader 读取 Shadow Map
~~~

太宽的同步（“所有阶段等所有阶段”）可能正确但失去并行；太窄则可能漏掉真实依赖。初学阶段先写正确、清晰的同步，再用验证层和 Profiler 调整。

Metal 许多同一 Command Buffer/Encoder 的顺序由 API 模型保证，但跨 Encoder、资源用途或多 Queue 时仍需理解依赖。不要把“代码里没写 Vulkan Barrier”误解为“没有同步规则”。

---

### 20.21 Image Layout / Resource State

Vulkan Image 有 Layout，其他 API 也有类似 Resource Usage/State 思维。它告诉实现当前怎样组织和使用 Image，例如：

- Color Attachment Write。
- Depth Attachment Write。
- Shader Sampled Read。
- Transfer Source/Destination。
- Present。

同一张 Scene Color 的一帧角色变化：

~~~text
Undefined/旧内容不需要
-> Color Attachment Write（主场景写入）
-> Shader Sampled Read（后处理采样）
-> 后处理写入 Swapchain Image
~~~

“状态转换”不是修改图片内容，而是告诉 GPU：之前的访问完成后，接下来将以另一种方式使用它，并建立必要可见性。

**Subresource** 指资源的一个具体部分，例如某一级 Mipmap、数组中的某一层，或 Depth/Stencil 中的某一个 Aspect。

规则是：同一时刻不要把同一个 Subresource 既当颜色附件写、又当 Shader Texture 读。除非你用的是 API 明确定义的特殊机制，否则这就是一个 Hazard。

---

### 20.22 Barrier、Semaphore 和 Fence

#### Barrier

主要用于 Command 工作内部或 Queue 上的资源执行/内存依赖：

~~~text
Shadow Depth Write
-- Barrier -->
Fragment Shader Sample Read
~~~

#### Semaphore

主要用于 GPU 提交之间或 Acquire/Present 的先后关系。Vulkan 中常见：

~~~text
Image Available Semaphore：Swapchain Image 可以开始用
Render Finished Semaphore：绘制完成，可以 Present
~~~

Timeline Semaphore 还可以用递增数值表达多个进度点；第一版可先使用 **Binary Semaphore（二值信号量）**，它只有“未发出信号”和“已发出信号”两类状态。

#### Fence

让 CPU 查询或等待某次 GPU 提交完成。常用于：

~~~text
本 Frame Slot 的 Fence 已完成
-> CPU 可以重置其 Command Buffer
-> 可以覆盖该 Slot 的 Uniform 数据
-> 可以再次用于新帧
~~~

简单记忆：

~~~text
Barrier：GPU 工作中的资源依赖
Semaphore：GPU 提交/呈现之间传递完成信号
Fence：GPU 告诉 CPU“这批工作完成了”
~~~

这只是入门分类；具体 API 允许更丰富用法。遇到代码时仍要问“谁等谁、保护哪份数据”。

---

#### 本部分检查点

**能做出来**：用一句话分别说明 Barrier、Semaphore、Fence 各解决什么问题，并举一个用错的后果。

**能回答**：

- 为什么资源不能过早复用？
- 在途帧（Frame in Flight）是什么，为什么先用 1 个？
- Image Layout 转换在 OpenGL 里由谁负责？

**卡在这里就先不要往下读**：三种同步分不清就不要去写真实 Vulkan 代码，那会是最难调的一类 Bug。

## 第四部分：从概念映射到最小迁移

### 20.23 一帧 Vulkan 风格伪代码

下面不是可直接编译代码，而是展示职责顺序：

~~~cpp
FrameContext& frame = frames[currentFrame];

waitForFence(frame.inFlightFence);       // GPU 已不用本帧槽资源
resetFence(frame.inFlightFence);

uint32_t imageIndex = acquireSwapchainImage(
    frame.imageAvailableSemaphore
);

updateFrameBuffers(frame, snapshot);     // 只改该 Frame Slot 的数据
resetCommandBuffer(frame.commandBuffer);

beginCommands(frame.commandBuffer);

transition(sceneColor,
           ShaderRead,
           ColorAttachmentWrite);

beginMainPass(frame.commandBuffer, sceneColor, sceneDepth);
bindPipeline(opaquePipeline);
drawOpaque(snapshot.opaqueItems);
endMainPass();

transition(sceneColor,
           ColorAttachmentWrite,
           ShaderRead);

transition(swapchainImages[imageIndex],
           Present,
           ColorAttachmentWrite);

beginPostProcessPass(swapchainImages[imageIndex]);
bindPipeline(toneMapPipeline);
bindTexture(sceneColor);
drawFullscreenTriangle();
endPostProcessPass();

transition(swapchainImages[imageIndex],
           ColorAttachmentWrite,
           Present);

endCommands(frame.commandBuffer);

submitGraphics(
    frame.commandBuffer,
    frame.imageAvailableSemaphore,
    frame.renderFinishedSemaphore,
    frame.inFlightFence
);

present(imageIndex, frame.renderFinishedSemaphore);
currentFrame = (currentFrame + 1) % frames.size();
~~~

逐段理解：

1. 等待的是当前 Frame Slot 上一次用途，不是每次把整个 GPU 清空。
2. Acquire 给出本次实际可画的 Swapchain Image。
3. 每个 Slot 有自己的可改写参数和 Command Buffer。
4. Scene Color 从采样状态转为写入，再转回采样。
5. Swapchain Image 从 Present 转为写入，最后转回 Present。
6. Semaphore 连接 Acquire、Draw、Present；Fence 连接 GPU 完成与 CPU 复用。

---

### 20.24 OpenGL、Vulkan、Metal 的概念映射

| 渲染意图 | OpenGL | Vulkan | Metal |
| --- | --- | --- | --- |
| 连接图形设备 | Context | Instance + Physical/Logical Device | MTLDevice |
| 向 GPU 提交 | 隐式 Context/Driver | Queue | MTLCommandQueue |
| 命令清单 | 调用即时进入驱动 | VkCommandBuffer | MTLCommandBuffer + Encoder |
| Shader/固定状态 | Program + 分散状态 | VkPipeline | MTLRenderPipelineState + DepthStencilState |
| Vertex/Uniform 数据 | Buffer | VkBuffer + Memory | MTLBuffer |
| Texture 数据 | Texture | VkImage + Memory + ImageView | MTLTexture |
| 采样规则 | Sampler | VkSampler | MTLSamplerState |
| Shader 资源绑定 | Binding/Uniform Location | Descriptor Set | Buffer/Texture Index 或 Argument Buffer |
| 离屏写入目标 | FBO | Attachment + Rendering/Render Pass | Render Pass Descriptor |
| 窗口画面 | Default Framebuffer | Surface + Swapchain | CAMetalLayer + Drawable |
| 资源依赖 | Driver 多数隐式处理 | Barrier/Layout/Semaphore | Encoder/Resource Usage/Event/Fence 等规则 |
| CPU 等 GPU | glFenceSync/glFinish 等 | VkFence | Command Buffer Completion / Shared Event |

表格是思维映射，不表示 API 一一等价。学习时先问渲染意图，再查该 API 的具体对象。

---

### 20.25 Vulkan 初始化对象的直觉顺序

Vulkan 的初始化名称很多，可以按“从系统到一帧”分层记忆：

~~~text
Instance：本应用使用 Vulkan 的入口
-> Physical Device：选择哪块 GPU
-> Queue Family：选择 GPU 提供的工作能力
-> Logical Device：创建本应用使用的设备接口和 Queue
-> Surface：连接窗口系统
-> Swapchain：创建可呈现 Image
-> Image View / Depth / Frame Resource
-> Descriptor Layout / Pipeline Layout
-> Shader Module / Pipeline
-> Command Pool / Command Buffer
-> Semaphore / Fence
-> Game Loop
~~~

不要死背 Handle 创建顺序。每个对象都回答一个问题：用哪块设备、往哪里提交、画到哪张图、Shader 读什么、何时可以复用。

---

### 20.26 Metal 初始化对象的直觉顺序

Metal 在 macOS 上更直接：

~~~text
MTLDevice：GPU 接口
-> MTLCommandQueue：命令提交入口
-> CAMetalLayer/MTKView：窗口可显示目标
-> Shader Function / Library
-> Render Pipeline State / Depth State
-> Buffer / Texture / Sampler
-> 每帧取得 Drawable
-> Command Buffer
-> Render Command Encoder
-> Draw
-> present Drawable
-> commit
~~~

使用 `MTKView` 可以封装 Drawable 和 resize 等窗口细节，但不会替你决定 Mesh、Pass、Pipeline 和资源边界。

---

### 20.27 macOS 上选择 Vulkan + MoltenVK 还是 Metal

macOS 原生现代图形 API 是 Metal。Vulkan 通常通过 **MoltenVK** 把 Vulkan 调用映射到 Metal。

#### 选择 Metal

适合：

- 重点学习 Apple 平台原生图形栈。
- 希望使用 Xcode GPU Frame Capture 和 Apple 文档。
- 不要求同一 Renderer API 直接运行 Windows/Linux。

#### 选择 Vulkan + MoltenVK

适合：

- 重点学习 Vulkan 显式模型。
- 未来希望迁移 Windows/Linux Vulkan。
- 愿意处理 Vulkan Portability 与 MoltenVK 差异。

MoltenVK 不是“macOS GPU 原生支持 Vulkan”，而是实现层。某些功能、性能和调试路径会受 Metal 能力影响。

对本课程建议：先完成 OpenGL 毕业项目，再选其中一条，用固定三角形和单 Pass 重新建立最小闭环。

---

### 20.28 Shader 语言与中间表示

- OpenGL 常使用 GLSL。
- Vulkan 通常消费 SPIR-V，可以由 GLSL 或 HLSL 编译得到。
- Metal 使用 MSL（Metal Shading Language）和编译后的 Library。

迁移不应从手工翻译所有 Shader 开始。先选最简单 Vertex/Fragment Shader，并保持：

- 相同 Vertex Attribute 含义。
- 相同 Uniform/Buffer 数据布局。
- 相同颜色空间和 Attachment Format。
- 相同坐标系约定，特别是 Clip Space y 和深度范围。

Vulkan 的 NDC 深度通常为 `0..1`，OpenGL 传统为 `-1..1`；Framebuffer y 方向与投影约定也可能不同。若三角形倒置或深度错误，先检查坐标约定，不要先怀疑 Rasterizer。

---

### 20.29 迁移前先整理 OpenGL Renderer

不要直接把每个 `gl*` 调用替换成 `vk*` 调用。先让 OpenGL 版具备明确结构：

~~~cpp
struct GraphicsPipelineDesc {
    ShaderHandle shader;
    VertexLayout vertexLayout;
    DepthState depth;
    BlendState blend;
    RasterState raster;
    AttachmentFormats attachments;
};

struct PassResourceUse {
    ResourceHandle resource;
    Access access; // Read / Write
    Usage usage;   // Sampled / ColorAttachment / DepthAttachment
};
~~~

还要做到：

- 每个 Pass 有明确输入、输出、尺寸和格式。
- 资源由 Asset/Render Resource Manager 管理。
- Snapshot 不包含 OpenGL 全局状态假设。
- Pipeline/Material/Geometry Handle 与游戏对象分离。
- 每个 Pass 自己设置状态。
- 调试标签和性能范围完整。

这些整理对 OpenGL 本身也有价值，不是为 Vulkan 制造无用抽象。

---

### 20.30 资源读写表：显式 API 的设计起点

为每个 Pass 写表：

| Pass | 读取 | 写入 | 开始行为 | 结束后用途 |
| --- | --- | --- | --- | --- |
| Shadow | Shadow Caster Mesh | Shadow Depth | Clear | Fragment Sample |
| Opaque | Shadow Depth、Material | Scene Color、Scene Depth | Clear | Color 被后处理读 |
| Transparent | Scene Depth、Material | Scene Color | Load | Color 被后处理读 |
| Bloom Extract | Scene Color | Bloom A | Clear | Blur Read |
| Tone Map | Scene Color、Bloom | Swapchain Image | Clear | Present |
| UI | Font/Icon、现有 Color | Swapchain Image | Load | Present |

从表中可以直接发现依赖：

~~~text
Shadow Write -> Main Fragment Read
Scene Color Write -> Bloom/Tone Map Read
Swapchain Color Write -> Present
~~~

这张表比一开始写 Barrier 代码更重要。Barrier 只是把真实数据依赖翻译给 API。

---

### 20.31 最小迁移路线

#### 阶段 1：固定清屏

创建窗口、设备、Swapchain/Drawable，每帧清为固定颜色并 Present。

验收：resize 后仍正确，验证层无错误。可以先使用 1 个 Frame Slot。

#### 阶段 2：硬编码 Triangle

创建 Pipeline、Vertex Buffer、Command Buffer，画一个无 Texture Triangle。

验收：能用图形调试器看到 Vertex 和 Pipeline。

完成这里就已经达到本章第一次实践目标。先写一页概念对照和问题记录，再决定是否继续迁移 Camera；后续阶段不是本章必须一次完成的作业。

#### 阶段 3：Camera 与 Uniform

接入每帧 Buffer 和 Frames in Flight，不覆盖 GPU 正在读的数据。

#### 阶段 4：Texture 与上传

使用 Staging、Image View、Sampler、Descriptor/Binding，画带 Texture Mesh。

#### 阶段 5：Depth 与多个对象

加入 Depth Attachment、模型矩阵和 Render Item。

#### 阶段 6：离屏 Scene Color 和后处理

建立第一个真实的写后读 Barrier。

#### 阶段 7：Shadow Pass

加入另一个明确 Pass 及 Shadow Depth 依赖。

#### 阶段 8：接回现有 Snapshot

同一份游戏 Snapshot 可选择交给 OpenGL 或新 Renderer，方便对照截图。

一次只建立一个可验证闭环。

---

### 20.32 Validation Layer 与 GPU 调试

Vulkan Validation Layer 会检查大量 API 使用错误：

- 对象生命周期。
- Descriptor/Binding 不匹配。
- Image Layout 和同步。
- Command Buffer 状态。
- Render Pass/Pipeline 兼容性。

它不会保证 Shader 数学正确，也不会保证画面符合设计。

开发时：

- 开启 Validation。
- 给对象设置 Debug Name。
- 给 Command Buffer 插入 Debug Label/Region。
- 一次处理第一条高严重度消息，后续错误可能只是连锁反应。
- 使用 RenderDoc（支持条件依平台）或 Xcode GPU Capture 检查一帧。

不要通过关闭 Validation 来“修复”错误。

---

### 20.33 常见显式 API 错误

#### 画面一直是 Clear Color

按顺序检查这五件事：

1. 是否 Acquire 到了正确的 Swapchain Image。
2. Command Buffer 是否真的提交了。
3. Pipeline 与 Attachment 的格式是否兼容。
4. Viewport 和 Scissor 是否设置过。
5. Present 是否等待了 Render Finished 信号。

#### 只有第一帧正常

检查 Fence reset/wait、Command Buffer reset、Frame Index 和 Swapchain Image Index 是否混淆。

#### 间歇闪烁或不同机器结果不同

高度怀疑缺失同步、资源被提前复用、未初始化内容或错误 Layout。

#### Resize 后崩溃

检查是否等待/处理旧 Swapchain 相关工作，并按依赖顺序重建 Image View、Depth、Framebuffer 等尺寸资源。

#### Texture 错乱

检查 Descriptor 生命周期、Image View、Sampler、Layout、上传完成同步和数据行距。

#### Validation 报“正在使用时销毁”

CPU 释放了 GPU 尚未完成使用的对象。使用 Frame Fence 或延迟删除队列。

#### CPU 每帧等到 GPU 完全空闲

检查是否滥用 `deviceWaitIdle`、立即等待当前提交。正常帧循环只等待要复用的 Frame Slot。

---

### 20.34 不要过早写“跨 API 通用抽象层”

初学者容易在第二个 API 还没画出三角形前设计：

~~~text
IGraphicsDevice
IUniversalCommandGraph
IAnyPlatformTexture
...
~~~

没有两套真实实现经验时，这些接口通常只是用 OpenGL 术语伪装 Vulkan，或把 Vulkan 的必要信息藏掉。

更可靠的顺序：

1. 保留与 API 无关的 Snapshot、Mesh Asset、Material 描述和 Pass 需求。
2. 分别写小而直接的 OpenGL 与 Vulkan/Metal 后端。
3. 观察真实重复和差异。
4. 只有能消除实质重复时再抽象。

不要为了“跨平台”牺牲每个 API 的可解释性。

---

### 20.35 常见误解

#### 误解一：Vulkan 画面天然比 OpenGL 更好

相同 Shader、资源和算法可以得到相近画面。Vulkan 提供控制与可预测性，不自动增加画质。

#### 误解二：Vulkan 一定更快

写得不当可能更慢。优势需要正确批处理、Pipeline 缓存、内存和同步设计才能体现。

#### 误解三：Command Buffer 录完后 GPU 已经完成

录制只是 CPU 写命令；提交后 GPU 才执行。

#### 误解四：Fence 和 Semaphore 是同一个“等待对象”

它们面对的执行者和使用位置不同。始终说清谁等待谁。

#### 误解五：Barrier 只是把 Image Layout 枚举改掉

它表达执行与内存依赖；Layout 只是 Image 使用状态的一部分。

#### 误解六：有两个 Frames in Flight 就只会用 Swapchain 第 0、1 张图

Frame Slot 与 Acquire 返回的 Swapchain Image Index 是两个不同索引。

#### 误解七：迁移必须一次重写整个游戏

Game、Physics、Audio 和 Snapshot 可以保留，只替换 Renderer 的输入消费方式。

---

### 20.36 练习

#### 练习 A：概念映射

选一个 OpenGL 主场景 Pass，列出它用到的 Program、VAO、Texture、FBO 和 Depth/Blend State。然后写出这些东西在 Vulkan 和 Metal 里分别对应哪个明确对象。

#### 练习 B：命令清单

不用写 API 代码，完整写出一帧 Shadow、Opaque、Post-process、UI 的 Command Buffer 伪命令顺序。

#### 练习 C：资源读写表

为项目每张 Render Target 标记每个 Pass 的 Read/Write、Usage、Load/Store 和下一用途。找出所有写后读依赖。

#### 练习 D：同步手算

解释以下三种情况需要 Barrier、Semaphore 还是 Fence，以及谁等待谁：

- Shadow Pass 写完后 Main Pass 采样。
- Acquire 的 Swapchain Image 可用后 Graphics Submit 才写。
- CPU 想重写两帧前的 Uniform Buffer。

#### 练习 E：Frames in Flight

画出 2 个 Frame Slot、3 张 Swapchain Image 的可能时间线。说明为什么 `currentFrame` 不能代替 `imageIndex`。

#### 练习 F：最小现代 API 原型

在独立目录完成 Clear Color 和一个 Triangle。开启 Validation/Metal Debug，窗口 resize 正常。暂时不要接模型加载、ImGui 和 Shadow。

#### 练习 G：离屏写后读

完成 Scene Color -> Fullscreen Triangle。用调试工具验证 Scene Color 在第一 Pass 是 Attachment，在第二 Pass 是 Sampled Texture。

#### 练习 H：故意出错实验（推演显式 API 的失败）

这一章不要求你写出完整的 Vulkan 程序，所以这次以推演为主。对每条，写出**会出现什么现象**以及**OpenGL 里为什么不会遇到它**：

1. 提交 Command Buffer 之后立刻改写它引用的 Uniform Buffer。
2. 只用一个 Frame Slot，但不等待它的 Fence。
3. 把一张 Texture 当颜色附件写的同时，在 Shader 里采样它。
4. Present 不等待 Render Finished 信号。
5. 窗口 resize 后不重建 Swapchain 相关资源。
6. Pipeline 声明的附件格式与实际 Render Pass 不一致。

这六条对应的正是 OpenGL 驱动一直在替你做的事。能说清每一条，就说明你理解了“责任转移”这件事——这比记住函数名重要。

如果你已经做了练习 F 的最小原型，至少把第 2 和第 4 条真的跑一次。显式 API 的同步 Bug 往往表现为“偶尔花屏”或“偶尔卡住”，只有亲眼见过才认得。

---

### 20.37 本章验收标准
进入第 21 章不要求先重写 Renderer。第一遍的核心验收是：

- 能画出 API、Driver、Queue、Command Buffer 和 GPU 的关系。
- 能解释 OpenGL 当前状态与显式 Pipeline 的差异。
- 能画出 Acquire -> Record -> Submit -> Present 的一帧流程。
- 能用“谁等待谁、保护哪份数据”解释 Barrier、Semaphore 和 Fence。
- 已为现有 Renderer 写出逐 Pass 资源读写表。

真正开始 Vulkan/Metal 原型后，再完成实践验收：

- Clear Color 与一个无 Texture Triangle 能稳定显示并正确 Resize。
- 验证工具开启时没有未处理的高严重度错误。
- 能区分 Buffer/Image Object、实际 Memory、View、Sampler 和 Descriptor 引用。
- 从 1 个扩展到 2 个 Frame Slot 后，能区分 Frame Slot 与 Swapchain Image Index。
- 能识别原型中实际遇到的 RAW、WAR 或 WAW，并解释对应同步。

迁移 Camera、Texture、Depth、后处理和 Shadow 是后续路线，不是读完本章的前置门槛。

你还应能用自己的话回答：

1. 为什么 Vulkan 代码更多，却没有改变 Triangle 到 Pixel 的原理？
2. CPU 提交 Command Buffer 后为什么不能立刻覆盖其中引用的数据？
3. Shadow Map 从 Depth Write 变为 Fragment Read 时真正需要保证什么？
4. Fence 为什么常与 Frame Slot 绑定？
5. 为什么 `deviceWaitIdle` 不应成为正常每帧同步方案？
6. 为什么应该先迁移一个 Triangle，而不是直接迁移完整毕业项目？

第 21 章会回到课程主目标：用 OpenGL 版完成一个从启动到结算都可运行、可解释、可测量的小型 3D 游戏。
