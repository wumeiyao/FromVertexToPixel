# 第 10 章：场景、相机与可见性

<!-- nav -->
> [上一章：网格、贴图与 glTF 资产](09-mesh-model-assets.md) ｜ [下一章：颜色空间、材质与基础光照](11-color-material-lighting.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/04_scene_camera_assets/10-scene-camera-culling.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

上一章已经能把一个模型文件转换成 Mesh、Material 和 Texture。本章开始处理“世界里有很多对象”的情况：谁负责保存对象，父子变换怎样传播，相机怎样决定从哪里看，以及哪些对象根本不必提交给 GPU。

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 解释为什么游戏对象不能直接交给 Renderer 绘制。
2. 区分 Local Transform 和 World Transform，并让父子层级正确传播。
3. 说明 RenderItem 里为什么只放 Mesh、Material、矩阵和 Bounds。
4. 列出 Camera 需要保存的数据，并解释 View Matrix 与相机 World Transform 的关系。
5. 用平面方程和有符号距离判断一个包围球在视锥内、外还是相交。
6. 从 ViewProjection 矩阵提取六个视锥平面，并把 Local Bounds 变换到 World Space。
7. 说明 Frustum Culling 为什么不能替代 Depth Test，以及为什么宁可多提交也不能漏提交。

本章分三部分：先把游戏对象转换成 Renderer 可读的 RenderItem，再用 Camera Frustum 筛选，最后生成本帧队列并调试结果。

## 第一部分：从场景对象得到 RenderItem

### 10.1 游戏对象不等于可以直接绘制的对象

一个游戏角色可能包含很多信息：

~~~text
生命值
玩家输入
AI 状态
碰撞体
音频
动画
Mesh 与 Material
世界位置
~~~

Renderer 不需要知道生命值，也不需要知道这个对象是不是敌人。它只关心绘制所需的数据：

~~~text
画哪个 Mesh
使用哪个 Material
放在世界哪里
属于哪个渲染 Layer
用什么包围体做可见性测试
~~~

因此应该把游戏世界与渲染世界分开。

~~~text
游戏对象              提取本帧              RenderItem
逻辑、物理、动画  -->  渲染数据      -->  Mesh、Material、矩阵、Bounds
                                              |
                                              v
                                       Camera 过滤与裁剪
                                              |
                                              v
                                        Render Queue
                                              |
                                              v
                                   Renderer 提交 Draw Call
~~~

这种分离不是为了追求复杂架构，而是为了让每一层只回答自己的问题。

---

### 10.2 Scene、Entity、Component 和 Transform

不同项目使用的名字不同，但概念通常相似。

#### 10.2.1 Scene：场景

Scene 是一组共同存在于当前世界中的对象。例如主菜单、第一关、测试房间可以是不同 Scene。

#### 10.2.2 Entity 或 GameObject：对象身份

Entity/GameObject 表示世界中的一个对象。它可以只有一个编号和名字，本身不必保存所有功能。

#### 10.2.3 Component：组件

Component 是附在对象上的一项功能或数据，例如：

~~~text
TransformComponent
MeshRendererComponent
CameraComponent
LightComponent
ColliderComponent
~~~

你的学习项目不必立刻实现完整 ECS。ECS 是 Entity Component System，实体-组件-系统：用 Entity 表示身份、Component 保存数据、System 批量处理某类数据。用普通 C++ `struct` 和数组也可以。真正重要的是：渲染器不要直接依赖所有游戏逻辑类型。

#### 10.2.4 Transform：位置、旋转与缩放

Transform 描述对象在空间中的放置方式。本章继续使用第 2 章已经学过的矩阵，不额外引入四元数：

~~~cpp
struct Transform {
    Vec3 position;
    Mat4 rotation;
    Vec3 scale;
};
~~~

`rotation` 是一个已经构造好的旋转矩阵。以后可以改用 Quaternion 保存旋转，但那只是旋转数据的另一种表示，不改变本章的场景提交和裁剪流程。

通常会把它们组合成 Model Matrix：

~~~text
M = T * R * S
~~~

顶点经过 `M` 后，从 Local Space 进入 World Space。

---

### 10.3 Local Transform 与 World Transform

Local Transform 是“相对于父节点”的变换；World Transform 是“相对于整个世界原点”的最终变换。

例如车轮相对汽车的位置是：

~~~text
汽车世界位置：       (10, 0, 5)
车轮相对汽车的位置： ( 1, 0, 1)
~~~

若忽略旋转和缩放，车轮世界位置大致是 `(11, 0, 6)`。有旋转和缩放时不能简单相加，要使用矩阵：

~~~text
M_world_child = M_world_parent * M_local_child
~~~

层级示意：

~~~text
World
└── Car                 world = car.local
    ├── Body            world = car.world * body.local
    └── Wheel           world = car.world * wheel.local
        └── WheelCap    world = wheel.world * cap.local
~~~

递归更新：

~~~cpp
void updateWorldTransform(Node& node, const Mat4& parentWorld) {
    node.world = parentWorld * node.local;

    for (Node& child : node.children) {
        updateWorldTransform(child, node.world);
    }
}
~~~

这段代码的意图是：父节点的最终结果先算好，再传给每个子节点。

大型场景可以用 dirty flag，也就是“数据是否已经改变、需要重新计算”的布尔标记，只在 Transform 改变时更新。但第一版先每帧更新，保证逻辑正确。

---

### 10.4 RenderItem：给 Renderer 的最小快照

RenderItem 是“一次可能被绘制的对象描述”。它不是 OpenGL 对象，也不是完整 GameObject。

~~~cpp
struct Sphere {
    Vec3 center;
    float radius;
};

struct RenderItem {
    MeshHandle mesh;
    MaterialHandle material;
    Mat4 modelMatrix;
    Sphere worldBounds;
    std::uint32_t layerMask;
};
~~~

逐项说明：

- `mesh`：要绘制的几何资源编号。
- `material`：要使用的材质资源编号。
- `modelMatrix`：把 Mesh 顶点从 Local Space 变到 World Space。
- `worldBounds`：已经位于 World Space 的包围体，用来快速判断可见性。
- `layerMask`：对象属于哪些可筛选类别。

为什么用 Handle，而不直接保存很大的 Mesh 对象？因为 Handle 很小，适合复制和排序；真正资源由 ResourceManager 统一拥有。

RenderItem 也可以叫 RenderProxy 或 DrawItem。名字不是重点，边界才是重点：Renderer 收到它后，不应再询问对象的生命值、AI 或碰撞体。

---

#### 本部分检查点

**能做出来**：父对象移动时，子对象跟着走；Renderer 只拿到 RenderItem，不认识游戏逻辑类型。

**能回答**：

- Local Transform 和 World Transform 的区别？
- RenderItem 里为什么要放 Bounds？
- 为什么不让 Renderer 直接遍历游戏对象？

**卡在这里就先不要往下读**：父子变换错位通常是乘法顺序问题。回到第 2 章 2.15 确认一次顺序，再继续。

## 第二部分：Camera 与可见性测试

### 10.5 Camera 到底保存什么

现实相机有位置、朝向和镜头；渲染相机还要明确输出区域和过滤规则。

~~~cpp
struct Camera {
    Mat4 view;
    Mat4 projection;
    Rect viewport;
    std::uint32_t cullingMask;
    FramebufferHandle target;
};
~~~

这里的 `Rect` 是一个矩形结构，保存 `x、y、width、height`；`FramebufferHandle` 是输出目标的资源编号。

#### 10.5.1 View Matrix：从相机角度看世界

View Matrix 把 World Space 位置转换到 View Space，也叫 Camera Space。

可以把它理解成相机 World Transform 的逆：

~~~text
V = inverse(M_camera_world)
~~~

相机向右移动，画面中的世界看起来向左移动，正是因为 View Matrix 对世界施加了相反变换。

#### 10.5.2 Projection Matrix：怎样投影

Projection Matrix 把 View Space 映射到 Clip Space。

透视相机需要：

- 垂直视野角 FOV。
- 宽高比 aspect = width / height。
- near plane：最近可见距离。
- far plane：最远可见距离。

正交相机不产生“近大远小”，通常使用 left、right、bottom、top、near、far 定义范围。

#### 10.5.3 ViewProjection Matrix

常把两个矩阵预先组合：

~~~text
VP = P * V
clipPosition = VP * worldPosition
~~~

它既用于顶点投影，也可以用于构造视锥裁剪平面。

#### 10.5.4 Viewport：画到目标的哪个矩形

Viewport 是 Framebuffer 中用于绘制的矩形区域，例如：

~~~text
x = 0, y = 0, width = 1280, height = 720
~~~

分屏游戏可以让两台 Camera 使用同一 Framebuffer 的不同 Viewport。

#### 10.5.5 Culling Mask：哪些 Layer 可以被这台相机看见

Layer 是程序定义的类别位。例如：

~~~text
bit 0：World
bit 1：Player
bit 2：Effects
bit 3：UI
~~~

若对象与相机至少有一个共同位，就允许继续处理：

~~~cpp
bool layerVisible = (item.layerMask & camera.cullingMask) != 0;
~~~

Layer 过滤是人为规则，不是空间计算。例如小地图 Camera 可以看见 World 和 Player，却不看见第一人称武器。

#### 10.5.6 Target：画到哪里

Camera Target 可以是窗口默认 Framebuffer，也可以是自建的离屏 Framebuffer。第 15 章会详细实现离屏输出。

---

### 10.6 什么叫“可见”

在渲染中，“可见”至少有四层含义：

| 层次 | 问题 | 由谁处理 |
| --- | --- | --- |
| Layer 可见 | 规则上允许这台 Camera 画吗？ | CPU Mask 过滤 |
| 视锥可见 | 对象是否可能落入相机视野？ | CPU Frustum Culling |
| 未被遮挡 | 是否被墙或其他物体挡住？ | GPU Depth Test，或更高级 Occlusion Culling |
| 产生最终颜色 | 片段是否通过深度、Alpha、混合等规则？ | GPU 输出合并阶段 |

本章实现的是前两层。深度测试在前面已经学习；Occlusion Culling 属于后续优化，不应与 Frustum Culling 混淆。

---

### 10.7 Frustum：相机能看见的空间体积

透视相机的可见区域像切掉尖端的金字塔，英文叫 View Frustum，中文常叫视锥体。

~~~text
              far plane
          +---------------+
         /                 \
        /                   \
       +---------------------+
        \                   /
         \                 /
          +---------------+
              near plane
                  ^
                Camera
~~~

它由六个平面围成：

~~~text
left, right, bottom, top, near, far
~~~

若一个对象完全位于任意一个平面的外侧，它不可能出现在屏幕上，可以不提交 Draw Call。

注意“完全位于外侧”。只要对象与视锥相交，就应保守地认为它可能可见。

---

### 10.8 为什么不用每个 Triangle 做裁剪测试

一个角色可能有十万个 Triangle。若 CPU 为每个对象逐三角形测试，裁剪本身就会非常昂贵。

解决办法是用简单形状包住整个 Mesh，称为 Bounding Volume，包围体。

#### 10.8.1 Bounding Sphere：包围球

只保存中心和半径：

~~~text
Sphere = center + radius
~~~

优点：

- 测试计算少。
- 对象旋转后仍然是球。
- 很容易从 Local Space 转到 World Space。

缺点：细长物体会被包得很松，可能多提交一些实际看不见的区域。

#### 10.8.2 AABB：轴对齐包围盒

AABB 是 Axis-Aligned Bounding Box，边始终平行于当前坐标空间的 x、y、z 轴。常用 `min/max` 或 `center/extents` 表示；`extents` 是盒子沿 x、y、z 三个方向的半尺寸。

它通常比球更贴合箱形物体，但对象旋转后要重新计算 World AABB。

#### 10.8.3 OBB：有方向包围盒

OBB 会跟随对象旋转，更贴合模型，但测试更复杂。第一版不需要实现。

学习项目先使用包围球。它可能产生“假阳性”，也就是认为某对象可见但实际没贡献 Pixel；这只会多画一些，不会让应该出现的物体消失。

---

### 10.9 平面方程与有符号距离

要测试球是否在视锥平面外，先理解 Plane 的表示：

~~~text
plane: n dot p + d = 0
~~~

符号含义：

- `n`：平面的单位法线，方向朝向视锥内部。
- `p`：平面上的任意点。
- `d`：平面相对原点的偏移。
- `dot`：点积。

对空间中的测试点 `x`：

~~~text
signedDistance = n dot x + d
~~~

当 `n` 已归一化时：

- 距离大于 0：点在法线指向的一侧，也就是视锥内侧。
- 距离等于 0：点在平面上。
- 距离小于 0：点在外侧。

#### 10.9.1 数字例子

平面 `x = 2`，法线朝正 x，可写成：

~~~text
n = (1, 0, 0)
d = -2
~~~

测试点 `(5, 1, 0)`：

~~~text
distance = (1,0,0) dot (5,1,0) - 2
         = 5 - 2
         = 3
~~~

它位于法线侧，距离平面 3 个单位。

测试点 `(1, 0, 0)`：

~~~text
distance = 1 - 2 = -1
~~~

它位于另一侧。

---

### 10.10 包围球与一个平面的测试

假设球中心到平面的有符号距离是 `s`，球半径是 `r`。

~~~text
s < -r  -> 整个球都在外侧，可以裁掉
s >  r  -> 整个球都在内侧
其他    -> 球与平面相交，必须保留
~~~

代码：

~~~cpp
struct Plane {
    Vec3 normal;
    float d;
};

bool sphereOutsidePlane(const Sphere& sphere, const Plane& plane) {
    float distance = dot(plane.normal, sphere.center) + plane.d;
    return distance < -sphere.radius;
}
~~~

为什么是 `-radius` 而不是 `0`？因为中心虽然可能在平面外，球的一部分仍可能伸进视锥。只有中心已经离外侧超过整个半径，才能确认整个球都在外面。

对六个平面逐一测试：

~~~cpp
bool visible(const Sphere& sphere, const Frustum& frustum) {
    for (const Plane& plane : frustum.planes) {
        if (sphereOutsidePlane(sphere, plane)) {
            return false;
        }
    }
    return true;
}
~~~

只要在一个平面外就能立即返回，这叫 early out，提前结束。

---

### 10.11 从 ViewProjection 提取六个平面

OpenGL Clip Space 满足：

~~~text
-w <= x <= w
-w <= y <= w
-w <= z <= w
~~~

设 `VP` 的四行是 `r0`、`r1`、`r2`、`r3`，并且使用：

~~~text
clip = VP * worldPosition
~~~

那么可构造：

~~~text
left   = r3 + r0
right  = r3 - r0
bottom = r3 + r1
top    = r3 - r1
near   = r3 + r2
far    = r3 - r2
~~~

每个结果包含 `(normal.x, normal.y, normal.z, d)`。随后必须归一化：

~~~cpp
void normalizePlane(Plane& plane) {
    float length = lengthOf(plane.normal);
    plane.normal /= length;
    plane.d /= length;
}
~~~

“矩阵的行”与“内存中连续存储的方向”不是一回事。GLM 常以列主序存储，所以不要直接假设 `matrix[0]` 是第一行。应使用数学库提供的方法正确取行，或把算法写成经过测试的函数。

若你的项目使用不同 Clip Space 深度范围、行向量或反向 Z，这些公式会改变。最可靠的验证方式不是盯着公式，而是绘制视锥线框并用已知测试点检查每个平面法线是否朝内。

---

### 10.12 把 Local Bounds 变到 World Space

模型加载时可以计算 Local Sphere。对象移动后，裁剪必须使用 World Sphere。

球中心按位置变换：

~~~text
worldCenter = M * localCenter
~~~

半径在统一缩放时：

~~~text
worldRadius = localRadius * abs(scale)
~~~

非均匀缩放 `(sx, sy, sz)` 时，为了保守包住物体，可取最大轴缩放：

~~~text
worldRadius = localRadius * max(abs(sx), abs(sy), abs(sz))
~~~

这会让球略松，但不会错误裁掉模型。

~~~cpp
Sphere transformSphere(const Sphere& local, const Mat4& model) {
    Sphere world;
    world.center = transformPoint(model, local.center);

    float sx = lengthOf(model.columnX().xyz);
    float sy = lengthOf(model.columnY().xyz);
    float sz = lengthOf(model.columnZ().xyz);
    world.radius = local.radius * max(sx, max(sy, sz));
    return world;
}
~~~

这里通过 Model Matrix 三个基向量的长度取得各轴缩放。实际函数名取决于你的数学库。

这个算法假设对象只使用平移、旋转和缩放，没有 Shear（剪切）。学习项目先采用这一约束；若以后允许复杂父子非均匀缩放，应改用更保守的 Bounds 算法。现在不要让这个扩展问题打断包围球裁剪主线。

---

#### 本部分检查点

**能做出来**：手算一次：给定一个平面和一个包围球，判断球在平面哪一侧、是否相交。

**能回答**：

- 有符号距离的正负表示什么？
- 为什么球心在某个平面外侧，还不能立刻裁掉它？
- 六个视锥平面是从哪个矩阵里提取的？

**卡在这里就先不要往下读**：符号搞反会让可见物体凭空消失。先用一个肯定在视锥正中间的物体验证“不被裁掉”。

## 第三部分：生成队列并验证裁剪

### 10.13 本帧可见对象怎样形成队列

一个简单、正确的场景提交过程可以是：

~~~cpp
RenderStats collectVisibleItems(
    const Scene& scene,
    const Camera& camera,
    std::vector<RenderItem>& output)
{
    RenderStats stats{};
    Frustum frustum = makeFrustum(camera.projection * camera.view);

    for (const Renderable& object : scene.renderables) {
        ++stats.total;

        if ((object.layerMask & camera.cullingMask) == 0) {
            ++stats.layerRejected;
            continue;
        }

        Sphere worldBounds = transformSphere(
            object.mesh.localBounds,
            object.worldMatrix
        );

        if (!visible(worldBounds, frustum)) {
            ++stats.frustumCulled;
            continue;
        }

        output.push_back(RenderItem{
            object.mesh,
            object.material,
            object.worldMatrix,
            worldBounds,
            object.layerMask
        });
        ++stats.submitted;
    }

    return stats;
}
~~~

这里没有调用任何 OpenGL API。它只生成本帧快照，因此可以写普通单元测试。

完整数据流：

~~~text
游戏逻辑更新 Transform
    -> 更新 World Matrix
    -> 更新 World Bounds
    -> Layer 过滤
    -> Frustum Culling
    -> 生成 RenderItem
    -> 分入 Render Queue
    -> Renderer 才开始调用 OpenGL
~~~

---

### 10.14 Frustum Culling、Depth Test 与 Occlusion Culling

这三个机制解决不同问题。

#### 10.14.1 Frustum Culling

问题：“对象是否完全在相机视野外？”

在 CPU 提交前完成。成功裁掉后，连 Draw Call 都不会发生。

#### 10.14.2 Depth Test

问题：“这个 Fragment 是否被更近的 Fragment 挡住？”

在 GPU 光栅化后发生。墙后的对象可能已经经过顶点处理和光栅化，最终才因深度测试丢弃 Fragment。

#### 10.14.3 Occlusion Culling

问题：“对象虽然在视锥内，但是否完全被其他对象挡住？”

它需要额外空间结构、深度金字塔或查询机制，比 Frustum Culling 复杂。第一版不要实现。

~~~text
视锥外的房子：Frustum Culling 直接不提交
视锥内、墙后的箱子：先提交，Depth Test 挡住
高级方案确认箱子完全被墙挡住：Occlusion Culling 可提前跳过
~~~

---

### 10.15 相机参数变化时要更新什么

相机移动或旋转：

~~~text
更新 camera world transform
-> 重新计算 view
-> 重新计算 viewProjection
-> 重新构造 frustum
~~~

窗口 Resize：

~~~text
更新 viewport width/height
-> 更新 aspect ratio
-> 重新计算 projection
-> 重新计算 viewProjection 与 frustum
~~~

若只改 OpenGL Viewport 却不改 aspect，画面会横向或纵向拉伸。若只改 Projection 却继续使用旧 Frustum，边缘物体可能错误消失。

---

### 10.16 可见性调试工具

裁剪 Bug 最危险的表现是“对象偶尔闪没”。不要靠猜，至少加入以下工具。

#### 10.16.1 统计计数

每帧显示：

~~~text
total = 1000
layer rejected = 120
frustum culled = 650
submitted = 230
~~~

应满足：

~~~text
total = layerRejected + frustumCulled + submitted
~~~

#### 10.16.2 绘制 Bounds

用线框绘制包围球或 AABB：

- 绿色：通过裁剪。
- 红色：被裁剪。
- 黄色：与视锥边缘相交。

#### 10.16.3 绘制 Frustum

从另一台调试 Camera 看主 Camera 的六个平面和八个角。这样能直观看见 near/far、FOV 和 aspect 是否正确。

#### 10.16.4 冻结裁剪相机

按键冻结用于裁剪的 ViewProjection，但允许调试相机继续移动。这样能从外面观察哪些 Bounds 被固定视锥接收或拒绝。

---

### 10.17 常见错误

| 现象 | 可能原因 | 检查办法 |
| --- | --- | --- |
| 屏幕中的对象突然消失 | 平面法线反向、bounds 太小 | 关闭裁剪并画 bounds |
| 所有对象都被裁掉 | ViewProjection 顺序或平面提取错误 | 用原点球测试六个 signed distance |
| 裁剪完全没有效果 | World bounds 未更新或半径过大 | 显示 bounds 数值与线框 |
| 旋转后对象边缘消失 | 直接缩放球心但半径没按最大轴缩放 | 检查 world radius |
| Resize 后边缘错误 | Projection 更新了，Frustum 没更新 | 记录矩阵版本号 |
| UI 被 3D Camera 画出 | Layer 和 Culling Mask 配置错误 | 以二进制打印 Mask |
| 父节点移动但子 Mesh 不动 | World Matrix 没递归传播 | 绘制节点坐标轴 |
| 裁剪开启后画面与关闭时不同 | 算法不保守，有可见对象被误删 | 逐对象记录被哪个平面拒绝 |

不要用“给包围球乘 10”长期掩盖错误。放大 Bounds 可以用于确认问题确实来自裁剪，但最终仍应修正坐标空间或变换。

---

### 10.18 练习

#### 练习 A：层级变换

创建父对象 Car 和子对象 Wheel。分别移动、旋转父对象，打印两者 World Matrix，并画出局部坐标轴。解释为什么子对象会跟随父对象。

#### 练习 B：手算平面距离

给定平面：

~~~text
n = (0, 1, 0)
d = -2
~~~

分别计算中心在 `(0,5,0)`、`(0,1.5,0)`、半径为 `1` 的球与平面的关系：完全内侧、相交还是完全外侧。

#### 练习 C：1000 个对象

在网格中放置 1000 个立方体。显示 `total`、`frustumCulled` 和 `submitted`。移动相机，观察计数变化。

#### 练习 D：故意出错实验（破坏裁剪）

分别制造并截图：

1. 忘记归一化 Plane。
2. 把某个 Plane normal 反向。
3. 使用 Local Bounds 直接测试 World Frustum。

写下每种错误的画面表现和定位方法。

---

### 10.19 本章验收标准

**最低验收**（第一次必须完成）：下面第 1、2、5、6 条。也就是——父子变换正确、Renderer 只认 RenderItem、视锥外对象不再提交，而且**开关裁剪时画面完全一致**。最后这一条最重要：它证明你的裁剪是保守的，没有误删可见对象。

**推荐验收**：第 3、4、7、8 条。

**扩展验收**（可留到第二遍）：包围盒替代包围球、层次化裁剪、Occlusion Culling。

完整清单——本章完成时，你的场景应满足：

1. 父子 Node 的 World Transform 正确传播。
2. Renderer 只接收 RenderItem，不依赖游戏逻辑类型。
3. Camera Resize 后 Viewport、Projection 和 Frustum 同步更新。
4. Layer Mask 能明确过滤一类对象。
5. 包围球 Frustum Culling 能让视锥外对象不再提交。
6. 关闭与开启裁剪时，最终画面完全一致。
7. 开启裁剪后，submitted 或 Draw Call 数量在合适场景中明显减少。
8. 可以绘制 Bounds、Frustum，并显示裁剪统计。

进入下一章前，请确认自己能解释：

- Local Transform 与 World Transform 有什么区别？
- View Matrix 为什么近似是相机 World Transform 的逆？
- Viewport 与 Projection 各解决什么问题？
- 为什么球心在平面外不一定能立刻裁掉整个球？
- Frustum Culling 为什么不能替代 Depth Test？
- 为什么裁剪宁可多提交一点，也不能少提交可见对象？

下一章将处理已经可见的表面怎样得到正确颜色：颜色空间、材质参数、法线与基础光照。
