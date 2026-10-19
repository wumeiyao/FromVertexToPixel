# 第 21 章：毕业项目、交付与技术报告

<!-- nav -->
> [上一章：从 OpenGL 过渡到 Vulkan 或 Metal](20-vulkan-bridge.md) ｜ 本书终点
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/10_capstone/21-capstone.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

毕业项目不是“再学一批新名词”，而是把前 20 章已经实现过的小系统连成一个完整、可重现、可解释的作品。

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 把前二十章的成果整合成一个能启动、能玩、能结束的单关卡。
2. 写出这一关的目标、失败条件和重开流程。
3. 按依赖顺序集成渲染、输入、物理、音频和 UI，并逐项验证。
4. 用调试面板和性能数据证明这一关在目标帧率下稳定运行。
5. 给项目写一份技术报告，说清每个系统为什么这样设计。
6. 指出自己实现中已知的限制，并说明如果继续做会先改哪一项。

一个 5 分钟能玩完的小关卡，如果从干净目录能构建、从开始界面能玩到结算、遇到错误有日志、每个 Render Pass 都能解释、性能结论有数据，就比一个无法完成的开放世界更能证明你真正理解了 3D 渲染游戏的基本结构。

本章不是新增 42 个知识点，而是一份项目执行手册：先限定范围和工程边界，再按阶段构建，随后测试和交付，最后总复习与验收。

本章给出的是完整项目地图，不是要求所有候选功能同时进入 Must。先区分两层交付：

- **核心毕业项目**：干净构建；Title 到 Finished 与 Restart；灰盒碰撞和收集；一个模型/纹理资产；方向光、Depth、Opaque 和硬阴影；最小 UI；一个短音效；错误日志；一次有基线的性能实验。
- **作品集增强项**：Transparent、Skybox、Bloom、Instancing、更完整调试面板、多个 Bug 调查和现代 API 原型。它们应根据题目和时间从 Should/Could 中选择。

核心层没有完成时，不要为了最终清单继续加入增强项。下面每个阶段都允许在标明的最低完成点停下。

## 第一部分：先限定要做什么

### 21.1 毕业项目的具体题目

建议项目名可以先叫 `RenderQuest`，内容是一个小型第三人称或俯视角收集关卡：

~~~text
开始画面
-> 玩家进入一个小型 3D 场景
-> 使用键盘移动，相机跟随或自由观察
-> 障碍物阻挡玩家
-> 收集 3 到 5 个发光物体
-> 每次收集有声音、UI 和画面反馈
-> 收集完成显示结算界面
-> 可以重新开始或退出
~~~

预计一次游玩 3 到 10 分钟。场景可以只有一间房、一座小岛或一条短走廊。

题材可以换，但必须保留完整循环：

~~~text
Title -> Playing -> Paused -> Finished -> Restart/Exit
~~~

---

### 21.2 项目目的不是做“自己的 Unity”

本项目要证明以下能力：

1. 能解释 3D Model 怎样成为 Pixel。
2. 能组织多 Pass Renderer。
3. 能把 Game、Physics、Audio、Asset 和 Renderer 分开。
4. 能从错误画面反向定位问题。
5. 能用性能数据做一次有效优化。
6. 能让另一个人在干净环境中构建和运行。

因此明确不做：

- 通用场景编辑器。
- 多人网络同步。
- 开放世界和无缝流式地图。
- 自研工业级 Physics。
- 动态全局光照（让间接反射光也随场景实时变化的复杂照明系统）。
- 完整骨骼动画编辑工具。
- 通用 ECS（Entity Component System，一种实体和组件数据组织模式）框架重写。
- 同时维护 OpenGL、Vulkan、Metal 三套完整后端。
- 账号、商城、在线服务。

这些都不是坏功能，只是不属于本次闭环。

---

### 21.3 什么叫 Scope（范围）

Scope 是项目承诺完成的功能边界。功能一旦写入“必须完成”，就会产生代码、资产、测试、调试和交付成本。

可以把需求分三层：

#### Must：必须有

没有它就不能证明课程目标，例如角色移动、碰撞、收集、结算、基本 Renderer。

#### Should：时间允许应有

能加强作品，但移除后仍是完整游戏，例如 Bloom、两种障碍、简单粒子。

#### Could：完成后再考虑

纯扩展，例如更多关卡、复杂动画、第二种相机。

任何新增功能必须先进入清单，而不是在写代码时顺手扩大范围。

---

### 21.4 一句话设计和成功条件

先写一句话：

> 玩家在一个小型 3D 场景中移动并收集 5 个能量体，避开或绕过障碍，全部收集后结算。

再写可观察的成功条件：

- 玩家能从 Title 点击/按键开始。
- 角色不能穿过墙和地面。
- 收集数量从 0 正确增加到目标值。
- 收集物消失且播放一次音效。
- 达到目标后进入 Finished，物理停止，结算 UI 可操作。
- Restart 恢复所有对象、分数和计时。
- 关闭窗口不会泄漏或在无 Context 时删除 GPU Resource。

