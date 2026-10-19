# 第 18 章：物理、音频与资产生命周期

<!-- nav -->
> [上一章：游戏循环、时间与渲染快照](17-game-loop-render-snapshot.md) ｜ [下一章：性能分析与有证据的优化](19-profiling-optimization.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/08_gameplay/18-physics-audio-assets.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

这一章把“会动、能碰撞、会发声、能正确加载和释放资源”接到上一章的 Game Loop 中。

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 划清 Physics、Gameplay 和 Renderer 三者的边界，说明 Transform 归谁所有。
2. 区分 Static、Dynamic 和 Kinematic Body，并说出各自适合什么对象。
3. 解释 Broad Phase、Narrow Phase 和 Solver 三个阶段分别在做什么。
4. 让一个 Dynamic Box 在固定 Tick 中落到地面并停稳，不抖动也不穿透。
5. 说明音效为什么要预加载，以及一次 Game Event 怎样只播放一次。
6. 用 Asset Manager 和 Handle 让多个 Entity 共享同一份 Mesh、Texture 和音效。
7. 说清资源在什么时刻创建、什么时刻释放，并在退出时不泄漏。

这几个领域都很大。本课程不要求你自研工业级物理引擎、音频解码器或资产管线，而是要求你理解它们的数据如何产生、由谁拥有、在一帧的什么位置更新，以及怎样与 Renderer 保持清晰边界。

本章分四部分：`18.1～18.15` 讲物理，`18.16～18.20` 讲音频，`18.21～18.30` 讲资产生命周期，`18.31～18.38` 把三个系统接入同一个可玩例子。先分别验证每个系统，再做整合。

不要把本章当成一次大改造。第一次只完成三件可单独观察的事：

1. **物理最小闭环**：Static Ground 上有一个 Dynamic Box，Physics 在固定 Tick 更新，Renderer 只读取结果。
2. **音频最小闭环**：程序启动时预加载一个短音效，按键或一次 Game Event 只播放一次。
3. **资产最小闭环**：Mesh、Texture 和短音效由 Asset Manager 常驻缓存，多个 Entity 只保存并共享 Handle，退出时统一释放。

角色控制器、Trigger 收集物是推荐整合；音乐 Streaming、Generation Handle、异步加载和 GPU 延迟删除是之后按需求增加的扩展。

**关于物理引擎**：正文会讲到 Broad Phase、Narrow Phase、Solver 这些工业级引擎（例如 Jolt Physics）的内部分工，目的是让你知道自己在用什么、遇到穿透和抖动时该怀疑哪一步。但**Jolt 不是本书的可运行依赖，第一遍不需要安装它**。[本章的参考实现](../exercises/08_gameplay/18-physics-audio-assets.md)只用不到一百行写了单个 Dynamic Body 对平面和边界的碰撞，够用来验证“固定步、Transform 归谁所有、事件只消费一次”这三件真正容易写错的事。等关卡里出现斜坡、堆叠和角色控制器时再换成 Jolt，`fixedPhysicsStep` 的调用位置不用改。

同理，音频用的是 macOS 自带的 AudioToolbox（`exercises/common/audio.hpp`），不需要 OpenAL 或 miniaudio。

## 第一部分：物理

### 18.1 先建立系统边界

一个可见、可碰撞、会发声的角色不是一个巨大对象完成所有工作，而是几个系统从不同角度描述同一个游戏实体。

**Entity（实体）**是游戏世界中一个可被识别的对象，例如玩家、门或收集物。它可以由若干 **Component（组件）**组成；组件只保存某一方面的数据，例如 Transform、Renderable 或 Audio Source。这里不要求先实现通用 ECS，只需用这个词表达“同一个游戏对象的不同职责”。

~~~text
Game Entity：玩家
├── Transform：游戏世界中的位置和朝向
├── Character/Physics Body：碰撞和运动规则
├── Renderable：Mesh、Material、可见性
├── Audio Source：脚步、受伤等声音
└── Gameplay State：生命值、得分、输入意图
~~~

系统职责：

- **Game**：决定规则，例如玩家是否能跳、收集物是否得分。
- **Physics**：根据速度、力和碰撞约束计算物体怎样移动。
- **Renderer**：读取最终 Transform 和表现资源，生成 Pixel。
- **Audio**：根据事件与声源状态，生成扬声器采样。
- **Asset Manager**：加载、共享、查找和释放 Mesh、Texture、声音等资源。

“分开”不意味着它们完全没有联系，而是联系通过明确数据和事件发生。

~~~text
Input Intent
    |
    v
Game Rules ─────────────────┐
    |                       | Gameplay Event
    v                       v
Physics                   Audio
    |                       ^
    v                       |
Transform                   |
    |                       |
    v                       |
Render Snapshot             |
    |                       |
    v                       |
Renderer                    |
    ^                       |
    └──── Asset Manager ────┘   （为 Renderer 和 Audio 提供共享资源）
~~~

---

### 18.2 先统一单位和坐标

物理公式只有在单位一致时才有意义。推荐约定：

- 距离：米 `m`
- 时间：秒 `s`
- 质量：千克 `kg`
- 速度：米/秒 `m/s`
- 加速度：米/秒平方 `m/s^2`
- 力：牛顿 `N = kg * m/s^2`

如果一个角色在模型文件中高 180 个单位，而 Physics 把 1 单位理解成 1 米，它会成为 180 米高。导入时需要统一尺度，例如让角色约为 `1.8 m`。

还要统一坐标约定：

- 哪个轴向上？例如 y-up。
- 哪个方向是前？例如局部 -z。
- 角度是度还是弧度？
- 物理库与渲染器矩阵使用的左右手系是否一致？

把约定写进项目文档，导入模型时集中转换，不要让每个玩法脚本自行猜测。

---

### 18.3 Transform、Mesh、Body 和 Shape

这四个词经常被初学者混为一谈。

#### Transform

描述物体在世界中的位置、旋转和缩放。Renderer 用它构造 Model Matrix。

#### Mesh

描述可见外形的 Vertex 和 Index。它追求视觉细节，可能包含数万个 Triangle。

#### Physics Body

物理世界中的对象，通常保存位置、旋转、速度、质量、是否受重力等状态。

#### Collision Shape

用于碰撞检测的几何形状，例如 Box、Sphere、Capsule 或简化凸包。

~~~text
玩家看起来：复杂人物 Mesh
玩家碰撞：简单 Capsule

        可见 Mesh
       /  o   o  
      |     ^     |
      |   -----   |
       \_________/
           ||
      .-----------.
     /             \   <- Capsule Collision Shape
     |             |
     \_____________/
~~~

碰撞 Shape 通常比 Mesh 简单，因为它需要在每个固定步参与许多几何计算。简单 Shape 更快、更稳定，也更容易设计手感。

---

### 18.4 位置、速度、加速度、力和质量

几个基础物理量：

- **位置（Position）**：当前在哪里。
- **速度（Velocity）**：位置变化率，包含方向和快慢。
- **加速度（Acceleration）**：速度变化率。
- **质量（Mass）**：物体对力产生加速度的抵抗程度。
- **力（Force）**：推动或拉动物体的作用。
- **重力（Gravity）**：持续向下的加速度，地球附近约 `9.81 m/s^2`。

牛顿第二定律：

~~~text
F = m * a
所以 a = F / m
~~~

同样的 10 N 力：

~~~text
质量 1 kg -> 加速度 10 m/s^2
质量 5 kg -> 加速度  2 m/s^2
~~~

物理库会替你进行更复杂的积分和约束求解，但你仍需知道输入的是力、速度还是直接位置。三者手感和碰撞行为不同。

---

### 18.5 Body 的三种常见类型

#### Static Body（静态物体）

不由物理模拟移动，质量可视为无限大。适合地面、墙壁和固定建筑。

#### Dynamic Body（动态物体）

受重力、力、速度和碰撞影响。适合箱子、球、碎片。

#### Kinematic Body（运动学物体）

由游戏代码指定运动，但会参与碰撞。适合移动平台、受脚本控制的门。具体行为取决于物理库。

| 类型 | 谁决定 Transform | 受力 | 常见用途 |
| --- | --- | --- | --- |
| Static | 创建时固定 | 否 | 地面、墙 |
| Dynamic | Physics | 是 | 箱子、球 |
| Kinematic | Game/动画 | 通常否 | 电梯、门 |

不要每帧直接修改 Dynamic Body 的 Transform 来“移动角色”。这相当于瞬移，可能绕过速度、碰撞响应和连续检测。应使用角色控制器、速度、力或物理库提供的移动接口。

物理引擎还常让长时间不动的 Dynamic Body 进入 **Sleeping（休眠）**：暂时不再为它执行完整模拟，直到碰撞、力或代码将它唤醒。这能节省工作；若物体“应该动却不动”，休眠状态也是检查项。

---

### 18.6 Collision Shape 的选择

常见 Shape：

- **Sphere（球）**：便宜、方向无关，适合球体和粗略范围。
- **Box（盒）**：适合箱子、墙体和方形区域。
- **Capsule（胶囊）**：中间圆柱加两端半球，适合角色。
- **Convex Hull（凸包）**：包住一组点的凸形外壳，可描述较复杂物体。
- **Triangle Mesh Collider（三角网格碰撞）**：用许多 Triangle 描述固定场景，通常只适合 Static Body。

**Convex（凸）**的直觉：形状内任意两点连线都不会跑到形状外。凸形状的碰撞算法通常比任意凹形状更稳定。

角色常用 Capsule，因为底部圆滑，不容易被细小边缘卡住；但 Capsule 本身不会自动解决台阶、斜坡和地面吸附，角色控制器仍需规则。

---

### 18.7 碰撞检测不是“拿所有物体两两精确比较”

如果场景有 1000 个物体，两两检查约需要近 50 万对。物理引擎通常分阶段减少工作。

#### Broad Phase（宽阶段）

用简单 Bounds 快速排除明显相距很远的物体。常见结构有动态 AABB Tree、Sweep and Prune 等。你不必先实现它们，只需理解结果是一小批“可能相交的候选对”。

#### Narrow Phase（窄阶段）

对候选对使用具体 Shape 算法，判断是否真正相交，并计算接触点、法线和穿透深度。

#### Solver（求解器）

根据接触约束调整速度或位置，使物体不继续穿透，并近似产生摩擦与反弹。

~~~text
所有 Body
-> Broad Phase：可能碰撞的候选对
-> Narrow Phase：真正的接触信息
-> Solver：修改速度/位置满足约束
-> 新 Transform
~~~

**Contact（接触）**通常包含：

- 接触点在哪里。
- 接触法线朝哪个方向。
- 穿透了多少。
- 两个表面的摩擦和恢复系数。

Broad Phase 中常用 **AABB（Axis-Aligned Bounding Box，轴对齐包围盒）**：一个边始终与世界坐标轴平行的盒子，比较最小/最大坐标就能快速判断是否可能重叠。Dynamic AABB Tree 把这些盒子组织成树；Sweep and Prune 则沿坐标轴排序区间并排除不重叠者。这里不要求实现它们，只需知道它们都在避免昂贵的全部两两精确检测。

---

### 18.8 摩擦、反弹和 Restitution

- **Friction（摩擦）**：抵抗接触面切向滑动。
- **Restitution（恢复系数）**：控制碰撞后弹性，0 接近不弹，1 接近保留法向速度。

这些不是“材质看起来粗糙”的渲染参数。Physics Material 和 Render Material 是不同概念：

~~~text
Render Material：Base Color、Normal、Roughness、Shader
Physics Material：Friction、Restitution
~~~

它们可以由同一个高层表面定义关联，但底层职责不同。

---

### 18.9 Discrete Collision 和 Tunneling

普通离散碰撞只检查固定步前后状态。高速小物体可能从墙一侧跳到另一侧，中间从未在采样时刻与墙相交，这叫 **Tunneling（穿透/隧穿）**。

~~~text
Tick 0:  球        |墙|
Tick 1:            |墙|      球
         两个时刻都没有重叠，但路径穿过墙
~~~

常见处理：

- 减小固定时间步。
- 加厚碰撞体。
- 对高速关键物体开启 **CCD（Continuous Collision Detection，连续碰撞检测）**。
- 用 Ray Cast 或 Shape Cast 检查运动路径。

CCD 成本更高，不应对所有物体无差别开启。

---

### 18.10 Trigger：只报告进入，不把物体推开

**Trigger / Sensor（触发器）**参与重叠检测，但通常不产生实体碰撞响应。

适合：

- 收集物范围。
- 关卡出口。
- 伤害区域。
- “玩家进入房间”事件。

~~~text
普通 Collider：检测接触 + 阻止穿过
Trigger：检测进入/停留/离开，不阻止穿过
~~~

Game 层处理 Trigger 事件：

~~~cpp
void Game::onTriggerEnter(EntityId a, EntityId b) {
    if (isPlayer(a) && isCollectible(b)) {
        collect(b);
        events.push(AudioEvent::PlayOneShot(SoundId::Collect));
    }
}
~~~

Physics 只报告“谁与谁发生了什么”，不直接增加分数、播放声音或删除 Renderer 资源。

---

### 18.11 Collision Layer 和 Mask

不是所有对象都需要互相检测。Layer 和 Mask 用来表达过滤规则。

例如：

| A \ B | World | Player | Enemy | Collectible |
| --- | --- | --- | --- | --- |
| Player | 碰撞 | - | 碰撞 | Trigger |
| Enemy | 碰撞 | 碰撞 | 可选 | 忽略 |
| Projectile | 碰撞 | 依阵营 | 依阵营 | 忽略 |

- **Layer**：对象属于哪一类。
- **Mask**：它希望与哪些类别发生查询或碰撞。

过滤既是玩法规则，也是性能优化。错误配置会表现为“看得见但碰不到”或“本应忽略却撞上”。调试 UI 应显示 Body 的 Layer、Mask 和 Shape。

---

### 18.12 Ray Cast 和 Shape Cast

**Ray Cast（射线检测）**从起点沿方向寻找第一个相交 Shape。它不是在屏幕上画线，而是一个几何查询。

常见用途：

- 检查角色脚下是否有地面。
- 点击选择 3D 对象。
- 枪械命中测试。
- 相机避免穿墙。

结果通常包含：

~~~cpp
struct RayHit {
    EntityId entity;
    Vec3 point;
    Vec3 normal;
    float distance;
};
~~~

**Shape Cast** 则让 Sphere/Capsule 等形状沿路径移动，适合角色运动和相机碰撞，比一条无限细的 Ray 更符合实际体积。

---

### 18.13 角色控制器不是普通刚体的简单别名

玩家期望的角色行为通常不完全符合自由刚体：

- 按键松开后希望快速停下。
- 能走上小台阶。
- 能稳定贴住斜坡和地面。
- 不希望被小箱子轻易撞飞。
- 跳跃高度和空中控制需要可设计。

因此很多物理库提供 Character Controller，或使用 Kinematic Capsule 加 Shape Cast 自行实现规则。

初学版只需：

1. 从 `PlayerIntent` 得到期望水平速度。
2. 应用重力与跳跃速度。
3. 让角色控制器尝试移动。
4. 根据碰撞修正位置和是否在地面。
5. 将结果写入当前 Transform。

不要一开始自研完整接触求解器；使用 Jolt 或 Bullet，让注意力留在系统边界和渲染学习上。

---

### 18.14 Physics 在固定更新中的位置

一个固定 Tick 可以按下面顺序组织：

~~~text
1. 保存 previous Transform
2. 消费本 Tick 的玩家/AI 意图
3. 给 Body 设置力、速度或控制器目标
4. Physics step(fixedDelta)
5. 读取 Dynamic Body 的结果到 current Transform
6. 收集碰撞和 Trigger 事件
7. Game 根据事件更新得分、生命值和对象状态
~~~

渲染帧随后在 previous/current 之间插值，并写入 Snapshot。

**Transform 的权威来源（Authority）**必须明确：

- Dynamic Body：Physics 是权威，Game 读取结果。
- Static Body：创建时的 Game/Scene 数据是权威。
- Kinematic Body：Game 提供目标，Physics 用它影响其他物体。

若 Game 和 Physics 每帧互相覆盖同一个 Transform，就会抖动或瞬移。

---

### 18.15 用库不等于放弃学习原理

Jolt、Bullet 等物理库会实现 Broad Phase、Narrow Phase、Solver、Sleeping 和 CCD。你仍需要决定：

- Body 类型和 Shape。
- 单位、固定步长和 Layer。
- 谁拥有 Body。
- 何时创建/销毁。
- 如何把 Physics Transform 交给 Snapshot。
- 如何把碰撞事件转换为玩法事件。

这正是游戏工程中真正需要掌握的边界。自己从零写可靠 Solver 会把课程重心从渲染转向数值物理。

---

进入下一部分前，先验证：Static、Dynamic、Kinematic Body 行为可区分，Trigger 只报告事件，Physics 只在固定步中更新。

#### 本部分检查点

**能做出来**：一个 Dynamic Box 落到 Static Ground 上停稳，不抖动、不下沉、不穿透；Renderer 只读取结果。

**能回答**：

- Transform 在这一帧里归谁所有？
- Static、Dynamic、Kinematic 各适合什么对象？
- Broad Phase、Narrow Phase、Solver 各做什么？

**卡在这里就先不要往下读**：物体抖动或穿透，先确认物理跑在固定 Tick 上（第 17 章那一步）。在变化的 deltaTime 上调物理参数是白费功夫。

## 第二部分：音频

### 18.16 数字声音从哪里来

声音是空气压力随时间变化。麦克风或声音制作软件把连续波形定期测量为数字，这叫 **Sampling（采样）**。

~~~text
连续波形：     /\      /\
             /  \    /  \
------------/----\--/----\------> 时间

离散采样：   . . . . . . . . .
每个点保存当时的振幅数值
~~~

术语：

- **Sample（采样点）**：某个时刻的振幅数值。
- **Sample Rate（采样率）**：每秒采多少次，例如 44,100 Hz 或 48,000 Hz。
- **Channel（声道）**：独立信号数量。Mono 是 1，Stereo 是左右 2 个。
- **PCM**：直接保存采样值的一类表示。
- **Bit Depth（位深）**：每个 Sample 使用多少位，影响可表示的振幅精度。
- **Codec（编解码格式）**：压缩与解压音频数据的方法，例如 Vorbis、Opus、AAC。

一段 Stereo 48 kHz 音频每秒需要左右声道各 48,000 个 Sample。

---

### 18.17 音频系统中的 Buffer、Source 和 Listener

- **Audio Buffer**：已解码的声音 Sample 数据。
- **Audio Source**：播放声音的发声对象，保存音量、音高、循环和空间位置。
- **Listener**：听者，通常跟随主相机或玩家头部。
- **Audio Device**：操作系统最终输出 Sample 的设备。

~~~text
sound file
-> read/decode
-> Audio Buffer
-> Audio Source 按播放位置读取 Sample
-> 混合多个 Source
-> 根据 Listener 做空间处理
-> Audio Device
-> 扬声器
~~~

Renderer 不需要知道 Audio Buffer。Audio 也不需要 OpenGL Texture。它们只共享游戏世界中的位置和事件。

---

### 18.18 短音效、音乐和 Streaming

短音效适合一次全部解码到内存：

~~~text
收集、脚步、点击、碰撞
优点：触发快
代价：占用内存
~~~

长音乐适合 **Streaming（流式播放）**：只保留一小段已解码 Sample，播放时持续从文件读取和补充。

~~~text
长背景音乐
磁盘/压缩文件 -> 小块解码 -> 播放队列 -> 持续补充
~~~

若把一小时音乐全部解码为 PCM，内存会很大；若每次播放短音效才临时打开文件解码，则会产生延迟和卡顿。

第一版只预加载并播放一个短音效。等你能确认“文件只加载一次、同一事件只触发一次、Source 播完会回收”后，再学习背景音乐 Streaming。本章先解释 Streaming 的原因和数据流，不把它列为第一次实现要求。

---

### 18.19 音量、音高和空间音频

- **Gain / Volume**：声音振幅倍率。多个声音叠加可能超过范围并产生 Clipping。
- **Pitch**：播放采样的速度比例，会同时影响音高和时长，具体库也可能提供独立处理。
- **Pan**：左右声道分配。
- **Attenuation（衰减）**：声源距离越远，音量通常越小。
- **Spatial Audio（空间音频）**：根据 Source 与 Listener 的相对位置计算方向、距离等听感。

最简距离衰减可以先用可控函数，而不是直接套真实物理：

~~~text
gain = 1 / (1 + k * distance^2)
~~~

其中 `k` 控制衰减速度。游戏音频首先服务可读性和体验，不必完全模拟现实。

Listener 的位置和朝向通常来自本帧相机或玩家 Transform。若坐标手系转换错误，左边的声音可能从右边传来。

---

### 18.20 用事件触发声音

不要让 Renderer 在“收集物不见了”时猜测应该播放声音。Game 发生确定事件后，显式通知 Audio。

~~~cpp
struct PlaySoundEvent {
    SoundHandle sound;
    Vec3 worldPosition;
    float gain;
};

void Game::collect(EntityId item) {
    score += 1;
    disableEntity(item);

    audioEvents.push_back({
        assets.sound("collect"),
        transform(item).position,
        1.0f
    });
}
~~~

Audio 系统消费事件并创建一次性 Source。播放结束后要回收 Source，不能每次收集都永久保留一个对象。

脚步声通常由角色运动/动画事件触发，而不是 Renderer 发现脚的位置接触了 Pixel。

多个 Source 混合后的振幅若超过输出能表示的范围，会被截到最大值，产生刺耳失真，这叫 **Audio Clipping（音频削波）**。可通过合理 Gain、总线音量和限制器避免，而不是让所有 Source 都以最大音量叠加。

---

进入下一部分前，先验证：同一个游戏事件只触发一次声音；多个 Audio Source 可以共享同一 Audio Buffer，且播放声音不需要每帧重新加载文件。

#### 本部分检查点

**能做出来**：按键或一次碰撞事件播放一个短音效，只响一次，不卡顿主循环。

**能回答**：

- 音效为什么要在启动时预加载？
- 一次 Game Event 怎样保证只触发一次播放？
- 音频更新应该放在一帧的什么位置？

**卡在这里就先不要往下读**：声音延迟或重复播放，问题几乎总在事件层，不在音频库。

## 第三部分：资产生命周期

### 18.21 Asset、Resource 和 Handle

这些词在资料中有时混用，本书采用以下约定：

- **Source Asset（源资产）**：磁盘上的原始或交换文件，如 `.gltf`、`.png`、`.ogg`。
- **CPU Asset**：文件读取并解析/解码后的内存数据，如 Vertex 数组、RGBA Pixel、PCM Sample。
- **GPU Resource**：上传到 GPU 后的 Buffer、Texture、Sampler 等。
- **Audio Resource**：交给音频库管理的 Buffer 或 Stream。
- **Handle**：程序引用资源的轻量标识，不直接复制大型数据。

~~~text
tree.gltf + tree.png
        |
        v
文件读取、解析、图片解码
        |
        v
CPU Mesh / CPU Image
        |
        v
OpenGL 上传
        |
        v
GPU Buffer / GPU Texture
        |
        v
MeshHandle / TextureHandle 供对象共享
~~~

“文件已经读出来”不等于“GPU 已经能画”。上传和创建 GPU Resource 是另一个阶段。

---

### 18.22 一个 Texture 的完整生命周期

以 `tree.png` 为例：

1. 请求：场景声明需要 `tree.png`。
2. 查缓存：Asset Manager 检查是否已经加载。
3. 文件 I/O：从磁盘读取压缩字节。
4. 解码：把 PNG 变为宽、高和 RGBA Pixel。
5. 验证：尺寸、格式和错误状态是否合理。
6. GPU 创建：`glGenTextures` 并分配存储。
7. 上传：将 Pixel 复制到 GPU Texture。
8. 生成 Mipmap 并设置 Sampler。
9. 释放临时 CPU Pixel，前提是没有其他用途。
10. 场景对象通过 Handle 共享 Texture。
11. 最后一个使用者离开后，安排删除 GPU Resource。

注意第 9 步：CPU Pixel 可释放，不代表 GPU Texture 被释放。它们是两份位于不同内存区域的数据。

---

### 18.23 为什么需要缓存和共享

场景中 100 棵树可以共享同一份 Mesh 和 Texture，只拥有不同 Transform。

~~~text
Tree Mesh -----------+
Tree Material -------+----> Tree instance 1: Transform A
                     +----> Tree instance 2: Transform B
                     +----> Tree instance 3: Transform C
~~~

如果每棵树都重复加载和上传：

- 浪费内存和显存。
- 增加启动时间。
- 增加 Draw 状态切换。
- 删除任何一棵树时难以判断资源是否还能释放。

Asset Manager 通常以规范化路径或 Asset ID 为键缓存资源。

~~~cpp
MeshHandle AssetManager::loadMesh(const AssetId& id) {
    auto it = meshCache.find(id);
    if (it != meshCache.end()) {
        return it->second.handle;
    }

    CpuMesh cpu = loadAndParseMesh(id);
    GpuMesh gpu = uploadMesh(cpu);
    return meshCache.insert(id, std::move(gpu));
}
~~~

第一版可以同步执行。先把生命周期做对，再做异步。

---

### 18.24 所有权、引用和 RAII

**Ownership（所有权）**回答：谁负责最终销毁这个资源？

一个推荐边界：

- Asset Manager 拥有 GPU Mesh、Texture 和 Audio Buffer。
- Entity 只保存 Handle，不直接调用 `glDeleteTexture`。
- Renderer 在 Draw 时通过 Handle 查到只读资源。
- 场景卸载时释放场景持有的 Handle。
- Asset Manager 根据策略回收不再使用的资源。

C++ 的 **RAII** 表示对象生命周期结束时由析构函数释放其拥有的资源：

~~~cpp
class Texture {
public:
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    ~Texture() {
        if (id_ != 0) {
            glDeleteTextures(1, &id_);
        }
    }

private:
    GLuint id_ = 0;
};
~~~

这里禁用复制，是为了避免两个 C++ 对象都认为自己拥有同一个 OpenGL ID 并重复删除。Move 则把所有权转移给新对象。

RAII 解决“拥有者销毁时释放”，但不能替你决定共享缓存何时移除，也不能保证 OpenGL Context 仍然有效。销毁顺序仍需设计。

---

### 18.25 Handle 为什么比裸指针稳定

简单 Handle 可以由索引和代数（generation）组成：

~~~cpp
struct TextureHandle {
    uint32_t index;
    uint32_t generation;
};
~~~

资源槽被删除并重用时 generation 增加。旧 Handle 再访问时可被识别为失效，而不是悄悄指向另一个 Texture。

初学版也可以先用不可复制的资源对象加稳定 ID，但要理解裸 OpenGL `GLuint` 只是一串数字：

- 它不表示所有权。
- 它不保证资源仍存在。
- 不同资源类型可能出现相同数字。
- Context 销毁后它没有意义。

不要把 `GLuint` 当成完整 Asset 系统。

本章第一次实现不要求立刻写 Slot Array 和 Generation。可以先让 Asset Manager 用 `AssetId` 查找常驻缓存，并返回一个不会在运行中失效的简单 ID。只有开始卸载、复用资源槽后，Generation 才真正解决“旧 Handle 错指向新资源”的问题。

---

### 18.26 引用计数、场景所有权和缓存策略

资源何时释放有多种策略：

#### 引用计数

每增加一个使用者计数加一，减少时减一，归零后释放。优点直观；缺点是循环引用、频繁操作和“缓存本身算不算引用”需要定义。

#### 场景/关卡所有权

加载关卡时获得一组资源，卸载关卡时统一释放。适合小型项目，行为容易预测。

#### 常驻缓存

加载后一直保留到程序结束。实现最简单，适合资源很少的学习项目，但内存会持续增长。

本课程可以先使用“Asset Manager 常驻缓存 + 程序退出统一释放”，完成关卡后再加入场景卸载。不要一开始混用三套没有明确规则的策略。

---

### 18.27 加载状态和失败路径

资源不应只有“存在/不存在”两个模糊状态。可以定义：

~~~cpp
enum class AssetState {
    Unloaded,
    Loading,
    Ready,
    Failed
};
~~~

加载失败时需要记录：

- 请求的 Asset ID 和最终解析路径。
- 失败阶段：打开文件、解析、解码、上传还是 Shader 编译。
- 库返回的具体错误。
- 使用了什么替代资源。

Renderer 可以使用粉紫棋盘格 Texture、单位立方体 Mesh 等 Placeholder，让缺失资源明显可见，同时保持程序能运行。

不要把加载失败静默变成 `0` Handle。黑屏会让你分不清“资产缺失”还是“渲染错误”。

---

### 18.28 同步加载和异步加载

**同步加载**：调用者等待文件读取、解析和上传全部完成。简单、可预测，但大资源会卡住主线程。

**异步加载**：I/O 和解码可在工作线程执行，游戏继续运行；完成后再创建 GPU/Audio 资源。

~~~text
工作线程：文件 I/O -> 解码 CPU 数据
                          |
                          v
主/渲染线程：       GPU Resource 创建与上传
~~~

OpenGL Resource 创建通常需要一个当前有效的 Context。不要默认任意线程都能调用 OpenGL。第一版保持同步；只有测到加载卡顿并完成资源状态设计后，再加入异步。

异步会额外引入：

- Loading 状态。
- 取消请求。
- 场景已卸载但加载刚完成的竞态。
- CPU 数据在线程间转移。
- GPU 上传时机与每帧预算。

这些问题真实存在，但不应阻止你先完成可玩的关卡。

---

### 18.29 GPU Resource 为什么可能延迟删除

CPU 调用 Draw 后，GPU 可能还没执行完。若立刻删除或复用相关资源，需要遵守 API 的生命周期规则。OpenGL 驱动通常会做一部分内部管理，但显式地组织帧生命周期仍更可靠。

一种常见策略是 **Deferred Deletion（延迟删除）**：

~~~text
本帧决定资源不再使用
-> 放入待删除队列
-> 等确认相关 GPU Frame 完成
-> 在拥有有效 Context 的线程真正删除
~~~

第 20 章会用 Fence 和 Frames in Flight 进一步解释。OpenGL 学习版可先在场景停止绘制后释放，并确保 Context 最后才销毁。

因此延迟删除是生命周期知识点，不是本章第一版缓存的必做代码。常驻到程序结束的缓存只要按正确关闭顺序释放，就可以先完成可靠实现。

---

### 18.30 程序关闭顺序

常见错误是先销毁窗口和 OpenGL Context，再让 Texture 析构函数调用 `glDeleteTextures`。此时已经没有有效 Context。

建议顺序：

~~~text
停止游戏循环
-> 停止并销毁 Audio Source/Stream
-> 清空 Scene Entity 和 Snapshot
-> Asset Manager 释放 GPU/Audio Resource
-> Renderer 释放 FBO、Shader、Buffer
-> 销毁 ImGui 后端
-> 销毁 OpenGL Context 和窗口
-> 退出进程
~~~

所有权设计能让顺序自然发生；如果大量使用全局静态对象，析构顺序会变得不可预测。

---

进入整合前，先验证：多个对象共享一份 Mesh、Texture 或 Sound；资源加载失败有明确状态和替代物；销毁顺序不早于最后一次使用。

#### 本部分检查点

**能做出来**：三个 Entity 共享同一份 Mesh、Texture 和音效，只保存 Handle；程序退出时资源按正确顺序全部释放。

**能回答**：

- Handle 和指针有什么区别？
- 谁拥有资源，谁只是引用？
- GPU 资源为什么不能在 Context 销毁后再删？

**卡在这里就先不要往下读**：退出时崩溃或报 GL 错误，就是释放顺序问题。回到 18.30 对一遍关闭顺序。

## 第四部分：系统整合与验收

### 18.31 一个完整例子：玩家收集物

从文件到画面和声音，完整数据流如下：

#### 加载阶段

1. Asset Manager 加载玩家和收集物 Mesh/Material。
2. Asset Manager 解码收集音效并创建 Audio Buffer。
3. Physics 创建玩家 Capsule Body、地面 Static Body、收集物 Trigger。
4. Game Entity 保存各自 Component 和资源 Handle。

#### 每个固定 Tick

1. 输入被转换为 `PlayerIntent`。
2. Character Controller 根据意图移动玩家。
3. Physics 检测玩家与收集物 Trigger 重叠。
4. Game 收到 Trigger Enter，增加分数并标记收集物 inactive。
5. Game 发送一次 `PlaySoundEvent`。

#### 渲染帧

1. Snapshot 跳过 inactive 收集物。
2. 玩家 Transform 经插值进入 Render Item。
3. Renderer 仍然共享原有 Mesh/Material，不删除资源。
4. UI 显示新的分数。

#### 音频

1. Audio 消费事件，创建一次性 Source。
2. Source 使用共享 Audio Buffer，从收集位置播放。
3. 播放结束后 Source 被回收，Buffer 仍在缓存中。

这个例子展示了一个游戏事件如何同时改变画面、UI 和声音，却不让 Renderer 直接调用 Audio。

---

### 18.32 调试物理

建议实现 Physics Debug Draw：

- 用线框画 Collision Shape。
- 画 Body 的位置和朝向轴。
- 画速度向量。
- 画接触点和法线。
- 用颜色区分 Static、Dynamic、Kinematic 和 Sleeping。
- 显示 Layer、Body ID 和是否 Trigger。

常见症状：

| 症状 | 优先检查 |
| --- | --- |
| 模型与碰撞体错位 | Pivot、导入缩放、Mesh 相对 Body 的局部偏移 |
| 角色穿墙 | 固定步过大、速度过高、Shape 太薄、CCD |
| 物体不断抖动 | Game 与 Physics 同时写 Transform、初始穿透、错误单位 |
| 角色无法落地 | Layer/Mask、地面 Shape、重力方向、Ground Query |
| Trigger 没反应 | 是否标记 Sensor、过滤规则、事件消费顺序 |
| 物体不再移动 | 是否进入 Sleeping、Body 类型是否错误 |

先看 Shape，而不是只看可见 Mesh。

---

### 18.33 调试音频

常见症状：

| 症状 | 优先检查 |
| --- | --- |
| 完全无声 | Device、Master Gain、Source 是否播放、Buffer 是否有效 |
| 声音只响一次后再也不响 | Source 状态和回收池 |
| 左右方向相反 | Listener 坐标系、前/上方向 |
| 离得很近也听不见 | 距离单位、Attenuation、最小/最大距离 |
| 音乐卡顿 | Streaming 队列是否及时补充、解码线程 |
| 爆音或失真 | 多 Source 相加 Clipping、Buffer 格式或采样率错误 |

在调试 UI 显示当前活跃 Source 数、每个 Source 的状态、音量、位置和剩余播放时间。

---

### 18.34 调试资产生命周期

Asset Manager 至少记录：

- Asset ID 和规范化路径。
- 当前状态。
- CPU/GPU/Audio 大小估计。
- 引用或所有权组。
- 加载次数与缓存命中次数。
- 创建和最后使用时间。

可以在退出前输出仍存活资源列表。若本应卸载的关卡资源仍在，说明存在泄漏或策略本来就是常驻，二者必须明确区分。

资源泄漏和“缓存有意保留”不是同一个概念；是否错误取决于设计约定。

---

### 18.35 常见误解

#### 误解一：看起来一样，碰撞 Shape 就该直接用 Render Mesh

视觉细节和碰撞稳定性目标不同。角色和动态物体优先使用简单 Shape。

#### 误解二：固定时间步之后绝不会穿透

固定步让模拟稳定，但高速物体仍可能需要 CCD 或 Cast。

#### 误解三：播放声音就是每帧重新加载文件

文件通常加载/解码一次，Source 持续推进播放位置。

#### 误解四：删除一个 Entity 就要删除它的 Texture

Entity 通常只持有共享 Handle，资源由 Asset Manager 按统一策略管理。

#### 误解五：RAII 能自动解决所有生命周期问题

RAII 解决拥有者析构时释放；所有权是谁、Context 是否仍有效、GPU 是否完成仍需设计。

#### 误解六：异步加载一定更高级

异步能减少卡顿，也显著增加状态和同步复杂度。小关卡先同步加载完全合理。

---

### 18.36 建议实现顺序

**第一阶段：三个系统各自工作**

1. 统一世界单位、up/forward 方向和导入尺度。
2. 接入物理库，创建 Static Ground 和一个 Dynamic Box。
3. 显示 Collision Shape Debug Draw。
4. 接入音频库，播放一个预加载的短音效。
5. 建立常驻 Asset Manager 缓存，让多个对象共享 Mesh/Texture/Sound。
6. 加入 Placeholder、明确错误日志和退出释放顺序。

**第二阶段：组合成收集物玩法**

7. 创建 Capsule 角色控制器，接收 `PlayerIntent`。
8. 加入 Collectible Trigger 和 Layer/Mask。
9. 用 Game Event 播放空间收集音效，并确认只触发一次。

**第三阶段：有实际需求再扩展**

10. 音乐 Streaming、场景卸载、Generation Handle、延迟删除或异步加载每次只选择一项实现和验证。

---

### 18.37 练习

#### 练习 A：Body 类型

创建 Static Ground、Dynamic Box 和 Kinematic Platform。观察谁能被力推动，谁由代码移动，谁影响其他 Body。

#### 练习 B：Shape 与 Mesh

给复杂模型分别使用 Triangle Mesh Collider 和简单 Box/Convex Shape，记录碰撞稳定性和性能差异。Dynamic 对象不要直接使用凹 Triangle Mesh。

#### 练习 C：Tunneling

让一个小球高速穿过薄墙，重现穿透。依次尝试减速、加厚墙、减小 fixed delta、开启 CCD，并记录结果。

#### 练习 D：Trigger 收集物

玩家进入 Trigger 后只触发一次得分和声音；停留其中不能每个 Tick 重复得分。

#### 练习 E：共享资源

创建 100 个相同物体。验证 GPU 只有一份 Mesh 和 Texture，每个实例只有独立 Transform/Body。

#### 练习 F：故意出错实验（加载失败）

故意改错一个 Texture 路径。程序应输出完整错误并显示 Placeholder，而不是崩溃或静默黑色。

#### 练习 G：退出检查

记录所有 GPU 和 Audio Resource 的创建/销毁数量。程序退出时，拥有者释放数量应配对，且释放发生在 Context/Device 销毁之前。

---

### 18.38 本章验收标准
最低验收：

- 世界单位、坐标轴和导入尺度有明确文档。
- Static Ground 与 Dynamic Box 的职责正确，能解释 Kinematic 与它们的区别。
- Render Mesh 与 Collision Shape 分离，并可视化调试。
- Physics 只在固定步更新，Dynamic Transform 由 Physics 作为权威来源。
- 一个短音效在启动时加载一次，可以反复播放，Source 播放结束后会回收。
- 多个 Entity 能共享 Mesh、Texture 和 Audio Buffer。
- Asset 加载状态、失败日志和 Placeholder 明确。
- 程序退出时先释放 GPU/Audio Resource，再销毁 Context/Device。

推荐的收集物整合验收：

- 玩家通过 Character Controller 或受控物理接口移动，不直接瞬移 Mesh。
- Trigger、Layer/Mask 和至少一种 Ray/Shape Query 正常工作。
- 收集事件同时更新 Game State、UI、可见性和一次性音效，但系统互不直接依赖内部实现。

音乐 Streaming、异步加载和运行中卸载资源不属于进入第 19 章的前置条件。选择实现时，再为所选扩展单独增加状态、失败路径和生命周期验收。

你还应能用自己的话回答：

1. Body、Shape、Mesh 和 Transform 分别是什么？
2. Broad Phase、Narrow Phase 和 Solver 各自做什么？
3. Trigger 为什么不会把玩家推开？
4. Audio Source 和 Audio Buffer 有什么区别？
5. CPU Image 上传后为什么可以释放，但 Texture 仍然存在？
6. Handle 为什么不等于资源所有权？

完成本章后，你已经拥有一个小型可玩场景。下一章不再凭感觉说“快或慢”，而是学习怎样测量它。
