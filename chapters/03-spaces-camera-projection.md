# 第 3 章：坐标空间、相机、投影与裁剪

<!-- nav -->
> [上一章：向量、矩阵与齐次坐标](02-vectors-matrices.md) ｜ [下一章：CPU Framebuffer 与第一个像素](04-cpu-framebuffer.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/01_math/03-spaces-camera-projection.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

上一章学习了“怎样变换一个点”。本章把多个变换串起来，让模型中的一个三维顶点最终得到屏幕上的二维位置。

渲染中最容易混乱的地方不是公式本身，而是忘了“当前这三个数是相对于谁描述的”。本章会始终为坐标标明所属空间。

## 本章导读

### 3.1 本章学习目标

完成本章后，应能：

1. 用“参考原点和参考坐标轴”解释坐标空间。
2. 逐步区分 Local、World、View、Clip、NDC 与 Screen Space。
3. 解释 View 矩阵为何是 Camera Transform 的逆。
4. 从小孔成像直觉理解透视和远小近大。
5. 解释 `clip.w`、透视除法和齐次裁剪的先后关系。
6. 手算一个顶点从局部坐标到屏幕坐标的全过程。

本章分三部分：先把 Local 坐标送到 Camera 面前，再做 Projection 和屏幕映射，最后完整追踪一个顶点。

## 第一部分：从模型空间到相机空间

### 3.2 坐标没有脱离参考系的绝对含义

假设你说：“杯子在 `(2,1)`。”这句话不完整，因为还不知道：

- `(0,0)` 在哪里？
- x 轴朝哪里？
- 一个单位代表一厘米、一米还是一个格子？

坐标空间（coordinate space）就是一套参考规则，至少包含一个原点和若干坐标轴。同一个地点相对于不同参考系会有不同数字。

生活例子：你坐在行驶的火车里，一只杯子相对桌面没有移动；但相对车站，它正随火车高速移动。杯子没有复制成两份，只是使用了“火车坐标系”和“地面坐标系”两种描述。

在渲染里，一个顶点也会连续改用不同参考系描述：

~~~text
Local -> World -> View -> Clip -> NDC -> Screen
~~~

箭头表示数据发生变换，不表示顶点被复制成新的物体。

### 3.3 先固定本书的坐标约定

不同 API、数学库和图片格式可能采用不同约定。约定本身没有唯一正确答案，但混用一定会出错。本书在 CPU 光栅器与 OpenGL 部分采用：

- 右手三维坐标系。
- 列向量，写作 `result = matrix * vector`。
- View Space 中相机位于原点，朝 `-z` 方向看。
- OpenGL Clip Space 规则：`-w <= x,y,z <= w`。
- OpenGL NDC 的 x、y、z 范围均为 `[-1,1]`。
- CPU Framebuffer 暂定左下角为 `(0,0)`；若图片格式需要左上原点，在输出边界处翻转一次。

右手坐标系可用右手规则理解：x 正方向叉乘 y 正方向得到 z 正方向。

注意：屏幕显示是二维的，不代表三维 z 轴消失了。z 在投影后仍被保存为深度，用于遮挡比较。

### 3.4 Local Space：模型自己的坐标

Local Space 也叫 Object Space 或 Model Space，中文常叫局部空间、对象空间或模型空间。它以模型作者选定的原点和坐标轴描述顶点。

例如一个边长为 2、中心位于自身原点的立方体，顶点可能位于 `(-1,-1,-1)` 到 `(1,1,1)`。这个立方体还没有被放到游戏世界的具体地点。

局部空间解决了复用问题：同一份椅子 Mesh 可以被放到房间的十个位置。顶点数组只保存一次，每把椅子有不同的变换。

~~~text
同一份局部顶点 P=(1,0,0)

椅子 A 的 Model Matrix -> 世界位置 (3,0,0)
椅子 B 的 Model Matrix -> 世界位置 (-5,0,8)
~~~

模型原点会影响旋转。若原点在椅子中心，椅子绕中心旋转；若原点在椅子腿角，它会绕那个角旋转。

### 3.5 World Space：所有物体共享的地图

World Space（世界空间）是关卡中所有对象共同使用的参考系。模型矩阵（Model Matrix）把局部点变成世界点：

~~~text
worldPosition = modelMatrix * localPosition
~~~

模型矩阵通常组合了局部缩放、旋转和世界平移：

~~~text
modelMatrix = translation * rotation * scale
~~~

#### 数值例子

局部顶点 `P=(1,0,0,1)`，模型只向右平移 10：

~~~text
Local P = (1,0,0,1)
World P = (11,0,0,1)
~~~

若复制一个相同模型，只把平移改为 `-4`，同一个局部顶点就会变成世界点 `(-3,0,0,1)`。Mesh 数据没有变化，变化的是 Model Matrix。

### 3.6 Scene Graph 与父子变换

游戏对象常有父子关系，例如轮胎是汽车的子对象。轮胎自己的局部矩阵只描述“相对汽车在哪里”，汽车矩阵再描述“汽车在世界哪里”。

列向量约定下：

~~~text
wheelWorld = carWorld * wheelLocal
pointWorld = wheelWorld * pointWheelLocal
~~~

合并后：

~~~text
pointWorld = carWorld * wheelLocal * pointWheelLocal
~~~

汽车移动时，轮胎会一起移动；轮胎还可在自身局部空间继续旋转。Scene Graph（场景图）就是用父子关系组织这些变换。第一版渲染器可以先不实现完整场景图，但要理解矩阵链为什么自然表达父子关系。

### 3.7 Camera Transform 描述相机在哪里

Camera Transform（相机变换）与普通物体的世界变换含义相同：它描述相机局部原点和局部坐标轴怎样位于世界中。

例如相机世界位置为 `(0,0,5)`，表示相机在世界 z=5 的地点。若它保持默认朝向并朝世界 `-z` 看，世界原点就在相机前方 5 个单位。

然而渲染顶点时，我们需要回答的不是“相机在世界哪里”，而是“这个世界点相对相机在哪里”。因此不能直接把 Camera Transform 乘到世界点上。

### 3.8 View Space：把相机假装固定在原点

View Space 也叫 Camera Space（观察空间或相机空间）。在这个空间中，我们约定：

- 相机位置永远是 `(0,0,0)`。
- 相机右方是 `+x`。
- 相机上方是 `+y`。
- 相机前方是 `-z`。

为了做到这一点，需要对整个世界施加相机运动的反向变换：

~~~text
viewMatrix = inverse(cameraWorldMatrix)
viewPosition = viewMatrix * worldPosition
~~~

#### 为什么是逆矩阵

想象相机向右移动 2 米。屏幕中的所有物体看起来都向左移动 2 米。相机向右的变换是 `+2`，世界相对相机的变换就是 `-2`。

~~~text
世界点 x = 10
相机世界 x = 2
点相对相机 x = 10 - 2 = 8
~~~

相机旋转也同理：相机向左转，相机眼中的世界就相对向右转。View Matrix 不是第二个相机对象，而是把世界坐标改写成相机参考系的矩阵。

### 3.9 用相机基轴理解 LookAt

`lookAt(eye, target, upReference)` 常用于构造 View Matrix：

- `eye`：相机世界位置，是一个点。
- `target`：相机要看的世界位置，是一个点。
- `upReference`：希望大致朝上的参考方向，常用 `(0,1,0)`。

首先得到相机方向。在“相机朝 -z 看”的约定中，可定义相机的后方轴：

~~~text
back = normalize(eye - target)
~~~

再求右方和真正上方：

~~~text
right = normalize(cross(upReference, back))
up    = cross(back, right)
~~~

为什么要重新计算 `up`？传入的 `upReference` 只是参考，它不一定与视线严格垂直。通过两次叉积得到的 `right`、`up`、`back` 两两垂直，形成相机的正交基（orthonormal basis）：三个轴互相垂直且长度均为 1。

若相机正朝世界上方看，`upReference` 与视线平行，第一次叉积会得到零向量。实际相机代码要避免或处理这种退化情况。

View Matrix 做两件事：

1. 用相机的三个基轴衡量世界点在相机右、上、后方向上的分量。
2. 减去相机位置，让 `eye` 自己变为原点。

可用断言验证：

~~~text
viewMatrix * Vec4(eye, 1) 应约等于 (0,0,0,1)
~~~

#### 本部分检查点

**能做出来**：给定一个物体的 Model Matrix 和相机位置，手算它的 World Position 和 View Position。

**能回答**：

- Local、World、View 三个空间分别以谁为参考？
- View Matrix 为什么是相机 World Transform 的逆？
- LookAt 里的三根基轴是怎么用叉积算出来的？

**卡在这里就先不要往下读**：回到 3.9 用具体数字重算一次 LookAt。投影部分假定你已经能把点送进 View Space。

## 第二部分：从相机空间投影到屏幕

### 3.10 Projection 之前：相机看到的是一个视锥体

透视相机可见区域像一个被截去尖端的四棱锥，叫视锥体（view frustum）：

~~~text
                 far plane，远平面
             +-------------------+
            /                   /|
           /                   / |
          +-------------------+  |
           \                 \   |
            \  可见区域       \  |
             +-------------+    \|
               near plane        相机
               近平面
~~~

视锥体由六个平面包围：左、右、上、下、近、远。只有其中的几何体才可能出现在最终画面。

近平面（near plane）是最靠近相机、仍允许显示的距离；远平面（far plane）是最远可见距离。它们必须满足：

~~~text
0 < near < far
~~~

near 不能为 0，因为透视投影靠除法形成远小近大，相机位置处会出现除以零。near 过小还会严重损失深度精度。

### 3.11 先理解正交投影

Orthographic Projection（正交投影）让平行投影线直接穿过空间。物体离相机更远时不会自动变小。

~~~text
近处同样高的物体：||||
远处同样高的物体：||||   屏幕高度仍相同
~~~

正交投影常用于工程视图、策略地图、2D 游戏和 UI。它仍然可以显示三维物体和深度，只是没有透视缩小。

正交投影的核心是把一个长方体范围线性映射到 NDC 的 `[-1,1]`。例如 x 从 `left` 到 `right`：

~~~text
x_ndc = 2 * (x - left) / (right - left) - 1
~~~

验证：

~~~text
x=left  -> x_ndc=-1
x=right -> x_ndc= 1
x 位于正中 -> x_ndc=0
~~~

这个“先减起点、再除范围、最后映射到 -1..1”的形式，之后会在 Viewport 映射中反向出现。

### 3.12 透视投影从相似三角形而来

Perspective Projection（透视投影）产生远小近大的效果。把相机想成一个小孔，观察 x-z 平面：

~~~text
物体点 P(x,z)
      *
      |\
      | \
      |  \
------+---\---------- 相机前方
成像面   相机 O
~~~

在相机朝 `-z` 看且前方点满足 `z<0` 的约定下，通过相似三角形可得投影横坐标：

~~~text
x_projected = f * x / (-z)
y_projected = f * y / (-z)
~~~

- `x,y,z` 是 View Space 点。
- `-z` 是点在相机前方的正距离。
- `f` 是与视野角度有关的缩放量，可理解为成像平面距离。

若两个点 x 相同：

~~~text
P1=(1,0,-2) -> x_projected=f/2
P2=(1,0,-10) -> x_projected=f/10
~~~

P2 更远，投影位置更靠近画面中心，因此物体看起来更小。远小近大不是额外缩放规则，就是除以深度的结果。

### 3.13 FOV 与焦距系数

FOV（field of view，视野角）表示相机能看到多大的角度。常用的是竖直 FOV，记作 `fovY`。

投影矩阵中的焦距系数通常为：

~~~text
f = 1 / tan(fovY / 2)
~~~

为什么是半角？画面中心线把上下对称的视锥分成两半，直角三角形中：

~~~text
tan(fovY/2) = 近平面半高 / 近平面距离
~~~

把标准化后的半高设为 1，就得到倒数关系。

- FOV 增大：`tan(fovY/2)` 增大，f 减小，同一物体投影更小，看到范围更广。
- FOV 减小：f 增大，物体投影更大，像望远镜。

注意把角度转换成弧度后再传给 `tan`。

### 3.14 Aspect Ratio 为什么会影响 x

Aspect Ratio（宽高比）定义为：

~~~text
aspect = viewportWidth / viewportHeight
~~~

例如 1920×1080 的宽高比约为 `1.7778`。若不修正宽高比，同一单位在宽屏的 x、y 方向会被错误地当作相同屏幕长度，圆可能变成椭圆。

当 FOV 是竖直 FOV 时，x 缩放通常使用：

~~~text
xScale = f / aspect
yScale = f
~~~

窗口大小改变后必须用新的宽高比更新 Projection Matrix。

### 3.15 OpenGL 透视矩阵逐项解释

在本书约定下，一个常见 OpenGL 透视矩阵是：

~~~text
P = [ f/aspect   0          0                 0          ]
    [    0       f          0                 0          ]
    [    0       0    -(far+near)/(far-near)  -2far*near/(far-near) ]
    [    0       0         -1                 0          ]
~~~

输入是 View Space 的 `(x,y,z,1)`，输出叫 Clip Coordinate：

~~~text
clip = P * viewPosition
~~~

每部分职责：

- 第一行按 FOV 和宽高比缩放 x。
- 第二行按 FOV 缩放 y。
- 第三行把 View Space 深度映射到 OpenGL 所需深度范围。
- 第四行让 `clip.w = -z`，为透视除法准备距离。

矩阵并没有直接执行除法，因为矩阵乘法只能做乘加。它先把分子放进 `clip.x/y/z`，把分母放进 `clip.w`，之后统一做除法。

不要脱离约定抄 Projection Matrix。Direct3D/Vulkan 的 NDC 深度范围、坐标方向或矩阵约定可能不同，公式也会相应改变。

#### 3.15.1 这张 Projection Matrix 的每个数字做什么

把矩阵中的四个主要系数先命名：

~~~text
A = f / aspect
B = f
C = -(far + near) / (far - near)
D = -2 * far * near / (far - near)

P = [ A  0  0  0 ]
    [ 0  B  0  0 ]
    [ 0  0  C  D ]
    [ 0  0 -1  0 ]
~~~

输入 `view=(x,y,z,1)` 后，逐行相乘：

~~~text
clip.x = A*x + 0*y + 0*z + 0*1 = A*x
clip.y = 0*x + B*y + 0*z + 0*1 = B*y
clip.z = 0*x + 0*y + C*z + D*1 = C*z + D
clip.w = 0*x + 0*y -1*z + 0*1 = -z
~~~

现在逐个解释：

| 位置/数字 | 作用 | 改变它会看到什么 |
| --- | --- | --- |
| `A=f/aspect` | 缩放 x；宽屏时除以更大的 aspect，防止圆被拉宽 | 改错后物体横向变形 |
| `B=f` | 缩放 y；由竖直 FOV 决定 | FOV 变小时物体在屏幕上变大 |
| `C` | 把 View Space 的 z 参与深度分子 | 决定 Near/Far 如何落到 NDC z |
| `D` | 给 `clip.z` 加常数，配合 C 把 near/far 映射到深度范围 | 改错后深度测试和裁剪边界错误 |
| `-1`（`m32`） | 让 `clip.w=-z`，把“离相机的距离”交给下一步除法 | 没有它就没有正确透视缩小 |
| 最后一列的 0 | 这张投影不靠平移 x/y；深度常数放在 `D` | 误放数值会改变输出公式 |

最关键的一点是：Projection Matrix 没有直接写出 `x/(-z)` 这样的除法。它先产生：

~~~text
clip.x = A*x
clip.w = -z
~~~

下一步透视除法才把两者相除：

~~~text
ndc.x = clip.x / clip.w = A*x / (-z)
ndc.y = clip.y / clip.w = B*y / (-z)
~~~

这就是“远小近大”的来源。比如 `A=1`、`x=1`：

~~~text
z=-2  -> ndc.x = 1/2 = 0.5
z=-10 -> ndc.x = 1/10 = 0.1
~~~

同样的 x，距离越远，除数 `-z` 越大，结果越接近 0，也就是越靠近屏幕中心。

再看 C、D 如何安排深度。以 `near=1、far=10` 为例：

~~~text
C = -11/9
D = -20/9
~~~

对近平面上的点 `z=-1`：

~~~text
clip.z = (-11/9)*(-1) + (-20/9) = -1
clip.w = 1
ndc.z  = -1/1 = -1
~~~

对远平面上的点 `z=-10`：

~~~text
clip.z = (-11/9)*(-10) + (-20/9) = 10
clip.w = 10
ndc.z  = 10/10 = 1
~~~

所以 `C` 和 `D` 的组合不是随便出现的：它们让 `z=-near` 落在 NDC z=-1，让 `z=-far` 落在 NDC z=1。中间深度并不是线性的，这也是远处深度精度更紧张的原因。

### 3.16 Clip Space 与 clip.w

Projection Matrix 输出四个数：

~~~text
clip = (clip.x, clip.y, clip.z, clip.w)
~~~

这时还没有得到最终三维位置，因为还未除以 w。Clip Space（裁剪空间）保留 w，是为了用统一的不等式描述可见区域：

~~~text
-w <= clip.x <= w
-w <= clip.y <= w
-w <= clip.z <= w
~~~

例如 `clip.w=5` 时，x 允许在 `[-5,5]`；`clip.w=1` 时只允许 `[-1,1]`。空间边界随距离扩大，正好对应原来的视锥体。

Projection Matrix 把难处理的斜边视锥体变成了齐次坐标中的统一裁剪范围。这是 Clip Space 名字的来源。

### 3.17 为什么必须先裁剪，再做透视除法

透视除法为：

~~~text
ndc.x = clip.x / clip.w
ndc.y = clip.y / clip.w
ndc.z = clip.z / clip.w
~~~

若一个顶点在相机后方，`clip.w` 可能为负；若它接近相机平面，w 接近 0。直接除法可能把坐标翻转或放大到极大值。

更重要的是，一个三角形可能只有部分穿过近平面：

~~~text
两个顶点在可见区域 + 一个顶点太近
~~~

可见部分仍应保留。不能只删除那个顶点，因为剩下两个点无法组成原可见多边形。正确裁剪会求边与裁剪平面的交点，生成新顶点，再把剩余多边形三角化。第 6 章会实现。

裁剪在齐次坐标中进行，还能正确插值之后需要的属性和 `1/w` 数据。

### 3.18 NDC：与分辨率无关的标准盒子

完成透视除法后得到 NDC（Normalized Device Coordinates，归一化设备坐标）。OpenGL 中可见范围是：

~~~text
-1 <= ndc.x <= 1
-1 <= ndc.y <= 1
-1 <= ndc.z <= 1
~~~

NDC 与窗口是 800×600 还是 3840×2160 无关。它把“投影和裁剪”与“具体屏幕像素数”分开。

~~~text
NDC 左边   x=-1
NDC 中心   x= 0
NDC 右边   x= 1
~~~

这里的 z 没被丢弃。它会映射到深度缓冲范围，用于遮挡测试。

### 3.19 Viewport Transform：从 NDC 到像素

Viewport（视口）是 Framebuffer 中用于显示渲染结果的矩形。若它从 `(viewportX, viewportY)` 开始，宽高为 `width,height`：

~~~text
screen.x = viewportX + (ndc.x + 1) * 0.5 * width
screen.y = viewportY + (ndc.y + 1) * 0.5 * height
~~~

公式分三步理解：

1. `ndc+1`：把 `[-1,1]` 移到 `[0,2]`。
2. `*0.5`：把 `[0,2]` 缩到 `[0,1]`。
3. `*width/height` 并加起点：映射到具体 Viewport。

宽 800、起点 0 的数值例子：

| ndc.x | 计算 | screen.x |
| ---: | --- | ---: |
| -1 | `(-1+1)*0.5*800` | 0 |
| 0 | `(0+1)*0.5*800` | 400 |
| 1 | `(1+1)*0.5*800` | 800，位于右边界 |

像素索引实际是 `0..799`。坐标 800 是连续 Viewport 的右边界，不是一个有效像素索引。光栅化时会计算包围盒并夹到合法索引范围。

若 Framebuffer 约定 y=0 在上方，需要在这一边界转换：

~~~text
screenYTopOrigin = height - screenYBottomOrigin
~~~

不要在多个函数里时而翻转 y，统一在明确的一处处理。

### 3.20 深度映射不是世界米数

透视投影后，NDC z 通常不是与 View Space 距离成线性关系。更多深度精度集中在近平面附近，远处的值挤得更密。

OpenGL 默认常把 NDC z 的 `[-1,1]` 映射到深度缓冲 `[0,1]`：

~~~text
depth = ndc.z * 0.5 + 0.5
~~~

这意味着 depth 是用于比较的投影值，不是“离相机多少米”。若需要从 depth 恢复世界距离，必须结合 Projection Matrix 做逆变换。

near 设得非常小、far 设得非常大时，大量精度浪费在极近处，远处相邻表面可能映射成几乎相同的深度，出现 z-fighting（深度闪烁）。例如地面和贴在地面上的另一张面交替抢占像素。

### 3.21 Clipping、Culling 与 Depth Test 不一样

三个词都可能让几何不出现在画面中，但发生阶段和原因不同：

| 名词 | 中文 | 处理什么 | 例子 |
| --- | --- | --- | --- |
| Clipping | 裁剪 | 切掉跨越视锥边界的部分 | 三角形一半穿过近平面 |
| Culling | 剔除 | 整体跳过确定不需绘制的图元或物体 | 背向相机的三角形 |
| Depth Test | 深度测试 | 每个 Fragment 与已记录深度比较 | 箱子遮住后面的墙 |

不要用“删除越界顶点”代替裁剪，也不要以为视锥裁剪能解决物体互相遮挡。

#### 本部分检查点

**能做出来**：手算一个点从 View Space 经投影矩阵到 Clip Space，再做透视除法得到 NDC，最后经 Viewport 得到屏幕坐标。

**能回答**：

- `w` 在投影后为什么不再是 1？
- NDC 的取值范围是多少？
- 远小近大到底是哪一步产生的？

**卡在这里就先不要往下读**：这是全书最关键的一条链。任何一步说不清就不要进第 4 章，否则第 5、6 章的插值会变成猜。

## 第三部分：完整追踪、实现与排错

### 3.22 一个顶点完整走过六个空间

下面用可手算的数字走完流程。约定：

- 模型只向 x 正方向平移 2。
- 相机位于世界原点、无旋转，因此 View Matrix 是单位矩阵。
- `fovY=90°`，`aspect=1`，所以 `f=1`。
- `near=1`，`far=10`。
- Viewport 为 800×800，左下原点。

局部点：

~~~text
P_local = (-1, 1, -5, 1)
~~~

#### 第一步：Local -> World

向右平移 2：

~~~text
P_world = (1, 1, -5, 1)
~~~

#### 第二步：World -> View

相机是默认状态，View Matrix 为单位矩阵：

~~~text
P_view = (1, 1, -5, 1)
~~~

`z=-5` 表示点在相机前方 5 个单位。

#### 第三步：View -> Clip

本例 Projection Matrix 的关键系数：

~~~text
-(far+near)/(far-near) = -11/9
-2far*near/(far-near) = -20/9
~~~

相乘得到：

~~~text
clip.x = 1
clip.y = 1
clip.z = (-11/9)*(-5) + (-20/9)*1 = 35/9 ≈ 3.8889
clip.w = -(-5) = 5

P_clip = (1, 1, 3.8889, 5)
~~~

检查裁剪条件：x、y、z 都在 `[-5,5]` 内，因此可见。

#### 第四步：Clip -> NDC

除以 w：

~~~text
P_ndc = (1/5, 1/5, 3.8889/5)
      ≈ (0.2, 0.2, 0.7778)
~~~

#### 第五步：NDC -> Screen

~~~text
screen.x = (0.2+1)*0.5*800 = 480
screen.y = (0.2+1)*0.5*800 = 480
depth    = 0.7778*0.5+0.5 ≈ 0.8889
~~~

最终，局部顶点 `(-1,1,-5)` 对应屏幕连续坐标约 `(480,480)`，并带有深度约 `0.8889`。它不是说某个 Vertex 自己就是这个 Pixel；三角形的三个顶点都得到屏幕位置后，光栅器才判断它们之间覆盖哪些像素。

### 3.23 C++ 中让空间类型更清楚

最简单实现都会使用 `Vec4`，但变量名应明确空间：

~~~cpp
Vec4 localPosition = {x, y, z, 1.0f};
Vec4 worldPosition = model * localPosition;
Vec4 viewPosition  = view * worldPosition;
Vec4 clipPosition  = projection * viewPosition;

// 输入：Clip Space 坐标。
// 输出：透视除法后的 NDC 坐标。
// 前提：三角形已经正确裁剪，且 w 不接近 0。
Vec3 ndcPosition = {
    clipPosition.x / clipPosition.w,
    clipPosition.y / clipPosition.w,
    clipPosition.z / clipPosition.w
};
~~~

也可以预先组合：

~~~cpp
Mat4 mvp = projection * view * model;
Vec4 clipPosition = mvp * localPosition;
~~~

组合更快，但调试初期建议保留中间值，这样能判断错误发生在哪个空间。

建议定义顶点阶段输出：

~~~cpp
struct ClipVertex {
    Vec4 clipPosition;
    Vec2 uv;
    Vec3 color;
};
~~~

不要过早只保存屏幕 x、y。后续裁剪和透视正确插值仍需要 `clip.w` 以及其他属性。

### 3.24 相机移动时哪些数会改变

理解空间的一个好方法是预测变化：

| 操作 | Local | World | View | Clip/NDC/Screen |
| --- | --- | --- | --- | --- |
| 只移动模型 | 不变 | 改变 | 改变 | 改变 |
| 只移动相机 | 不变 | 不变 | 改变 | 改变 |
| 只改变 FOV | 不变 | 不变 | 不变 | Clip/NDC/Screen 改变 |
| 只改变窗口分辨率，宽高比不变 | 不变 | 不变 | 不变 | NDC 不变，Screen 改变 |
| 改变宽高比并更新投影 | 不变 | 不变 | 不变 | Clip/NDC/Screen 可能改变 |

若只移动相机却发现 World Position 变化，说明代码把 View 变换错误地写回了世界数据。

### 3.25 常见错误与黑屏定位

| 现象 | 可能原因 | 最小检查 |
| --- | --- | --- |
| 所有物体在相机后面 | 相机前方正负号混乱 | 默认相机前方点应有 `view.z<0` |
| 相机向右，世界也向右 | View Matrix 没取逆 | 用纯平移手算 `world-camera` |
| 圆变成椭圆 | Projection 的 aspect 错或窗口变化后未更新 | 打印 framebuffer 宽高和 aspect |
| 远近大小完全相反 | 透视除法符号或矩阵约定混用 | 测试 `(1,0,-2)` 与 `(1,0,-10)` |
| 靠近相机出现整屏尖刺 | 未做近平面裁剪或 w 接近 0 仍除法 | 打印 clip.w 并观察跨近平面三角形 |
| 画面上下颠倒 | Screen/图片 y 原点约定不一致 | 只在最终输出处翻转一次 |
| 深度闪烁 | near 太小、far 太大或表面几乎重合 | 提高 near、降低 far，检查共面几何 |
| 矩阵看似正确仍黑屏 | 行列约定、乘法顺序或上传转置错误 | 分空间打印一个已知测试顶点 |

### 3.26 随读自检

1. 为什么同一个顶点能同时有 Local 和 World 两组不同坐标？
2. 相机世界位置为 `(3,0,0)` 时，为什么 View 变换包含向左平移 3？
3. 透视矩阵为什么先输出 `clip.w`，而不是直接在矩阵里除以 z？
4. 为什么跨越近平面的三角形不能只删掉越界顶点？
5. NDC 的价值是什么？

参考答案：

1. 它们相对于不同原点和坐标轴描述同一地点。
2. 要把相机假装放回原点，世界必须施加相机运动的逆。
3. 矩阵只能做线性乘加，不能按输入动态除法；同时保留 w 便于齐次裁剪。
4. 原三角形仍有一部分可见，需要求交点生成新顶点。
5. 把投影结果统一到与具体屏幕分辨率无关的标准范围。

### 3.27 练习

#### 练习 A：空间日志

选择一个立方体顶点，打印它每帧的 Local、World、View、Clip、NDC 和 Screen 值。每次只改变一个参数，先写预测再运行。

#### 练习 B：相机逆变换

先只支持无旋转相机。用 `viewPosition = worldPosition - cameraPosition` 手算三个点，再与平移 View Matrix 的结果比较。

#### 练习 C：远小近大

使用 90° FOV 和 aspect=1，计算 `(1,0,-2)`、`(1,0,-5)`、`(1,0,-10)` 的 NDC x。应分别约为 `0.5、0.2、0.1`。

#### 练习 D：Viewport 映射

写函数：

~~~cpp
Vec2 ndcToScreen(Vec3 ndc, int width, int height);
~~~

输入 NDC 和 Viewport 尺寸，输出左下原点的连续屏幕坐标。验证 `(-1,-1)`、`(0,0)`、`(1,1)`。

#### 练习 E：故意出错实验（破坏空间链）

依次做以下实验并截图记录：

- 把 aspect 固定为 1 后拉宽窗口。
- 把 View Matrix 换成 Camera World Matrix。
- 在组合 MVP 时交换 View 与 Model。
- 不检查 w，直接对相机后方顶点透视除法。

不要只恢复代码，要用空间概念解释每种错误画面。

### 3.28 本章验收标准

这一章的核心只有一条链。链通了就可以继续，细节可以第二遍再补。

**最低验收**（第一次必须完成）：

- 能画出 `Local -> World -> View -> Clip -> NDC -> Screen`，并为每一步说出参考系。
- 能手算一个点走完这条链，包括透视除法和 Viewport 映射。
- 能解释为什么 `w` 在投影之后不再是 1。
- 程序能打印每个空间的中间值。出错时你能指出是哪一步开始偏的。

**推荐验收**：相机逆变换的推导、FOV/aspect/near/far 各自的影响、从相似三角形解释 `x/(-z)`。

**扩展验收**（可留到第二遍）：正交投影、无限远 far、反向深度（Reverse-Z）。

> 最低验收里的第二条不能跳。第 5、6 章的插值和第 8 章的 Shader 调试，全部建立在“能手算一个点走完这条链”之上。手算不出来，后面的黑屏就只能靠猜。

---

完整清单：

- 能画出 `Local -> World -> View -> Clip -> NDC -> Screen`，并为每一步说出参考系和变换矩阵。
- 能解释 Camera Transform 与 View Matrix 的逆关系。
- 能从相似三角形说明为什么投影坐标含有 `x/(-z)`。
- 能解释 FOV、aspect、near、far 各自改变什么。
- 能手算本章 3.22 的完整例子，误差允许来自四舍五入。
- 能说明裁剪、剔除和深度测试的区别。
- 程序可打印每个空间的中间值，出现错误时能定位到具体变换阶段。

下一章先不画三角形，而是在 CPU 内存中创建一张真正可保存的二维画布。这样可以明确“屏幕坐标最终如何变成数组中的颜色”。