不要用“系统基本正常”作为验收，它无法测试。

---

### 21.5 从用户体验反推系统

一个简单动作会跨过多个系统。以“收集能量体”为例：

~~~text
玩家输入
    |
    v
Character Controller
    |
    v
Physics Trigger
    |
    v
Game Collect Event ──────────┐
    |                        | 一次性音效
    v                        v
Score + Entity State     Audio One-shot
    |
    v
Render Snapshot
    |
    v
收集物消失 + UI 更新
~~~

这张图是系统设计，不是函数调用必须完全同名。它表达谁产生事实、谁消费结果。

每个玩法需求都做同样分析：

| 需求 | 输入/事实 | 改变的状态 | 可见/可听结果 |
| --- | --- | --- | --- |
| 移动 | PlayerIntent | Physics Transform | 角色和相机位置变化 |
| 暂停 | Pause Event | GameState | 物理停止、暂停 UI 出现 |
| 收集 | Trigger Enter | Score、Active | Mesh 消失、数字变化、声音 |
| 结算 | Score 达目标 | Finished | 结算 UI、计时停止 |
| 重开 | Restart Command | 重置 World | 所有对象回初始状态 |

---

### 21.6 最小模块边界

建议模块不是为了追求抽象，而是为了防止职责互相渗透：

~~~text
Application
├── Platform
│   ├── Window
│   ├── Input
│   └── Clock
├── Game
│   ├── GameState
│   ├── World/Entity
│   ├── PlayerController
│   └── CollectibleSystem
├── Physics
│   ├── PhysicsWorld
│   └── CharacterController
├── Render
│   ├── Renderer
│   ├── Passes
│   ├── RenderSnapshot
│   └── GpuResources
├── Audio
│   └── AudioSystem
├── Assets
│   └── AssetManager
└── Debug
    ├── DebugUi
    └── Profiler
~~~

允许小项目中多个类先放在同一个文件夹，但依赖方向要明确：

~~~text
Game -> Physics 接口、Audio 事件、Asset Handle
Game -> 构建 Render Snapshot
Renderer -> 只读 Snapshot 和 Render Resource
Renderer -X-> 不访问 PlayerController/PhysicsBody
Physics -X-> 不调用 OpenGL
Audio -X-> 不决定得分或销毁 Entity
~~~

`-X->` 表示禁止依赖。

---

### 21.7 推荐工程目录

~~~text
CodeA/
├── CMakeLists.txt
├── cmake/
├── src/
│   ├── main.cpp
│   ├── platform/
│   ├── game/
│   ├── physics/
│   ├── render/
│   ├── audio/
│   ├── assets/
│   └── debug/
├── shaders/
├── assets/
│   ├── models/
│   ├── textures/
│   ├── audio/
│   └── scenes/
├── tests/
├── captures/
│   ├── screenshots/
│   ├── gpu/              # 平台支持的 GPU Capture；当前 Mac 可为空
│   └── profiles/
├── docs/
│   ├── design.md
│   ├── frame-graph.md
│   └── technical-report.md
└── build/              # 生成目录，不提交手改内容
~~~

教材目录和实际程序可以暂时共存，但代码、运行资产、测试证据要有稳定位置。

不要依赖 VS Code 当前工作目录碰巧等于项目根目录。程序应从可执行文件位置、构建配置或安装布局可靠找到 Asset。

---

### 21.8 构建系统的最低要求

根 CMake 应明确：

- 项目名和 C++ 标准。
- 主可执行目标。
- 源文件与 Include Directory。
- GLFW、GLM、OpenGL、ImGui、物理和音频依赖。
- Shader/Asset 如何复制到运行目录。
- Debug/Release 的合理编译选项。

示意结构：

