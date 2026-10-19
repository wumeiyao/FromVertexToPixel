# 第 17 章：游戏循环、时间与渲染快照

<!-- nav -->
> [上一章：UI 与图形调试工具](16-ui-debug-tools.md) ｜ [下一章：物理、音频与资产生命周期](18-physics-audio-assets.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/08_gameplay/17-game-loop-render-snapshot.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

到第 16 章为止，你已经能画出一帧并观察它。这一章开始回答另一个问题：程序怎样持续接收输入、更新世界，并一帧接一帧地显示运动？

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 区分渲染帧和固定更新（Fixed Tick），并解释为什么两者节奏不同。
2. 用 Accumulator 实现固定步长更新，并说明它为什么要保留余数。
3. 解释插值系数 alpha 从哪里来，以及为什么相机和物体必须用同一个 alpha。
4. 把输入整理成“意图”，并让一次性事件只被消费一次。
5. 说明 Render Snapshot 为什么是只读数据，而不是 Game World 的深拷贝。
6. 解释固定步长为什么能让物理和游戏逻辑在不同机器上结果一致。
7. 指出 VSync、画面撕裂和循环节奏之间的关系。

本章分三部分：先建立稳定的时间循环，再把输入变成游戏状态，最后用只读 Snapshot 把更新结果交给 Renderer。

对应的实现也分三次完成：先让一个物体按“米/秒”稳定移动；再加入固定更新、Accumulator 和插值；最后才整理输入意图与 Render Snapshot。Snapshot 是系统边界的改造，不应与时间循环 Bug 同时调试。

## 第一部分：时间与固定更新

### 17.1 一个游戏程序为什么不能只执行一次

普通命令行程序可以读取数据、计算结果、打印后退出。游戏需要持续响应玩家，因此程序启动后会重复执行一组步骤：

~~~text
读取系统事件
-> 读取玩家输入
-> 推进游戏规则与物理
-> 整理本帧要画的数据
-> Renderer 画一帧
-> 把画面交给显示器
-> 回到开头
~~~

这段重复过程叫 **Game Loop（游戏循环）**。

最简形式是：

~~~cpp
while (!windowShouldClose()) {
    pollEvents();
    updateGame();
    renderFrame();
    swapBuffers();
}
~~~

每行意图：

- `windowShouldClose()`：用户是否关闭窗口。
- `pollEvents()`：让操作系统把键盘、鼠标、窗口 resize 等事件交给程序。
- `updateGame()`：更新角色、敌人、计时器和规则。
- `renderFrame()`：根据当前状态生成图像。
- `swapBuffers()`：把刚画好的 Back Buffer 展示出来。

问题是，这个最简版本没有处理“时间”。不同电脑每秒循环次数不同，游戏速度就可能不同。

---

### 17.2 Frame、刷新率、FPS 和时间单位

先明确几个词：

- **Frame（帧）**：一次完整更新或一次完整渲染得到的画面，需结合上下文判断。
- **FPS（Frames Per Second）**：程序每秒完成多少个渲染帧。
- **Refresh Rate（刷新率）**：显示器每秒最多按其节奏更新多少次，单位 Hz。
- **Frame Time（帧时间）**：一帧花费的时间，常用毫秒 `ms`。
- **Second（秒）**：游戏速度和物理计算最好统一使用的时间单位。

FPS 和帧时间互为倒数：

~~~text
frameTimeSeconds = 1 / FPS
frameTimeMs      = 1000 / FPS

60 FPS  -> 约 16.67 ms
30 FPS  -> 约 33.33 ms
120 FPS -> 约 8.33 ms
~~~

显示器是 120 Hz，不代表游戏一定能达到 120 FPS；游戏达到 300 FPS，也不代表显示器能展示 300 个完整刷新画面。

---

### 17.3 Delta Time：两次更新之间经过了多久

**Delta Time** 常写作 `dt`，表示从上一次更新到这一次更新经过的秒数。

假设角色速度是每秒 3 米：

~~~text
本帧经过 0.016 秒
移动距离 = 3 m/s * 0.016 s = 0.048 m
~~~

代码是：

~~~cpp
position += velocity * deltaTime;
~~~

单位也能帮助检查公式：

~~~text
(米 / 秒) * 秒 = 米
~~~

错误写法是每帧固定移动 `0.05` 米：

~~~cpp
position += 0.05f;
~~~

60 FPS 时一秒移动 3 米；120 FPS 时一秒移动 6 米。角色速度被电脑性能改变了。

---

### 17.4 从可靠的时钟读取时间

游戏需要使用**单调时钟（Monotonic Clock）**。它只向前走，不会因为用户修改系统日期而突然跳跃。

C++ 可以使用 `std::chrono::steady_clock`：

~~~cpp
using Clock = std::chrono::steady_clock;

auto previous = Clock::now();

while (!windowShouldClose()) {
    auto current = Clock::now();
    std::chrono::duration<double> elapsed = current - previous;
    previous = current;

    double frameDelta = elapsed.count(); // 单位为秒
    // ...
}
~~~

不要混用毫秒和秒。若速度单位是米/秒，却把 `16.67 ms` 当成 `16.67 s` 使用，物体会瞬间飞走。

建议在引擎内部统一用秒，并让变量名说明含义，例如 `frameDeltaSeconds`。

---

### 17.5 可变时间步：简单，但结果会随帧率变化

最直观做法是每个渲染帧调用一次更新，并传入本帧 Delta Time：

~~~cpp
while (running) {
    double dt = measureFrameDelta();
    pollEvents();
    game.update(dt);
    renderer.render(game);
    present();
}
~~~

它适合相机平滑、UI 动画和简单视觉效果，但不适合稳定物理。

原因是很多模拟采用数值近似。步长越大，误差越大。两个相同的 0.5 秒过程：

~~~text
方案 A：执行 30 次，每次 dt = 1/60 秒
方案 B：执行 1 次，dt = 0.5 秒
~~~

它们的碰撞、弹簧和高速运动结果可能完全不同。B 甚至可能让物体一次跨过墙壁。

---

### 17.6 速度、加速度和最简单的积分

为了理解固定时间步，先看最小运动公式。

- **位置（Position）**：物体在哪里，单位可为米。
- **速度（Velocity）**：位置变化有多快，单位米/秒。
- **加速度（Acceleration）**：速度变化有多快，单位米/秒平方。
- **积分（Integration）**：根据当前变化率，近似计算一小段时间后的状态。

最简单的半隐式 Euler 更新：

~~~cpp
velocity += acceleration * dt;
position += velocity * dt;
~~~

同一总时间使用不同 `dt`，误差可能不同。固定 `dt` 不能让近似变成绝对精确，但它让误差模式稳定、调试可重复。

---

### 17.7 固定时间步

**Fixed Time Step（固定时间步）**表示游戏模拟每次都用相同的时间长度，例如 `1/60` 秒。

~~~text
fixedDelta = 1 / 60 秒 = 约 0.0166667 秒
~~~

显示帧率可以是 30、60、120 或暂时波动，模拟仍按一格一格的固定时间推进。

但真实一帧可能不是刚好 `fixedDelta`。因此需要一个 **Accumulator（累加器）** 保存“已经经过、但还没有被模拟消耗”的时间。

~~~cpp
constexpr double fixedDelta = 1.0 / 60.0;
double accumulator = 0.0;

while (running) {
    double frameDelta = measureFrameDelta();
    accumulator += frameDelta;

    while (accumulator >= fixedDelta) {
        fixedUpdate(fixedDelta);
        accumulator -= fixedDelta;
    }

    render();
}
~~~

`while` 而不是 `if` 很重要：一个较慢的渲染帧可能跨过多个固定步，需要补做多次模拟。

---

### 17.8 手算 Accumulator

设固定步长为 `10 ms`，为了方便计算。

第一帧真实经过 `16 ms`：

~~~text
accumulator = 0 + 16 = 16 ms
执行 1 次 fixedUpdate，消耗 10 ms
accumulator 剩 6 ms
~~~

第二帧真实经过 `7 ms`：

~~~text
accumulator = 6 + 7 = 13 ms
执行 1 次 fixedUpdate，消耗 10 ms
accumulator 剩 3 ms
~~~

第三帧真实经过 `28 ms`：

~~~text
accumulator = 3 + 28 = 31 ms
执行 3 次 fixedUpdate，共消耗 30 ms
accumulator 剩 1 ms
~~~

经过三帧，真实时间共 `51 ms`，模拟已经推进 `50 ms`，还保留 `1 ms` 等待以后凑成完整固定步。

---

### 17.9 为什么固定模拟可能看起来抖动

假设物理每秒只更新 20 次，而显示器每秒显示 120 次。两次物理更新之间有多个渲染帧看到完全相同的位置，于是运动像一小格一小格跳。

解决方式是**渲染插值（Render Interpolation）**：保存上一个物理状态和当前物理状态，按累加器剩余比例画中间位置。

~~~text
previous state ------- rendered state ------- current state
                  alpha = accumulator / fixedDelta
~~~

线性插值公式：

~~~text
renderPosition = previousPosition * (1 - alpha)
               + currentPosition  * alpha
~~~

若 `alpha = 0.25`，渲染位置位于旧状态到新状态的 25% 处。

重要边界：

- 插值只改变本帧**画在哪里**。
- 它不能把插值位置写回 Physics。
- 游戏规则仍只认固定更新产生的真实状态。

旋转不能简单对 Euler 角逐分量插值；使用四元数时通常采用 `slerp` 或规范化的 `nlerp`。初学阶段可以先只插值位置。

---

### 17.10 为什么画面会略落后于最新物理状态

上述插值通常在 `previous` 和 `current` 之间画，因此视觉上大约落后一个固定步。这是为了得到稳定且有两端可插值的数据。

`1/60` 秒约为 `16.67 ms`，多数单机学习项目可以接受。以后做强响应性的网络或竞技游戏时才需要更复杂的预测技术。

不要为了消除这点延迟，直接把渲染插值结果写回物理，否则系统边界会被破坏。

---

### 17.11 卡顿、断点与“死亡螺旋”

程序被断点暂停 5 秒后，时钟会得到巨大 `frameDelta`。若全部加入累加器，循环会尝试补做 300 次 60 Hz 物理更新。补更新本身又花时间，下一帧更慢，欠下更多更新，这叫 **Spiral of Death（死亡螺旋）**。

第一层保护是限制单帧可接受时间：

~~~cpp
frameDelta = std::min(frameDelta, 0.25);
~~~

第二层保护是限制每个渲染帧最多执行多少个固定步：

~~~cpp
int steps = 0;
constexpr int maxStepsPerFrame = 8;

while (accumulator >= fixedDelta && steps < maxStepsPerFrame) {
    fixedUpdate(fixedDelta);
    accumulator -= fixedDelta;
    ++steps;
}

if (steps == maxStepsPerFrame) {
    // 记录警告，并丢弃来不及补做的完整 Tick，只保留不足一个 Tick 的余数。
    accumulator = std::fmod(accumulator, fixedDelta);
}
~~~

`std::fmod(a, b)` 返回浮点除法的余数；例如 `fmod(0.037, 0.010)` 约为 `0.007`。它声明在 `<cmath>` 中。

限制意味着极端卡顿时游戏时间可能暂时变慢，但比永远追不上更可控。这里保留余数是为了让后面的 `alpha = accumulator / fixedDelta` 仍位于 `[0, 1)`。也可以采用其他明确策略，但不能一面停止补步，一面让多个完整 Tick 永久留在 Accumulator 中。要把丢步情况记录到调试面板，而不是静默发生。

---

#### 本部分检查点

**能做出来**：一个物体按“米/秒”匀速移动，把窗口帧率从 60 换到 30，它的移动速度**不变**。

**能回答**：

- Accumulator 里存的是什么，为什么要保留余数？
- 插值系数 alpha 从哪里来，范围是多少？
- 什么是“死亡螺旋”，怎样避免？

**卡在这里就先不要往下读**：帧率变了速度也变，说明还在用 deltaTime 直接驱动逻辑。这一步不过，物理章节会全乱。

## 第二部分：输入与游戏状态

### 17.12 输入事件、输入状态和输入意图

这三个概念不要混在一起。

#### 输入事件（Input Event）

某个时刻发生一次，例如：

~~~text
Space 在这一刻被按下
窗口在这一刻失去焦点
鼠标滚轮滚动了一格
~~~

适合触发“跳跃一次”“打开暂停菜单”。

#### 输入状态（Input State）

当前是否持续成立，例如：

~~~text
W 现在是否按住
鼠标左键现在是否按住
手柄摇杆当前偏移多少
~~~

适合持续移动。

#### 输入意图（Input Intent / Command）

将具体设备转换为游戏含义：

~~~cpp
struct PlayerIntent {
    Vec2 move;       // 期望移动方向，范围约为 -1..1
    bool jumpPressed;
    bool pausePressed;
};
~~~

键盘、手柄、AI 和回放都能产生同一种 `PlayerIntent`。Game 层不必知道意图来自哪个设备。

~~~text
键盘 W / 手柄摇杆 / AI
           |
           v
      PlayerIntent
           |
           v
   Character Controller
~~~

一次性事件还需要**缓冲到下一个固定 Tick 并只消费一次**。假设渲染帧检测到 `jumpPressed`，但这一帧 Accumulator 还不够执行固定更新；若立刻丢弃事件，跳跃会失效。反过来，一个慢帧补做三个固定 Tick 时，也不能让同一次按键跳三次。可以由 `InputBuffer` 保存“尚未消费的按下事件”，第一次 fixed update 取走它，而持续移动方向在每个 Tick 都可读取。

---

### 17.13 为什么不要让按键直接修改 Mesh 位置

错误耦合：

~~~text
W 键 -> 修改 Renderer 中模型矩阵
~~~

这样做会产生问题：碰撞系统不知道角色移动了；AI 不能复用移动规则；回放必须伪造键盘；Renderer 开始承担游戏规则。

更清晰的数据流：

~~~text
输入设备
-> PlayerIntent
-> 游戏/角色控制器计算速度或力
-> Physics 更新真实 Transform
-> 构建 Render Snapshot
-> Renderer 根据 Transform 画 Mesh
~~~

Renderer 只负责“当前应该画在哪里”，不决定“玩家是否有资格移动到那里”。

---

### 17.14 Game State：开始、游玩、暂停和结算

最小游戏也需要明确当前处于什么阶段。可以使用枚举：

~~~cpp
enum class GameState {
    Title,
    Playing,
    Paused,
    Finished
};
~~~

不同状态允许不同更新：

| 状态 | 物理 | 游戏计时 | 3D 渲染 | UI |
| --- | --- | --- | --- | --- |
| Title | 停止 | 停止 | 可选 | 开始菜单 |
| Playing | 运行 | 运行 | 运行 | HUD（Heads-Up Display，游玩时常驻的状态界面） |
| Paused | 停止 | 停止 | 仍可渲染 | 暂停菜单 |
| Finished | 停止或受控 | 停止 | 仍可渲染 | 结算界面 |

**暂停不是停止整个进程。**窗口事件、渲染和 UI 仍要更新，否则菜单无法点击、窗口无法响应。

可以分别维护：

- `realDelta`：真实时间，用于调试 UI 和不受暂停影响的动画。
- `gameDelta`：游戏时间，暂停时为 0 或停止固定模拟。

---

#### 本部分检查点

**能做出来**：按一次跳跃键只跳一次，即使这一帧补做了三个固定 Tick；掉帧时也不会漏掉这次按键。

**能回答**：

- 为什么一次性事件要缓冲到下一个固定 Tick？
- 持续输入和一次性输入的处理方式有什么不同？
- Game State 切换应该发生在渲染帧还是固定 Tick？

**卡在这里就先不要往下读**：跳跃偶尔失效或者连跳，就是事件消费次数不对。先把事件消费打印出来数一遍。

## 第三部分：用 Render Snapshot 连接更新与绘制

### 17.15 Renderer 不应直接遍历整个游戏世界

早期代码可能这样写：

~~~cpp
renderer.render(gameWorld);
~~~

Renderer 于是需要知道 Player、Enemy、PhysicsBody、Collectible 等所有游戏类型。每新增玩法对象，Renderer 都可能修改。

更好的边界是构建 **Render Snapshot（渲染快照）**：游戏更新完成后，把当前帧需要画的数据整理成一份只读交接单。

~~~text
游戏世界：角色、生命值、AI、碰撞、任务
                  |
                  | 提取可见表现数据
                  v
Render Snapshot：相机、Render Item、灯光、UI
                  |
                  v
Renderer：只读并提交 Pass
~~~

这里的“快照”不是把所有 GPU Mesh 复制一遍，而是保存轻量的 Handle、矩阵和参数。

---

### 17.16 Snapshot 中有什么

一个最小设计：

~~~cpp
struct RenderItem {
    MeshHandle mesh;          // 指向已加载 GPU Mesh 的轻量标识
    MaterialHandle material;  // 指向材质的轻量标识
    Mat4 modelMatrix;         // 本帧插值后的模型变换
    Bounds worldBounds;       // 可用于可见性和调试
};

struct CameraData {
    Mat4 view;
    Mat4 projection;
    Vec3 worldPosition;
};

struct LightData {
    Vec3 direction;
    Vec3 color;
    float intensity;
};

struct RenderSnapshot {
    CameraData camera;
    std::vector<RenderItem> opaqueItems;
    std::vector<RenderItem> transparentItems;
    std::vector<LightData> lights;
    UiData ui;
};
~~~

这些类型中的 `Handle` 是资源标识，不是资源所有权。第 18 章会讲资产生命周期。

Snapshot 不应包含：

- Physics 引擎内部 Body 指针。
- 玩家输入设备对象。
- 可让 Renderer 修改生命值的引用。
- 每帧复制的大型 Texture Pixel 数据。

---

### 17.17 “只读”和“不可变”是什么意思

Snapshot 构建完成后，本帧 Renderer 只能读取它。

原因不是追求形式，而是避免这种冲突：

~~~text
Renderer 正在读取对象位置
游戏逻辑同时把对象移动到另一个位置
Renderer 读到一半旧状态、一半新状态
~~~

单线程版本不会真正同时执行，但只读边界仍有价值：

- 数据流更容易理解。
- Renderer 可以独立测试。
- 截图或回放可以保存同一份输入。
- 将来若并行准备渲染，不必重做系统边界。

C++ 可以通过接口表达：

~~~cpp
void Renderer::render(const RenderSnapshot& snapshot);
~~~

`const` 表示这个函数不应修改 Snapshot。

---

### 17.18 构建 Snapshot 的完整数据流

~~~cpp
RenderSnapshot buildSnapshot(const GameWorld& world,
                             double alpha) {
    RenderSnapshot result;

    result.camera = buildCameraData(world.camera, alpha);

    for (const Entity& entity : world.entities) {
        if (!entity.renderable.has_value()) {
            continue;
        }

        RenderItem item;
        item.mesh = entity.renderable->mesh;
        item.material = entity.renderable->material;
        item.modelMatrix = interpolateTransform(
            entity.previousTransform,
            entity.currentTransform,
            alpha
        );
        item.worldBounds = transformBounds(
            entity.renderable->localBounds,
            item.modelMatrix
        );

        if (entity.renderable->transparent) {
            result.transparentItems.push_back(item);
        } else {
            result.opaqueItems.push_back(item);
        }
    }

    result.ui = buildUiData(world);
    return result;
}
~~~

逐段意图：

1. 新建本帧快照。
2. 使用同一个 `alpha` 插值相机和对象，避免两者节奏不同。
3. 跳过没有可视表现的 Entity。
4. 只复制 Mesh/Material Handle 和本帧矩阵。
5. 按不透明与透明队列分类。
6. 从游戏状态生成 UI 数据。

可见性裁剪可以在构建 Snapshot 时完成，也可以由 Renderer 的场景提交阶段完成。第一版只需保持一个明确位置，不要两边重复裁剪。

---

### 17.19 一个完整、可工作的循环骨架

下面把时间、输入、固定更新、插值、Snapshot 和 Renderer 串起来：

~~~cpp
constexpr double fixedDelta = 1.0 / 60.0;
constexpr double maxFrameDelta = 0.25;

double accumulator = 0.0;
auto previousTime = Clock::now();

while (!windowShouldClose()) {
    auto currentTime = Clock::now();
    double frameDelta = seconds(currentTime - previousTime);
    previousTime = currentTime;
    frameDelta = std::min(frameDelta, maxFrameDelta);

    pollWindowEvents();
    InputState input = readInputState();
    inputBuffer.update(input); // 保存持续状态，并缓存尚未消费的一次事件

    if (game.state() == GameState::Playing) {
        accumulator += frameDelta;

        int steps = 0;
        while (accumulator >= fixedDelta && steps < 8) {
            game.savePreviousTransforms();
            PlayerIntent tickIntent = inputBuffer.consumeForFixedTick();
            game.fixedUpdate(tickIntent, fixedDelta);
            accumulator -= fixedDelta;
            ++steps;
        }

        if (steps == 8 && accumulator >= fixedDelta) {
            debugStats.droppedSimulationTime +=
                accumulator - std::fmod(accumulator, fixedDelta);
            accumulator = std::fmod(accumulator, fixedDelta);
        }
    }

    game.updatePresentation(frameDelta); // UI 等非固定表现更新

    double alpha = accumulator / fixedDelta;
    RenderSnapshot snapshot = buildSnapshot(game.world(), alpha);
    renderer.render(snapshot);
    debugUi.draw(game, renderer);
    present();
}
~~~

这不是唯一正确的 Game Loop，但每一段职责明确。初学阶段先让它稳定，再考虑多线程或复杂任务系统。

---

### 17.20 VSync、画面撕裂与循环节奏

**VSync（垂直同步）**通常让 `present` 或 Buffer Swap 等待显示器合适的刷新时刻，以减少 **Tearing（画面撕裂）**。

撕裂是显示器扫描一帧时，程序中途换成了另一张画面，屏幕上下部分来自不同 Frame。

开启 VSync 后循环可能在 `swapBuffers` 等待，因此 FPS 接近刷新率。关闭后程序可能尽快渲染，FPS 更高但可能撕裂。

无论 VSync 是否开启，物理都不应依赖“每次 Buffer Swap 就前进固定距离”。固定模拟与显示节奏必须分开。

---

### 17.21 Determinism：相同输入是否一定得到相同结果

**Determinism（确定性）**表示相同初始状态与相同输入序列总得到相同结果。

固定时间步能改善可重复性，但不自动保证完全确定：

- 不同 CPU 的浮点细节可能不同。
- 容器遍历顺序可能改变碰撞处理顺序。
- 随机数若没有固定种子会不同。
- 物理库本身可能不承诺跨平台逐位一致。

本课程的目标不是实现严格锁步网络，而是让同一台机器上的行为容易重现。记录随机种子、固定步数和输入意图，已经能大幅帮助调试。

---

### 17.22 暂停、单步和慢动作是优秀的调试工具

实现三个调试能力：

- **Pause Simulation**：停止固定更新，但继续渲染和处理 UI。
- **Step One Tick**：暂停时只执行一次 `fixedUpdate`。
- **Time Scale**：控制游戏时间倍率，例如 `0.1` 倍慢动作。

单步可以观察碰撞前后 Transform；慢动作可以检查动画、粒子和摄像机是否使用了正确的时间来源。

不要把 Time Scale 直接乘到所有真实时间上。调试 UI、窗口和某些音频仍可能需要真实时间。

---

### 17.23 常见错误与排查

#### 角色在高刷新率显示器上移动更快

检查是否每帧加固定距离，或者同一输入在 variable update 和 fixed update 中被处理了两次。

#### 物体每隔一小段跳一下

检查是否直接渲染固定状态而没有插值；同时打印本帧 fixed step 次数和 `alpha`。

#### 断点恢复后程序卡很久

检查 `frameDelta` 上限和每帧最大 fixed step 数。

#### 暂停菜单无法操作

很可能暂停了整个循环。应只暂停游戏模拟，继续事件、UI 和渲染。

#### 相机和角色产生相对抖动

检查二者是否使用同一个插值 `alpha`，以及相机跟随是在正确阶段更新。

#### 一按键就连续跳跃

你可能把“按住状态”当成“按下事件”。跳跃通常只消费从未按到按下的边沿事件。

#### Renderer 偶尔读到无效对象

检查 Snapshot 是否保存了短生命周期裸指针；优先使用稳定 Handle 或本帧值数据。

---

### 17.24 建议实现顺序

1. 用 `steady_clock` 测量并显示 `frameDelta`。
2. 先做可变时间移动，观察不乘 Delta Time 的错误。
3. 加入固定步长和 Accumulator。
4. 在调试面板显示 fixed step 次数、Accumulator 和 Alpha。
5. 保存 previous/current Transform，实现位置插值。
6. 将具体键盘输入转换为 `PlayerIntent`。
7. 加入 Title、Playing、Paused、Finished 状态。
8. 构建最小 Render Snapshot，让 Renderer 不再访问 GameWorld。
9. 加入暂停单步与最大积压保护。

---

### 17.25 练习

#### 练习 A：帧率无关移动

让一个物体以每秒 3 米移动 10 秒。分别限制渲染为 30、60、120 FPS，记录最终位置。

#### 练习 B：手算固定步

固定步为 `16 ms`，连续三帧 Delta 为 `10 ms`、`25 ms`、`8 ms`。写出每帧执行次数和剩余 Accumulator。

#### 练习 C：插值可视化

同时画出 previous、interpolated、current 三个位置，用不同颜色的小球表示。调低物理频率到 10 Hz，观察插值前后差异。

#### 练习 D：输入意图

让键盘和手柄都生成相同的 `PlayerIntent`。Game 层不能直接查询 GLFW 键码。

#### 练习 E：暂停与单步

暂停后仍能移动调试窗口、旋转仅用于观察的相机，并可按一次按钮推进一个物理 Tick。

#### 练习 F：Snapshot 边界

让 `Renderer::render` 只接受 `const RenderSnapshot&`。移除 Renderer 对 GameWorld、PhysicsBody 和 Player 类型的包含依赖。

#### 练习 G：故意出错实验（破坏时间循环）

每次只改一处，先预测，再运行，然后恢复：

1. 用 `deltaTime` 直接驱动逻辑，然后把帧率从 60 限制到 20。
2. 固定步补做不设上限，然后在断点处停留几秒再继续。
3. Accumulator 每帧清零，不保留余数。
4. 相机用 `alpha`，物体用 `1 - alpha`。
5. 一次性输入事件在渲染帧就地消费，不缓冲到下一个 Tick。
6. 把固定步长从 1/60 改成 1/5。

第 2 条是“死亡螺旋”：程序会越来越慢直到卡死。第 4 条的数值全部正确，但画面会抖——这说明**插值用错**和**数据算错**是两类完全不同的 Bug。

---

### 17.26 本章验收标准

三组按顺序验收。**时间循环是最低验收**，第一次必须完成；输入与状态是**推荐验收**；Snapshot 边界属于**推荐到扩展**，可以在第二遍再彻底整理。

先验收时间循环（最低）：

- 角色移动使用“每秒单位”，30、60、120 FPS 下 10 秒移动距离基本一致。
- 物理固定以 60 Hz 或你明确选择的频率更新。
- 调试面板显示 Delta、Accumulator、fixed step 次数和 interpolation alpha。
- 大 Delta 和单帧最大 fixed step 有保护与日志。
- previous/current Transform 和渲染插值工作正常。

再验收输入与状态（推荐）：

- 输入设备先转换为意图，再交给游戏逻辑。
- 暂停时游戏模拟停止，但窗口、UI 与渲染继续响应。

最后验收 Snapshot 边界（推荐到扩展）：

- Renderer 只读取不可变 Render Snapshot，不访问 Physics 或玩家脚本。
- Snapshot 中只保存轻量表现数据和稳定资源 Handle。

你还应能用自己的话回答：

1. Delta Time 为什么要用秒？
2. 固定时间步解决了什么，又没有解决什么？
3. Accumulator 剩下的时间代表什么？
4. 插值为什么不能写回 Physics？
5. 输入状态和输入事件有什么区别？
6. Snapshot 为什么不是整个 GameWorld 的深拷贝？

完成这一章后，下一章会把 Physics、Audio 和 Asset 系统接入这条清晰的数据流。
