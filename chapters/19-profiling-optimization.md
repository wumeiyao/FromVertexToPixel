# 第 19 章：性能分析与有证据的优化

<!-- nav -->
> [上一章：物理、音频与资产生命周期](18-physics-audio-assets.md) ｜ [下一章：从 OpenGL 过渡到 Vulkan 或 Metal](20-vulkan-bridge.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/09_profiling_vulkan/19-profiling-optimization.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

当游戏已经有一条可重复运行的正确路径后，就可以讨论“怎样更快”。性能优化不是把代码写得更复杂，也不是背一份技巧清单，而是通过测量找出当前最影响目标体验的限制，再用最小代价减少它。

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 区分 CPU 时间、GPU 时间和等待时间，并说明先量后改的理由。
2. 用 CPU 计时和 GPU Timer Query 取得一帧各阶段的耗时。
3. 判断当前瓶颈在 CPU 提交、顶点处理、片段处理还是带宽。
4. 解释 Overdraw、状态切换和 Draw Call 数量各自怎样拖慢一帧。
5. 用一次可复现的实验验证一项优化是否真的有效。
6. 说明 LOD、批合并和减少分辨率分别适合解决哪一类瓶颈。
7. 把测量数据和结论记录成能复查的证据，而不是凭感觉调参数。

**Profiler（性能分析器）**是记录程序各部分时间、调用关系、资源和硬件活动的工具。**Timeline（时间线）**按时间横向显示 CPU、GPU 或线程何时工作与等待。本章既会使用程序内计时器，也会使用图形调试/平台 Profiler。

本章遵循一条固定路线：先定义目标，再可靠测量，然后用实验定位瓶颈，最后只保留有证据的优化。

第一次学习只需完成一条完整证据链：选一个固定场景，记录基线，判断主要时间花在 CPU 还是 GPU，再选择一项对应改动并复测。第 19.18～19.25 节是“诊断后可选择的工具箱”，不是每个项目都必须依次套用的优化清单。

## 第一部分：目标与性能语言

### 19.1 性能不是“越快越好”，而是满足预算

项目首先要定目标。例如：

~~~text
目标平台：Apple Silicon Mac
目标分辨率：1920 x 1080
目标帧率：60 FPS
目标场景：1000 个实例、一个方向光、一层 Shadow Map、Bloom
~~~

目标 60 FPS 意味着每帧只有约：

~~~text
1000 ms / 60 = 16.67 ms
~~~

这叫 **Frame Budget（帧预算）**。

不同目标：

| 目标 FPS | 每帧预算 |
| ---: | ---: |
| 30 | 33.33 ms |
| 60 | 16.67 ms |
| 90 | 11.11 ms |
| 120 | 8.33 ms |
| 144 | 6.94 ms |

没有目标平台、场景和画质条件，“性能很好”没有可验证含义。

---

### 19.2 为什么用毫秒比只看 FPS 更清楚

FPS 是帧时间的倒数，不是线性刻度。

例一：

~~~text
100 FPS -> 10.00 ms
110 FPS ->  9.09 ms
只节省约 0.91 ms
~~~

例二：

~~~text
30 FPS -> 33.33 ms
40 FPS -> 25.00 ms
节省约 8.33 ms
~~~

两个例子都“增加 10 FPS”，实际收益差近九倍。优化报告应记录毫秒，再把 FPS 当作体验层的辅助表达。

---

### 19.3 一帧不是 CPU 做完后 GPU 才从头开始

CPU 负责输入、游戏逻辑、裁剪、排序和提交 GPU 命令。GPU 负责执行此前提交的 Vertex、Rasterization、Fragment 和 Pass。

它们通常像流水线一样重叠：

~~~text
时间 ----->

CPU: [准备 Frame N] [准备 Frame N+1] [准备 Frame N+2]
GPU:       [执行 Frame N] [执行 Frame N+1] [执行 Frame N+2]
~~~

因此一帧“CPU 10 ms，GPU 12 ms”通常不等于 `22 ms`。若二者能充分重叠，吞吐更接近较慢的一端，但 Present、资源依赖、队列深度和 VSync 会影响实际结果。

把 CPU 和 GPU 当成两个同时工作的工人，比“CPU 把图交给 GPU 后原地等完”更接近现实。

---

### 19.4 Draw Call 是提交命令，不是同步执行函数

调用：

~~~cpp
glDrawElements(...);
~~~

通常表示 CPU 把绘制命令放进驱动/命令队列。函数返回时，GPU 很可能还没完成这次 Draw，甚至尚未开始。

所以这样计时：

~~~cpp
auto begin = Clock::now();
glDrawElements(...);
auto end = Clock::now();
~~~

测到的是 CPU 提交该调用所花时间，不是 GPU 画 Triangle 和 Fragment 的完整时间。

GPU 时间要用 GPU Timer Query 或平台 Profiler；结果通常在之后的帧才能读取，读取太早可能迫使 CPU 等 GPU，反而改变被测对象。

---

### 19.5 CPU-bound、GPU-bound 与等待

#### CPU-bound（CPU 受限）

CPU 准备一帧太慢，GPU 有时在等命令。可能原因：

- 太多 Draw Call 和状态设置。
- 游戏逻辑或 Physics 太重。
- 场景遍历、裁剪、排序低效。
- 每帧大量内存分配。
- 资源解析或日志写入发生在主循环。

#### GPU-bound（GPU 受限）

GPU 执行命令太慢，CPU 可能最终在 Present 或队列限制处等待。可能原因：

- 分辨率高、Fragment 太多。
- Shader 计算或 Texture 采样昂贵。
- Shadow、透明和后处理 Pass 太多。
- Overdraw 严重。
- 几何量过大或 Vertex Shader 复杂。
- 显存带宽不足。

#### Synchronization-bound（同步等待受限）

CPU/GPU 本来可以并行，却因读取结果、资源锁或过度同步互相等待。例如每帧立刻读取 GPU Query、`glFinish`、同步截图。

#### I/O-bound

程序等待磁盘、网络或资源解码。常表现为偶发卡顿，而不是每帧稳定变慢。

“受限”只表示当前条件下谁决定完成时间。改变分辨率或场景后，瓶颈可能移动。

---

### 19.6 Throughput、Latency 和 Stutter

- **Throughput（吞吐）**：一段时间能完成多少帧，常由平均 FPS 表示。
- **Latency（延迟）**：输入发生到对应画面显示之间经过多久。
- **Stutter（卡顿）**：少数帧异常变慢，导致运动不均匀。

平均 60 FPS 不代表流畅：

~~~text
多数帧 10 ms
每秒有一帧 100 ms
平均值可能仍不差，但玩家会明显看到顿挫
~~~

因此要查看帧时间分布，而不只看平均 FPS。

---

### 19.7 Average、Median、P95 和 P99

收集很多帧时间后：

- **Average（平均值）**：总时间除以帧数，容易被极端值影响。
- **Median / P50（中位数）**：一半帧比它快，一半比它慢。
- **P95**：95% 的帧不超过这个时间，最慢 5% 更慢。
- **P99**：99% 的帧不超过这个时间，用于观察尾部卡顿。
- **Max（最大值）**：最慢单帧，可能受断点、系统调度等偶发因素影响。

例：

~~~text
Average: 12 ms
Median:  10 ms
P95:     18 ms
P99:     45 ms
Max:    180 ms
~~~

这说明大部分帧很快，但尾部存在明显卡顿。只报 Average 会隐藏问题。

---

### 19.8 建立稳定测试场景

测量前固定变量：

- 相同机器、电源模式和温度状态。
- 相同分辨率、VSync 和画质设置。
- 相同场景、相机路径和对象数量。
- 相同构建类型。不要把未优化 Debug 与 Release 直接比较。
- 避开 Shader 首次编译、资源首次加载等 Warm-up 阶段，或单独记录它们。**Warm-up（预热）**是先运行一段不计入稳定结果的时间，让一次性初始化完成。
- 测量足够多帧，例如 10 到 30 秒。

若每次相机看向不同地方，Fragment 数、阴影可见对象和透明 Overdraw 都会变化，比较不再公平。

可以录制一段相机路径和输入回放，让测试可重复。

---

#### 本部分检查点

**能做出来**：建立一个可重复的测试场景：同样的相机路径、同样的对象数量，跑两次得到接近的数字。

**能回答**：

- 为什么必须先有稳定场景才能谈优化？
- CPU 时间、GPU 时间和等待时间怎么区分？
- 为什么不能用“感觉流畅”当结论？

**卡在这里就先不要往下读**：两次测量差很多，说明场景不受控（有随机、有异步加载）。先把它固定下来。

## 第二部分：测量并定位瓶颈

### 19.9 CPU 计时器

最简单的范围计时器：

~~~cpp
class ScopedCpuTimer {
public:
    ScopedCpuTimer(const char* name, Profiler& profiler)
        : name_(name), profiler_(profiler), begin_(Clock::now()) {}

    ~ScopedCpuTimer() {
        double ms = milliseconds(Clock::now() - begin_);
        profiler_.recordCpu(name_, ms);
    }

private:
    const char* name_;
    Profiler& profiler_;
    Clock::time_point begin_;
};

void buildSnapshot() {
    ScopedCpuTimer timer("BuildSnapshot", profiler);
    // 函数结束时析构，自动记录耗时
}
~~~

计时器本身也有成本。不要给每个极小函数都加高频日志；先按系统和 Pass 分大块，再深入最慢区域。

每帧建议记录：

~~~text
Frame CPU
├── Poll/Input
├── Fixed Update
│   ├── Gameplay
│   └── Physics
├── Build Snapshot
├── Culling/Sorting
├── Render Submission
└── UI
~~~

嵌套计时要区分 Inclusive Time（包含子范围）和 Exclusive Time（不含子范围）。

---

### 19.10 GPU Timer Query

OpenGL 可用 Query 记录一段 GPU 时间。概念流程：

~~~cpp
glBeginQuery(GL_TIME_ELAPSED, opaqueQuery);
drawOpaquePass();
glEndQuery(GL_TIME_ELAPSED);

// 之后的帧确认结果可用，再读取；不要立刻强制等待
GLuint64 nanoseconds = 0;
glGetQueryObjectui64v(oldQuery, GL_QUERY_RESULT, &nanoseconds);
double milliseconds = nanoseconds / 1'000'000.0;
~~~

真实实现通常为每个 Frame in Flight 准备一组 Query，延迟几帧读取。需要检查 `GL_QUERY_RESULT_AVAILABLE`，避免无意同步。

建议按 Pass 测：

~~~text
Shadow GPU ms
Opaque GPU ms
Transparent GPU ms
Bloom GPU ms
Tone Map GPU ms
UI GPU ms
~~~

RenderDoc 的单帧时序和平台 GPU Profiler 也能提供更深入信息。Timer Query 适合长期显示在调试面板中。

---

### 19.11 规模指标不是耗时，但能解释耗时

应同时记录工作规模：

- Draw Call 数。
- Render Item 数。
- 提交和最终可见 Triangle 数。
- Fragment/Pixel 规模的近似信息。
- Shadow Caster 数。
- Transparent Item 和粒子数。
- Texture、Buffer 与 Render Target 内存。
- 每帧上传字节数。
- Shader/Material/Pipeline 切换数。

“Draw Call 变少”不等于“一定更快”；但若 CPU Render Submission 同时下降，因果解释就更有力。

---

### 19.12 Pixel、Fragment、Overdraw 和 Fill Rate

屏幕 `1920 x 1080` 约有 207 万 Pixel。一个全屏 Pass 理论上至少要处理约 207 万个 Fragment；4K 是约 829 万，接近四倍。

**Overdraw（过度绘制）**表示同一个 Pixel 位置产生多个 Fragment，最后只有部分结果可见。

~~~text
相机 -> 透明烟雾层 1 -> 烟雾层 2 -> 草叶 -> 墙 -> 最终 Pixel
       同一位置可能运行多次 Fragment Shader
~~~

**Fill Rate（填充率）**描述 GPU 处理和写入 Pixel/Fragment 的能力。复杂 Fragment Shader、高分辨率、多层透明和多个全屏 Pass 都会增加压力。

不透明物体配合深度测试可拒绝被遮挡 Fragment，但是否能“早期拒绝”还受 Shader 和状态影响。透明物体通常必须保留后方颜色，难以用普通深度写入消除 Overdraw。

---

### 19.13 顶点与几何成本

每个 Draw 的 Vertex Shader 会处理提交的 Vertex。影响包括：

- Mesh Vertex 数。
- Shadow Pass 中重复绘制几何。
- Skinning、Morph 等 Vertex Shader 工作。
- 相同 Mesh 是否重复提交。
- Vertex Buffer 布局和带宽。

Triangle 很小、远小于一个 Pixel 时，Rasterization 效率也可能下降。这叫 Micro-triangle 问题。LOD 可以避免远处模型仍保留大量不可见细节。

仅看 Triangle 数不足以判断 GPU 时间：100 万个简单大三角和 100 万个微小、重叠、复杂着色三角成本不同。

---

### 19.14 带宽、显存和资源上传

GPU 不只计算，也需要搬运数据。

- **Memory Bandwidth（内存带宽）**：单位时间能读写多少数据。
- **VRAM / GPU Memory（显存/GPU 内存）**：保存 Texture、Buffer、Render Target 的空间；统一内存架构上物理内存可能共享，但访问模式仍有成本。
- **Upload（上传）**：CPU 数据传到 GPU 可用资源。
- **Readback（回读）**：GPU 数据读回 CPU，通常更容易造成等待。

一个 `1920 x 1080` 的 RGBA16F Texture 每 Pixel 8 字节，单层约：

~~~text
1920 * 1080 * 8 ≈ 16.6 MB
~~~

若 Bloom 链、Scene Color、历史缓冲各有多张，内存和每帧带宽会快速增加。

常见问题：

- 每帧重新上传不变 Mesh。
- 每帧创建/删除 Texture 和 Buffer。
- 使用远大于实际需要的 Render Target 格式。
- 没有 Mipmap，远处 Texture 采样既闪烁又低效。
- 为调试每帧同步读取大 Texture 回 CPU。

---

### 19.15 内存分配和数据布局

CPU 端每帧大量 `new/delete`、字符串拼接和 `vector` 反复扩容会导致耗时波动。

第一步通常不是建立复杂对象池，而是：

- `reserve` 预计数量。
- 重用每帧临时容器。
- 避免热循环中的格式化日志。
- 让 Render Item 连续存放，便于缓存访问。
- 不在每个对象 Draw 时临时查找磁盘路径。

**Cache Locality（缓存局部性）**的直觉是：CPU 读取连续、紧邻的数据通常比在许多随机指针之间跳转更快。

先用 Profiler 证明分配或内存访问是问题，再引入专用分配器。

---

### 19.16 优化实验的科学流程

每次优化按以下步骤：

1. 写出目标，例如“1080p 下 P95 小于 16.67 ms”。
2. 固定测试场景和条件。
3. 记录基线：CPU/GPU 分段、规模指标、截图。
4. 提出一个假设，例如“1000 次 Draw 使 CPU submission 过慢”。
5. 只改变一个主要变量，例如改为 Instancing。
6. 重新 Warm-up，并多次测量相同区间。
7. 比较毫秒和画面结果。
8. 解释收益来自哪里、代价是什么。
9. 收益不稳定或代价不可接受时撤销。

优化结论应写成：

~~~text
在 M2、1080p、固定相机、1000 个相同 Mesh 的场景中，
将逐对象 Draw 改为 Instanced Draw 后：
Draw Call 1000 -> 1，CPU Render Submission 5.2 ms -> 0.8 ms，
GPU 4.1 ms -> 3.9 ms，画面一致。
结论：该场景主要减少了 CPU 提交成本。
~~~

而不是只写“Instancing 性能更好”。

---

### 19.17 快速判断瓶颈的实验

下面是定位方向的实验，不是最终证明。

#### 大幅降低渲染分辨率

若 GPU 时间明显下降，说明 Fragment、带宽或全屏 Pass 可能重要。CPU 逻辑通常不会因分辨率下降而大幅变快。

#### 关闭某个 Pass

逐个关闭 Shadow、Transparent、Bloom、UI，观察 GPU Timer。一次只关一个。

#### 减少 Draw Call，但保持可见几何接近

例如使用 Instancing。若 CPU submission 明显下降，说明提交成本重要。

#### 减少 Triangle，但保持分辨率

替换低模或 LOD。若 GPU Vertex/Geometry 时间下降，几何成本重要。

#### 暂停游戏逻辑但继续渲染

若 CPU Frame 大幅下降，Physics/Gameplay 需要深入分析。

#### 关闭 VSync

测极限吞吐时通常需要关闭，否则帧率会被刷新率封顶。但最终体验测试仍要使用目标 VSync/呈现模式。

---

#### 本部分检查点

**能做出来**：拿到一帧内各阶段的 CPU 和 GPU 耗时，并能指出当前瓶颈在哪一类。

**能回答**：

- 降低分辨率后帧率大幅提升说明什么？
- 减少对象数量后没变化说明什么？
- GPU Timer Query 测的是哪一段？

**卡在这里就先不要往下读**：不知道瓶颈在哪就去优化，等于随机改代码。19.17 的几个实验必须先做。

## 第三部分：针对已知瓶颈优化

只有第二部分的测量支持某个假设时，才进入对应小节：CPU 提交慢才看排序和 Instancing，几何阶段慢才看 LOD，Fragment 或带宽慢才看分辨率、Overdraw 和后处理。若当前项目没有某类瓶颈，跳过对应优化才是正确做法。

### 19.18 减少不可见工作：Culling

最便宜的工作是根本不做。

可见性层级：

1. Layer/状态：已禁用对象不提交。
2. Frustum Culling：Bounds 不在相机视锥内则不画。
3. Distance Culling：超过设计距离的细小对象不画。
4. Occlusion Culling：被其他物体完全遮挡的对象不画，复杂度更高。

Shadow Pass 使用光源视锥，不应简单复用主相机可见列表：相机外对象仍可能向画面内投影阴影。

记录“总对象、主相机可见、Shadow Caster、被裁剪”数量，确认裁剪收益与正确性。

---

### 19.19 减少提交开销：排序、批处理和 Instancing

#### Material/Pipeline 排序

不透明对象可按 Pipeline、Material、Mesh 排序，减少 Shader、Texture 和状态切换。透明对象则首先受正确的远到近排序约束，不能只按材质合并。

#### Instancing

许多对象共享 Mesh/Material，只提供不同 Transform 和少量实例参数，一次 Draw 绘制多个实例。

~~~text
非 Instancing：1000 个对象 -> 1000 次 Draw
Instancing：   1000 个对象 -> 1 次或少量 Draw + 1000 份实例数据
~~~

Instancing 主要减少 CPU 提交和状态重复，不会让 GPU 少画 1000 个实例的 Triangle 和 Pixel。

#### Texture Atlas / Array

**Texture Atlas（纹理图集）**把许多小图排进一张大图，通过不同 UV 区域选择；**Texture Array（纹理数组）**保存尺寸/格式一致的多层图片，通过层索引选择。二者都可减少资源绑定切换，但要处理 UV、Mipmap 边缘、格式一致性和更新策略。

不要为了减少 Draw Call 把整个关卡合并成一个永远不可裁剪的巨型 Mesh。优化一个指标可能损害另一个指标。

---

### 19.20 降低几何成本：LOD

**LOD（Level of Detail，细节层级）**根据屏幕大小或距离选择不同复杂度 Mesh。

~~~text
近处：LOD0，20,000 Triangle
中距：LOD1， 5,000 Triangle
远处：LOD2，   500 Triangle
极远：不绘制或 Billboard
~~~

**Billboard（公告板）**是始终朝向相机的简单平面，用一张图片近似远处复杂物体，例如树或云。

距离只是近似，更合理的依据是物体在屏幕上占多大。一个巨型建筑即使很远仍可能很大。

LOD 切换可能突然“跳变”（Popping）。可通过合理阈值、过渡淡化或更相似的轮廓减轻。

Shadow Mesh 也可以更简单，因为阴影通常不需要所有小细节。

---

### 19.21 降低 Fragment 与带宽成本

常见手段：

- 降低内部 Render Scale，再放大到窗口。
- Bloom、Blur 等在半分辨率或四分之一分辨率执行。
- 减少透明层重叠和大面积透明粒子。
- 不透明物体合理使用 **Depth Prepass（深度预处理）**：先只画深度，再在颜色 Pass 提前拒绝被挡 Fragment；前提是测量证明收益大于额外几何 Draw。
- 简化最慢的 Fragment Shader 或减少 Texture 采样。
- 降低 Shadow Map 分辨率、级联数量或更新频率。
- 使用满足精度的更小 Render Target 格式。
- 合并能安全合并的全屏 Pass，减少中间读写。

每种手段都有画质或复杂度代价。例如 Render Scale 会降低清晰度；Shadow 降分辨率会增加锯齿；合并 Pass 会降低模块独立性。

---

### 19.22 Dynamic Resolution 的边界

**Dynamic Resolution（动态分辨率）**根据 GPU 压力调整内部渲染尺寸，以维持目标帧时间。

它只主要减少与 Pixel 数相关的 GPU 工作，不会解决：

- CPU Gameplay 太慢。
- Draw Call 提交太多。
- Physics 太慢。
- 大量同步等待。

调整还需要**迟滞（Hysteresis）**和上下限：只有超出较宽阈值才切换，避免每帧在两个分辨率间来回跳。第一版先提供固定 Render Scale 滑块，理解效果后再自动控制。

---

### 19.23 Shader 优化不要靠猜

看到除法、分支或函数调用，不代表它一定是瓶颈。现代 GPU 和编译器行为复杂。

可靠流程：

1. GPU Profiler 指出某个 Pass 显著耗时。
2. 降低该 Pass 分辨率或用简单 Shader 替换，验证 Shader/Fragment 成本。
3. 检查采样次数、带宽、分支一致性和寄存器压力等工具信息。**寄存器压力**表示每个并行 Shader Invocation 需要保存多少临时值；过多时可能减少 GPU 同时容纳的工作或把数据溢出到更慢存储。
4. 一次改变一个部分并重新测量。

可先做“替代 Shader 实验”：只输出常量颜色。如果 GPU 时间几乎不变，问题可能不在光照公式，而在 Overdraw、带宽或其他阶段。

---

### 19.24 避免无意同步

以下操作可能让 CPU 等 GPU：

- `glFinish()`。
- 立即读取刚提交的 Query 结果。
- 读取当前 Render Target Pixel 到 CPU。
- 映射仍被 GPU 使用的 Buffer，未采用合适策略。
- 每帧创建、修改并马上重用同一 GPU Resource。

同步有时是正确性所需，但应知道它在哪里、为何存在。截图、Picking Readback 等可以延迟读取，而不是强迫当前帧停住。

OpenGL 驱动可能隐藏等待位置，因此结合 CPU Timeline、GPU Timeline 和实验判断。

---

### 19.25 Shader 编译、资源加载和首次使用卡顿

有些 Stutter 不是稳定每帧成本，而是第一次发生：

- 首次编译/链接 Shader。
- 首次创建 Pipeline 或驱动内部变体。
- 首次加载/解码 Texture。
- `vector` 首次扩容。
- 第一次遇到新音效或字体 Glyph。

解决思路：

- 在 Loading 阶段预加载关键资源。
- 预热将要使用的 Shader/Material 组合。
- 复用容器并合理 reserve。
- 将不可避免的加载放在可接受界面，并显示进度。

不要用平均 1000 帧把一次 200 ms 卡顿“稀释掉”；P99 和帧时间曲线更能暴露它。

---

### 19.26 Amdahl 定律的直觉

如果某个模块只占一帧的 10%，即使把它优化到耗时为 0，整帧最多也只快 10%。

例：

~~~text
原帧时间 20 ms
UI 只占 1 ms
即使 UI 完全免费，仍为 19 ms
~~~

因此应优先处理占比大、可改善且不破坏质量的区域。不要因为某段代码容易改，就默认它值得优先优化。

---

#### 本部分检查点

**能做出来**：完成一项优化，并用第一部分的测试场景给出“改前/改后”的具体数字。

**能回答**：

- 这项优化针对的是哪一类瓶颈？
- 它带来了什么代价（内存、画质、复杂度）？
- Amdahl 定律告诉你什么时候不该继续优化这一处？

**卡在这里就先不要往下读**：说不出改前改后的数字，这项优化就不算完成。

## 第四部分：记录结论并验收

### 19.27 完整实验：1000 个相同模型

#### 基线

固定相机看到 1000 个相同 Mesh；关闭 VSync；预热 5 秒；记录之后 20 秒。

需要记录：

~~~text
CPU Frame Average/P95
CPU Culling
CPU Submission
GPU Opaque
Draw Call
Triangle
Framebuffer size
截图
~~~

#### 假设

“CPU Submission 的 6 ms 主要来自 1000 次相似 Draw 和状态设置。”

#### 改动

把对象 Transform 写入实例 Buffer，使用一次 `glDrawElementsInstanced`。

#### 结果解释

若 Draw Call 从 1000 降到 1，CPU Submission 从 6 ms 降到 1 ms，而 GPU Opaque 基本不变，假设得到支持。

若 GPU 时间仍很高，Instancing 没有减少实际 Triangle 和 Fragment；还需要 LOD、裁剪或降低 Pixel 成本。

若画面少了对象，要先修正确性，不能把“没有画完”当成性能收益。

---

### 19.28 性能报告模板

~~~markdown
## 目标
平台、分辨率、目标 FPS、测试场景。

## 基线
构建版本、运行设置、CPU/GPU 分段、P50/P95/P99、规模指标。

## 假设
哪类工作是瓶颈？依据是什么？

## 单一改动
具体改了什么，其他条件怎样保持一致？

## 结果
优化前后表格、帧时间图、同机位截图。

## 解释
收益来自 CPU、GPU、带宽、内存还是 I/O？

## 代价与风险
画质、内存、代码复杂度、加载时间有什么变化？

## 决策
保留、继续实验，还是撤销？
~~~

---

### 19.29 常见误解

#### 误解一：Draw Call 越少一定越快

Draw Call 是一个重要规模指标，但合并可能破坏裁剪，GPU 工作甚至增加。必须测时间。

#### 误解二：Triangle 越少一定越快

如果瓶颈是全屏 Bloom 或透明 Overdraw，减少少量 Triangle 几乎无效。

#### 误解三：CPU 时间加 GPU 时间就是帧时间

二者通常重叠，且还受 Present 与同步影响。应看 Timeline 和最终 Frame Time。

#### 误解四：平均 60 FPS 就是稳定 60 FPS

P95/P99 和帧时间曲线可能显示严重 Stutter。

#### 误解五：Release 更快，所以只需最后再 Profile

最终判断应基于接近发布的构建，但开发期间也要持续测量。Debug 中发现的逻辑热点仍有价值，只是数字不能直接当发布结果。

#### 误解六：优化代码越复杂越专业

复杂度是成本。没有可测收益的复杂优化应撤销。

---

### 19.30 建议排查顺序

1. 确认测试条件、目标和构建类型。
2. 看整帧 CPU、GPU 和 Present/等待关系。
3. 查看 P50、P95、P99，区分稳定慢与偶发卡顿。
4. 按 CPU 系统和 GPU Pass 分段。
5. 记录 Draw、Triangle、对象、分辨率和内存等规模。
6. 用分辨率、Pass 开关、对象数等实验验证方向。
7. 提出一个假设，只做一个主要改动。
8. 同条件重复测量并检查截图。
9. 写下收益、代价和结论。

---

### 19.31 练习

#### 练习 A：换算帧预算

手算 24、30、60、90、120、144 FPS 对应的毫秒预算，并解释为什么增加相同 FPS 不代表相同收益。

#### 练习 B：帧时间统计

记录至少 2000 帧，显示 Average、Median、P95、P99 和 Max。故意每秒阻塞 50 ms，观察哪些指标最明显。

#### 练习 C：CPU/GPU 分段

为固定更新、Snapshot、Culling、Submission 和每个 GPU Pass 加计时。Query 结果延迟读取，不能每帧 `glFinish`。

#### 练习 D：分辨率实验

分别在 100%、75%、50% Render Scale 下记录 GPU Pass。判断哪些 Pass 随 Pixel 数明显变化。

#### 练习 E：Instancing

比较 1000 个相同对象逐个 Draw 和 Instanced Draw。记录 CPU/GPU 时间、Draw、Triangle 和截图。

#### 练习 F：Overdraw

叠加大面积透明四边形，逐步增加层数。观察 Transparent Pass GPU 时间，并制作简单 Overdraw 可视化。

#### 练习 G：LOD

为一个模型准备三档几何。记录相机拉远时 Triangle 和 GPU 时间，检查切换是否明显跳变。

#### 练习 H：优化报告

选择一个真实瓶颈，严格按第 19.28 节模板提交前后对比。报告中必须包含一次无效或被撤销的实验，说明为什么撤销。

#### 练习 I：故意出错实验（破坏测量本身）

这一章的“错误”不在渲染，而在**测量方法**。每条都先预测结论会怎样被带偏，再实际跑一次：

1. 在 Debug 构建下测量，然后和 Release 的数字放在一起比较。
2. 只测一帧就下结论。
3. 用 `printf` 打印每个物体的耗时，然后测总帧时间。
4. 测量时让相机自由移动，而不是走固定路线。
5. 先优化，再测量，然后用“感觉变快了”作为结论。
6. 用 CPU 时间去判断一个明显是片段瓶颈的问题。

每一条都要写出：**这个做法会让你得到什么错误结论？** 这一章真正要防的就是“量错了，然后自信地改错了地方”。

---

### 19.32 本章验收标准
进入第 20 章前，最低应完成一次可复现的性能实验：

- 性能目标包含平台、分辨率、画质、场景和 FPS。
- 调试 UI 至少显示 Frame Time、P95 和与假设有关的工作规模；P50、P99 和完整 Timeline 可继续完善。
- CPU 至少按主要系统分段；若假设涉及 GPU，GPU 按相关 Pass 使用非阻塞方式计时。
- 能通过 Timeline 或实验区分 CPU-bound、GPU-bound 和同步等待。
- 测试有固定相机/回放、Warm-up、构建类型和足够样本。
- 提出一个基于测量的瓶颈假设，并只改变一个主要变量。
- 该改动有前后毫秒对比、相同条件截图和代价说明；无收益时也如实记录并撤销。
- 没有把减少 Draw/Triangle 本身当成最终成功指标。

扩展练习可按当前瓶颈选择 Instancing、分辨率、Overdraw、LOD 或首次加载 Stutter 中的一项。没有证据时，不需要为了“完成优化章节”把它们全部加入项目。

你还应能用自己的话回答：

1. 为什么 CPU 包围 `glDrawElements` 的计时不是 GPU Draw 时间？
2. CPU 10 ms、GPU 12 ms 为什么通常不能直接说一帧是 22 ms？
3. P95 比 Average 多告诉了你什么？
4. 降低分辨率明显变快能支持什么假设，又不能证明什么？
5. Instancing 减少了什么，没有减少什么？
6. 为什么一次优化必须保存相同条件下的画面证据？

完成本章后，你已经能用数据解释 OpenGL Renderer。下一章会把这些明确的 Pass、Resource 和依赖映射到 Vulkan 或 Metal。