~~~cmake
cmake_minimum_required(VERSION 3.20)
project(RenderQuest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(RenderQuest
    src/main.cpp
    src/game/Game.cpp
    src/render/Renderer.cpp
    # 其余源码显式列出或按项目约定组织
)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

target_link_libraries(RenderQuest PRIVATE glfw OpenGL::GL)

add_custom_command(TARGET RenderQuest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:RenderQuest>/assets
)
~~~

这只是示意，依赖目标名按实际安装方式调整。关键是：从空 `build/` 开始，Configure、Build、Run 的步骤有文档且能复现。

---

### 21.9 Definition of Done：每项任务怎样才算完成

任务“实现 Shadow”不能在代码能编译时就标完成。建议 Definition of Done（完成定义）：

1. 功能在正常路径可见。
2. 至少一个错误/边界情况有处理。
3. 调试工具能观察关键中间数据。
4. 有截图、测试或日志作为证据。
5. 没有新增高严重度 OpenGL/Validation 错误。
6. 文档更新了状态和已知限制。
7. 从干净构建仍能运行。

示例：Shadow Map 完成需要：

~~~text
[x] 角色和场景产生方向光阴影
[x] Shadow Depth 可在 Debug UI 显示
[x] Bias 可调，已记录 Acne/Peter-panning 取舍
[x] 相机移动后阴影稳定在设计范围
[x] GPU 截帧工具能指出 Shadow Pass 输出和 Main Pass 输入；或在当前 Mac 上用中间 Texture、Pass 日志和资源标签给出等价证据
[x] GPU Pass 时间已记录
~~~

---

### 21.10 先做 Vertical Slice

**Vertical Slice（垂直切片）**是一条很窄、但从输入到最终反馈完整走通的功能。

第一个切片只需：

~~~text
灰色地面
-> 一个胶囊或立方体玩家
-> WASD 输入
-> Physics 移动和墙体碰撞
-> Camera
-> 一个收集 Trigger
-> UI 由 0 变 1
-> 播放一个音效
~~~

它可以很丑，但必须从 Input 走到 Physics、Game、Snapshot、Renderer、UI 和 Audio。

先完成垂直切片，可以尽早暴露系统边界问题。若先分别做“完整 Renderer”“完整资产系统”“完整 UI”，最后集成时风险会集中爆发。

---

#### 本部分检查点

**能做出来**：一页项目定义：这一关的目标、失败条件、重开方式，以及**明确不做**的功能清单。

**能回答**：

- 什么是 Vertical Slice，为什么先做它？
- 哪些功能是“做完才算这一关”，哪些是加分项？
- 你打算用多少时间完成第一个可玩版本？

**卡在这里就先不要往下读**：范围没锁定就开始写代码，是毕业项目最常见的失败原因。

## 第二部分：按阶段完成可玩的项目

### 21.11 阶段 0：基线和设计文档

目标：在加功能前固定范围、平台和证据格式。

任务：

- 写一句话设计和 Must/Should/Could。
- 画主循环和系统依赖图。
- 写坐标、单位、颜色空间和 Asset 路径约定。
- 定目标平台、分辨率和 FPS。
- 建立 Debug/Release 构建。
- 创建截图、Profile、Bug 记录目录。

验收：另一位读者能在 5 分钟内说清游戏目标和技术边界。

---

### 21.12 阶段 1：窗口、循环和灰盒场景

**Graybox（灰盒）**是用简单方块、平面和纯色快速验证空间与玩法的关卡。

任务：

- 创建窗口和 OpenGL Context。
- Game Loop、固定 60 Hz、Accumulator、插值。
- 自由相机或跟随相机。
- 灰色地面、墙和一个玩家代理 Mesh。
- Shader 编译/链接日志、Pass 日志和基本 ImGui。
- 当前 Context 支持时，再启用 OpenGL Debug Output。

验收：

- 30/60/120 FPS 下移动结果一致。
- resize 和 Retina Framebuffer 正常。
- 暂停、单步和退出无异常。

此阶段不要导入复杂美术资源。

---

### 21.13 阶段 2：Physics 与最小玩法切片

任务：

- Static Ground/Wall。
- Character Capsule 与控制器。
- Collectible Trigger。
- Score 与 GameState。
- Title、Playing、Paused、Finished。
- 一个预加载音效。

验收：

- 玩家不能穿墙或掉出地面。
- 每个收集物只计分一次。
- 暂停不推进 Physics。
- Restart 后 Body、Trigger、Score、Timer 全部重置。
- Physics Shape 可视化与 Mesh 对齐。

---

### 21.14 阶段 3：Asset 和模型场景

任务：

- glTF Model、Mesh、Material、Texture 加载。
- Mipmap、Sampler 和 sRGB/Linear 格式正确。
- Asset Cache 与 Handle。
- 多实例共享资源。
- 缺失资源 Placeholder 和错误日志。

验收：

- 相同 Asset 只上传一次。
- 模型尺度、方向和 Pivot 符合项目约定。
- 故意删一个 Texture 时，程序仍运行并明确显示错误。
- 程序退出释放顺序正确。

只替换 Graybox 中最重要的对象，不必制作大量美术资产。

---

### 21.15 阶段 4：基础光照和颜色正确性

任务：

- 方向光。
- Base Color、Normal 和基础材质参数。
- Linear Lighting。
- sRGB Texture 解码和最终显示编码。

验收：

- Base Color 不会被重复 sRGB 解码。
- Normal 方向可视化正确。
- 光源方向和强度在 Debug UI 可调。
- 与固定参考截图比较，颜色变化能解释。

先保证颜色链正确，再追求“更鲜艳”。Exposure 和 Tone Mapping 需要第 6 阶段的 HDR 离屏目标，到那里再加入。

---

### 21.16 阶段 5：Pass、阴影与队列

核心任务：

- Shadow Pass。
- Opaque Queue。
- 明确清屏背景。
- UI Pass。
- 每个 Pass 的输入输出和状态表。

增强任务：场景确实使用半透明材质时加入 Transparent Queue；需要环境背景时再加入 Skybox。

验收：

- Shadow Depth 可单独显示。
- Opaque 正确写 Depth。
- UI 在场景和后处理之后。
- 支持平台上的 GPU Capture Event Group 与设计的 Pass 对应；当前 Mac 则用 Pass 日志与 Debug UI 对应。

若实现 Transparent，再单独验收它保持 Depth Test、关闭 Depth Write 并按远到近排序。

在 `frame-graph.md` 记录：

~~~text
Shadow Depth -> Main
Main Scene Color/Depth -> Transparent/Post
Scene Color -> Bloom/Tone Map
Final Color -> UI -> Present
~~~

---

### 21.17 阶段 6：离屏渲染和后处理

本阶段是推荐增强，不阻塞核心游戏闭环。任务按顺序为：

- Scene Color/Depth FBO。
- Fullscreen Triangle。
- Tone Mapping。
- 可选 Bloom 或另一个明确可解释的后处理。
- Render Scale 和 resize 资源重建。

验收：

- 可直接显示 Scene Color，证明离屏链正确。
- Tone Mapping 开关有可预测差异。
- resize 后 FBO 完整、无旧尺寸拉伸。
- UI 不被 Bloom 或 Tone Mapping 意外处理。

若实现 Bloom，再要求每个中间 Texture 可查看；未实现时不创建空 Bloom Pass。

如果 Bloom 仍不稳定，它属于 Should，可保留简单灰度/色调映射，不得拖垮完整交付。

---

### 21.18 阶段 7：调试、错误处理和回归证据

调试面板最低包含能支撑当前项目排错的内容：

- CPU Frame Time、P95、Draw 和 Triangle。
- Camera、Light、Framebuffer 参数。
- Shadow Depth；实现离屏后再显示 Scene Color/Bloom。
- Physics Shape 和 Contact 开关。
- Game State、Fixed Step、Accumulator。

Active Audio Source、Loaded Asset、内存估计、P50/P99 和更完整的中间资源浏览器属于增强项。指标不是越多越好；每一项都应能回答一个实际问题。

故障测试：

- Shader 编译失败。
- Asset 路径错误。
- FBO 不完整。
- 窗口 resize/minimize。
- 音频设备不可用。
- 玩家落出关卡。

系统应给出可理解错误或恢复策略，而不是静默黑屏。

---

### 21.19 阶段 8：Profile 和一次有效优化

先定义固定测试：

~~~text
平台、Release、1080p、固定相机路径、VSync off（吞吐测试）、运行 20 秒
~~~

记录基线后，选择一个有证据的瓶颈。候选：

- 多个相同收集物使用 Instancing。
- 视锥外对象裁剪。
- Bloom 降为半分辨率。
- 重用每帧 Render Item 容器。
- 减少不必要状态切换。

必须保留：

- 优化前后相同机位截图。
- CPU/GPU ms 和至少 P95；需要专门分析偶发卡顿时再加入 P99。
- Draw/Triangle/分辨率等解释指标。
- 画质、内存或复杂度代价。
- 一项无效实验及撤销理由。

“画少了所以快了”如果破坏正确性，不算优化。

---

### 21.20 阶段 9：内容锁定和只修 Bug

**Content Lock / Feature Freeze（功能冻结）**表示不再增加玩法和画面功能，只处理：

- 崩溃。
- 数据丢失或无法结算。
- 明显画面错误。
- 构建和路径问题。
- 严重性能回退。
- 文档和交付缺失。

没有功能冻结，项目会在最后一天因新功能破坏已有闭环。

建议给最后 20% 到 30% 时间用于集成、测试和报告，而不是全部排满开发任务。

---

#### 本部分检查点

**能做出来**：一个能启动、能操作、能判定胜负、能重开的可玩版本，哪怕画面很朴素。

**能回答**：

- 集成顺序是怎么定的，为什么不能先做画面？
- 每个阶段你用什么现象验证它完成了？
- 出现回归时你怎么定位是哪个系统引入的？

**卡在这里就先不要往下读**：先让它“能玩”，再让它“好看”。顺序反过来会让调试变成大海捞针。

## 第三部分：测试、调试与交付证据

### 21.21 每周/阶段任务怎样写

一个可执行任务必须包含：

~~~text
任务：实现 Shadow Map
依赖：第 12、13 章
可见结果：角色在地面留下方向光阴影
实现边界：单方向光、单 Shadow Map，不做级联
调试证据：Shadow Depth 缩略图 + GPU 截帧或当前 Mac 的等价 Pass/资源日志
验收：相机移动后阴影正确；Bias 可调；无 FBO 错误
回退方案：若 PCF 未完成，保留硬阴影，不影响交付
~~~

“研究阴影”“完善 Renderer”不是可验收任务，粒度太大。

每个阶段结束必须有一个可运行版本。不要等到所有模块分别完成后才第一次集成。

---

### 21.22 状态重置：Restart 最容易暴露隐藏问题

Restart 不是简单把 Score 设为 0。需要恢复：

- GameState 和 Timer。
- 玩家 previous/current Transform、速度和 Ground 状态。
- 所有 Collectible active 和 Trigger 状态。
- Dynamic Body 速度、休眠和位置。
- Camera 跟随/插值状态。
- 一次性 Audio Source。
- UI 临时状态。
- 随机种子（若要求可重复）。

Asset Cache、Shader 和长期 GPU Resource 通常不必重载。Game World 状态与共享 Asset 生命周期是不同层。

连续 Restart 20 次是很好的生命周期压力测试。

---

### 21.23 错误处理的层级

将错误按是否可恢复分类：

#### Fatal：无法继续

例如创建窗口/OpenGL Context 失败。输出具体原因，安全释放已创建资源并退出非 0 状态。

#### Recoverable：可使用替代方案

例如单张 Texture 缺失。显示 Placeholder 并继续，同时记录错误。

#### Warning：结果可能降级

例如音频设备不可用。游戏可无声运行，UI 显示警告。

错误消息至少包含：

~~~text
发生了什么
在哪个 Asset/Pass/对象
底层错误内容
采取了什么恢复动作
用户/开发者下一步可检查什么
~~~

不要只打印 `failed`。

---

### 21.24 测试分层

不需要建立庞大测试框架，但关键逻辑应能验证。

#### Pure Logic Test

不启动窗口即可测试：

- Score 和 Finished 状态。
- Restart 状态重置。
- 输入映射。
- Bounds/Frustum 数学。
- Handle generation 和资源缓存键。

#### Integration Test

启动多个系统：

- Trigger Enter 只收集一次。
- Snapshot 不包含 inactive Entity。
- Asset 加载后 Renderer 能找到 Handle。
- Resize 重建 FBO。

#### Visual Test

固定场景和相机截图：

- 基础材质。
- Shadow。
- Transparency。
- Tone Mapping/Bloom。
- UI。

#### Manual Play Checklist

手动从 Title 玩到 Finished、Pause、Restart、Exit，并尝试异常操作。

自动测试不能完全替代视觉检查，视觉检查也不能替代纯逻辑测试。

---

### 21.25 一份手动测试清单

**Release Candidate（发布候选版本）**是已经停止增加功能、准备通过最后测试后交付的版本。对它逐项执行：

~~~text
[ ] 从空 build 目录 Configure 和 Build
[ ] 从项目文档给出的方式启动
[ ] Title 可开始，窗口可 resize
[ ] WASD/相机行为正确
[ ] 角色不能穿墙
[ ] 每个 Collectible 只计数一次
[ ] Pause 时 Physics/Timer 停止，UI 可操作
[ ] Resume 后无巨大 Delta 或位置跳跃
[ ] 收集完成进入 Finished
[ ] Restart 完整重置
[ ] 连续 Restart 20 次
[ ] 音量关闭时游戏仍可完成
[ ] 故意缺少一个非关键 Texture 时出现 Placeholder
[ ] 退出时无 OpenGL 高严重度错误
[ ] GPU 截帧或等价调试证据与帧图一致
[ ] Release 在目标场景达到性能目标
~~~

每个 Bug 写最短复现步骤。无法重现的问题很难可靠修复。

---

### 21.26 Bug 错误排查与记录模板

~~~markdown
## 标题
一句话描述现象。

## 环境
提交、构建类型、机器、分辨率、设置。

## 重现步骤
1. ...
2. ...
3. ...

## 预期结果
应该发生什么。

## 实际结果
实际看到什么。

## 证据
日志、截图、GPU Capture Event（平台支持时）、Profiler 时间线。

## 初始假设
认为哪一段数据链可能错误。

## 根因
第一个错误状态或错误数据在哪里产生。

## 修复与验证
改了什么；用什么测试证明修复且未回归。
~~~

“把某个数调到看起来正常”不等于找到根因。

---

### 21.27 Frame Graph 文档

技术报告必须有一张帧图和资源表。例如：

~~~text
Shadow Pass
    |  Shadow Depth
    v
Opaque Pass
    |  Scene Color + Depth
    v
Transparent Pass
    |  HDR Scene Color
    ├──────────────┐
    v              v
  Bloom         Tone Map  <── Bloom Texture
                   |
                   v  LDR Final Color
                  UI
                   |
                   v
                Present
~~~

对每个 Pass 写：

- 它为什么存在。
- 输入资源从哪里来。
- 输出写到哪里。
- Depth/Blend/Cull 状态。
- Draw 哪些队列。
- CPU 和 GPU 时间。
- 如何在调试工具中观察。

只画框箭头而不解释资源格式和读写，不算完整帧图。

---

### 21.28 技术报告目录

推荐报告结构：

~~~text
1. 项目摘要与目标
2. 操作方式和构建运行
3. 系统架构与依赖方向
4. 一帧从 Input 到 Present 的数据流
5. Renderer Pass 与 Resource 表
6. 坐标、单位和颜色空间约定
7. Asset 生命周期
8. Physics、Audio 与 Game Event 集成
9. 调试工具与可观察性
10. 一个完整 Bug 调查；可追加另外两类
11. 性能基线和优化实验
12. 已知限制与取舍
13. Vulkan/Metal 迁移设计
14. 学习回顾
~~~

报告不是 API 名称汇总，而是用自己的项目证据解释概念。

---

### 21.29 Bug 调查应该怎样写

核心交付至少完整记录一个真实 Bug。若准备作品集版本，建议再补到三个，并分别覆盖不同层：

#### 画面 Bug

例如透明物体边缘变黑。通过 Pixel History、Blend State 和 Alpha 类型找到根因。

#### 生命周期 Bug

例如 Restart 后 Texture Handle 失效。通过资源创建/销毁日志发现共享资源被 Entity 错误删除。

#### 时间/玩法 Bug

例如暂停恢复后玩家飞出。通过 Delta/Accumulator 记录发现暂停时间被当成一个巨大物理步。

每个 Bug 都写：现象、错误假设、证据、根因、修复、回归验证。承认最初猜错是报告价值的一部分。

---

### 21.30 性能章节应包含什么

最低内容：

| 指标 | 基线 | 优化后 | 变化 |
| --- | ---: | ---: | ---: |
| CPU Frame P95 | ... ms | ... ms | ... |
| GPU Frame P95 | ... ms | ... ms | ... |
| 目标 Pass | ... ms | ... ms | ... |
| Draw Call | ... | ... | ... |
| Triangle | ... | ... | ... |
| Render Scale | ... | ... | ... |

还需写：

- 测试环境和固定场景。
- 原假设。
- 单一主要改动。
- 同机位前后截图。
- 画质、内存和复杂度代价。
- 为什么保留或撤销。

不要只放一个 FPS 数字。

---

### 21.31 Vulkan/Metal 迁移章节应写什么

不要求毕业项目已经完整迁移。报告至少说明：

- 哪些模块可原样保留：Game、Physics、Audio、Asset 描述、Snapshot。
- 哪些模块必须替换：OpenGL Resource、Pipeline、Command、FBO/Present。
- 当前 Pass 资源读写表如何转换为 Barrier/Resource State。
- Framebuffer resize 如何映射到 Swapchain 重建。
- 每帧数据如何按 Frames in Flight 分配。
- 第一阶段只迁移哪个最小闭环。

如果已经做了现代 API Triangle，可附截图和验证信息，但不应因此拖延 OpenGL 主项目交付。

---

### 21.32 构建和运行文档

README 至少包含：

- 支持的平台和已验证系统版本。
- 编译器、CMake 和依赖要求。
- 从获取源码到运行的完整命令或 VS Code 操作。
- Debug/Release 选择方式。
- 控制方式。
- Asset 路径和工作目录约定。
- 已知问题。

终端流程示例：

~~~zsh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/RenderQuest
~~~

如果程序只能在你 VS Code 当前目录、某个绝对路径或未说明环境变量下运行，就还没有完成交付。

---

### 21.33 干净机器验证

**Clean Build（干净构建）**表示删除生成目录后，从源码和声明依赖重新生成。它能发现：

- 忘记加入 CMake 的 `.cpp`。
- 本机残留文件掩盖缺失 Asset。
- Shader 或动态库没有复制。
- Include Path 只在个人 VS Code 配置中存在。
- 代码依赖绝对路径。

理想情况下在另一台符合要求的机器验证；至少在本机新的 `build-clean/` 目录执行完整流程。

不要提交依赖于个人 `.vscode/tasks.json` 才能构建的工程逻辑。工程规则应在 CMake 中。

---

### 21.34 交付物清单

核心提交包括：

- 可构建源码和 CMake 配置。
- 合法可分发或自制的运行 Asset。
- README 构建、运行、控制说明。
- `technical-report.md` 或导出的 PDF。
- 一张最终画面截图。
- 一张 Debug UI/Render Target 截图。
- 一份平台支持的 GPU Capture，或当前 Mac 上由中间 Texture、Pass 日志和资源绑定构成的清晰分析图。
- 一份性能数据文件或表格。
- 至少一个完整 Bug 调查记录。
- 已知问题和范围外清单。
- 可选：短视频或 GIF 展示完整循环。

作品集增强版可补充另外两类 Bug 调查、更多中间 Texture、GPU Capture 文件和现代 API Triangle，但不能以它们替代核心项目可构建、可完成和可重启。

所有截图应标注对应版本和设置。

---

### 21.35 最终演示顺序

建议演示控制在 10 到 15 分钟：

1. 从 Title 玩到 Finished，再 Restart。
2. 打开 Debug UI，展示 Snapshot、Physics 和 Pass 指标。
3. 显示 Shadow/Scene/Bloom 中间 Texture。
4. 在支持平台打开 GPU Capture，从最终 Pixel 找到一个 Draw；当前 Mac 则展示等价的 Draw 日志与中间 Texture 证据链。
5. 展示帧图和一次性能优化数据。
6. 解释一个真实 Bug 的证据链。
7. 说明若迁移 Vulkan/Metal，第一步和首个 Barrier 在哪里。

不要把演示时间用于滚动大量源码。展示数据流和证据，再指向关键代码。

---

### 21.36 最终评分表

总分可按 100 分自评：

| 类别 | 分数 | 核心要求 |
| --- | ---: | --- |
| 可构建与完整游戏循环 | 20 | 干净构建、Title 到 Finished、Restart/Exit |
| 渲染正确性 | 20 | Mesh/Material、Depth、颜色、Shadow、UI；已选择的增强效果正确 |
| 系统边界 | 15 | Game/Physics/Audio/Asset/Renderer 职责清晰，Snapshot 只读 |
| 调试与可观察性 | 15 | Debug UI、错误日志、中间 Texture、GPU 截帧或等价证据 |
| 性能方法 | 15 | 固定测试、相关 CPU/GPU 分段、至少 P95、有效优化与代价 |
| 技术报告 | 10 | 帧图、生命周期、Bug 调查、迁移路线 |
| 范围控制与完成度 | 5 | 无关键半成品，已知限制诚实明确 |

一个没有 Bloom、但所有 Must 项稳定且报告完整的项目，可以优于功能更多却无法重开或构建的项目。

---

### 21.37 “真正完成”的定义

毕业项目完成意味着：

- 不是只在某次旧 Build 上运行过，而是能从空 Build 重建。
- 不是只有最终截图，而是能从开始玩到结算。
- 不是“画面看起来对”，而是能展示中间资源和 Draw 证据。
- 不是“感觉优化了”，而是有相同条件下的毫秒对比。
- 不是 Renderer 能访问所有系统，而是依赖和所有权清晰。
- 不是没有 Bug，而是已知问题有记录，关键问题有调查方法。
- 不是功能越多越好，而是承诺范围全部闭环。

---

#### 本部分检查点

**能做出来**：一份交付证据：截图、性能数字、已知问题列表，以及一份说清设计取舍的技术报告。

**能回答**：

- 你怎么证明这一关在目标帧率下稳定？
- 已知限制里哪一条你会第一个去修？
- 如果别人要接手这个项目，他最需要先读什么？

**卡在这里就先不要往下读**：交付证据凑不齐，说明项目还没到“真正完成”的定义（21.37）。

## 第四部分：总复习与最终验收

### 21.38 课程总复习：跟踪最终一个 Pixel

选择最终截图中玩家衣服上的一个 Pixel，用自己的项目回答：

1. 它属于哪个 Triangle？
2. Triangle 的 Vertex 来自哪个 glTF Mesh 和 GPU Buffer？
3. Model Matrix 来自哪个 Entity 的插值 Transform？
4. View/Projection 来自哪份 Camera Snapshot？
5. Fragment Shader 采样了哪些 Texture？
6. Normal 和 Light 怎样得到光照值？
7. Shadow Map 在哪个 Pass 生成、何时读取？
8. Depth Test 为什么让它可见或被挡住？
9. 当前颜色写入默认 Framebuffer，还是先写入 Scene Color？对应格式是什么？
10. 若实现 Tone Mapping/Bloom，它们怎样改变颜色；若未实现，颜色从光照后怎样到达最终目标？
11. UI 是否在它上方再次混合？
12. 最终哪张 Buffer 被 Present？

如果每个答案都能指向项目中的数据、Pass、截图或代码，这套课程就完成了真正闭环。

---

### 21.39 课程总复习：跟踪一次收集事件

再选择一次玩家收集物：

1. 键盘怎样变成 PlayerIntent？
2. 固定 Tick 怎样移动 Character Controller？
3. Physics 怎样报告 Trigger Enter？
4. Game 为什么只加一次分？
5. 收集物怎样从下一份 Snapshot 消失？
6. UI 数字怎样更新？
7. Audio Event 怎样找到共享 Buffer 并播放一次？
8. Restart 怎样恢复 Trigger 和 Entity？
9. 这些过程如何记录和调试？

这条链证明你做的不只是 Renderer Demo，而是一个小型 3D 游戏。

---

### 21.40 结题练习

#### 练习 A：范围表

写 Must/Should/Could，每项包含“可见结果、依赖章节、验收、回退方案”。Must 不超过 12 项。

#### 练习 B：架构图

画出 Input、Game、Physics、Snapshot、Renderer、Audio、Asset 的数据与依赖方向，明确禁止依赖。

#### 练习 C：帧图

写出每个 Pass 的输入、输出、格式、Load/Store、Depth/Blend 状态和 GPU 时间。

#### 练习 D：垂直切片

在任何画面美化前，完成一个 Collectible 的输入到声音/画面/UI 全链路，并保留截图。

#### 练习 E：重置测试

连续完成并 Restart 20 次。记录 Entity、Body、Audio Source 和 Asset Resource 数量，确认没有持续增长或状态残留。

#### 练习 F：干净构建

新建 `build-clean/`，只按 README 操作。记录所有遗漏并修正文档或 CMake。

#### 练习 G：性能报告

完成一次有效优化和一次被撤销实验，使用第 19 章模板。

#### 练习 H：最终讲解

不查看术语表，用 10 分钟讲清“一个 Pixel”和“一次收集事件”的完整来源。录音后检查是否出现未定义术语或跳过的数据步骤。

#### 练习 I：故意出错实验（破坏集成边界）

毕业项目的 Bug 大多不在单个系统内部，而在**系统之间**。每次只改一处，先预测，再运行，然后恢复：

1. 让 Gameplay 和 Physics 同时写同一个 Transform。
2. 重开关卡时不重置分数或 Entity 状态。
3. Render Snapshot 保存引用而不是值，然后在渲染中途修改游戏状态。
4. 收集事件不去重，让同一个收集物触发两次。
5. 关卡切换时不释放上一关的资源。
6. 把一个 Pass 的状态设置删掉，依赖上一个 Pass 留下的状态。

第 3 条和第 6 条是整合阶段最难查的两类问题：它们都**时好时坏**，取决于执行顺序。做完后把现象写进 [plan/progress.md](../plan/progress.md)，这正是技术报告里“已知限制”那一节的素材。

---

### 21.41 最终验收标准

这份清单很长，但不是一次做完的。按下面三档推进：

- **最低（这一关算做完了）**：「构建与运行」+「游戏循环」两组全部通过，加上「Renderer」的前两条。此时你有一个能从空目录构建、能玩、能重开的完整作品。
- **推荐（可以拿出去给人看）**：再通过「Renderer」剩余条目和「系统边界」全组。
- **扩展（作品集级别）**：剩余各组，以及技术报告和性能报告。

先把最低档走通再回头补。一条一条按顺序勾，不要并行推进多组——整合阶段的 Bug 大多来自同时改动多个系统。

#### 构建与运行

- [ ] 从空 Build 目录可 Configure、Build、Run。
- [ ] README 不依赖个人绝对路径或隐藏 VS Code JSON。
- [ ] Debug 和 Release 都能启动，Release 用于性能数据。
- [ ] 关键 Asset 随运行产物正确部署。

#### 游戏循环

- [ ] Title、Playing、Paused、Finished、Restart、Exit 完整。
- [ ] 固定 Physics 与可变渲染分离，移动不依赖 FPS。
- [ ] 暂停恢复无巨大 Delta，Restart 完整重置。
- [ ] 游戏能在 3 到 10 分钟内完成。

#### Renderer

- [ ] glTF Mesh/Material/Texture 正确。
- [ ] Camera、Depth、Culling 和 Render Queue 正确。
- [ ] Linear/sRGB 链可解释；实现 HDR 时再验收 Tone Mapping。
- [ ] Shadow、Opaque、UI Pass 有明确输入输出；实际实现的 Transparent/Post/Sky 也有合同。
- [ ] Resize 后 Viewport、FBO 和 Texture 尺寸正确。

#### 系统边界

- [ ] Renderer 只读 Render Snapshot。
- [ ] Physics Shape 与 Render Mesh 分离。
- [ ] Audio 由 Game Event 驱动。
- [ ] Entity 使用 Asset Handle，共享资源不被任意删除。
- [ ] GPU/Audio Resource 在 Context/Device 销毁前释放。

#### 调试和正确性

- [ ] Shader/Pass 日志无未处理高严重度问题；Context 支持时再检查 OpenGL Debug Output。
- [ ] Debug UI 能查看关键指标、中间 Texture 和系统状态。
- [ ] 能从最终错误结果定位到 Draw 和输入资源；支持平台可用 GPU Capture，当前 Mac 使用日志、中间 Texture 与替换实验。
- [ ] 缺失非关键 Asset 有 Placeholder 和清晰日志。
- [ ] 至少一个真实 Bug 有完整证据链；作品集增强版建议补到三个不同层次。

#### 性能

- [ ] 目标平台、分辨率、场景、画质和 FPS 明确。
- [ ] 与性能假设有关的 CPU/GPU 分段和至少 P95 已记录。
- [ ] 有一次前后可比且画面正确的有效优化。
- [ ] 有一次无效实验及撤销说明。

#### 报告和交付

- [ ] 技术报告包含架构、帧图、生命周期、Bug、Profile 和迁移路线。
- [ ] 截图、Capture 和数据标记版本与设置。
- [ ] 已知限制和范围外功能明确。
- [ ] 另一位读者按文档能构建并完成一局。

---

### 21.42 完成后的下一条路线

毕业项目通过验收后，再选一个方向，不要同时扩张：

#### 画面路线

PBR、IBL、动画、级联阴影、TAA、更多 LOD。每次仍按 Pass、资源和性能证据推进。

#### 现代 API 路线

用 Vulkan + MoltenVK 或 Metal 重建 Triangle -> Texture -> Depth -> Scene Color/Post-process，再接同一 Render Snapshot。

#### 引擎结构路线

更完善的 Scene 序列化、Job System、Render Graph 和热重载。必须由现有真实复杂度推动，而不是提前设计万能框架。

#### 内容路线

在不破坏系统边界的前提下增加关卡、动画、音频和玩法。

无论选择哪一条，都保留本书的核心习惯：先定义输入和输出，先做最小闭环，用工具观察，用数据验证，再增加复杂度。
