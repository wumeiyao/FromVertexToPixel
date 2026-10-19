# 第 1 章：一帧画面如何产生

<!-- nav -->
> [上一章：如何使用本书](00-reading-guide.md) ｜ [下一章：向量、矩阵与齐次坐标](02-vectors-matrices.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/01_math/01-rendering-mental-model.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

这一章不要求会 OpenGL、C++、矩阵或数学公式。目标只有一个：当你看见游戏里的 3D 物体时，能说清楚它怎样从一组数据变成屏幕颜色。

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 区分 Model File、Mesh、Vertex 和 Triangle 四个层级。
2. 说出 position、normal、uv 各自回答什么问题。
3. 解释 Index 为什么和 Vertex 属性分开保存。
4. 说清 Mesh、Material、Texture 和 Scene Object 怎样配合。
5. 区分“加载一次资源”和“每帧绘制”。
6. 说出 CPU、GPU 和 Draw Call 各自负责什么。
7. 解释三个 Vertex 为什么能产生成千上万个 Fragment。
8. 不看书复述一遍从 Vertex 到 Present 的完整链条。

本章先建立主线。内存布局、矩阵公式、Shader 代码和光栅化算法会在后续章节分别展开，现在不需要一次学完。

本章分三部分：先认清输入数据，再跟随它走完一帧，最后区分容易混淆的概念并练习复述。

## 第一部分：渲染器拿到的是什么

### 1.1 先看最终结果：屏幕只认识 Pixel

显示器不是三维空间。它显示的是一张二维网格，每个小格叫 Pixel，中文叫像素。

一个 Pixel 至少需要红、绿、蓝三种颜色强度：

~~~text
R：红色强度
G：绿色强度
B：蓝色强度
~~~

本书常暂时用 0 到 1 表示强度：

| RGB | 颜色 |
| --- | --- |
| `(0,0,0)` | 黑色 |
| `(1,1,1)` | 白色 |
| `(1,0,0)` | 红色 |
| `(0,1,0)` | 绿色 |
| `(0,0,1)` | 蓝色 |
| `(1,1,0)` | 黄色 |

一张 1920 x 1080 的画面有两百多万个 Pixel。所谓“渲染一帧”，最终就是为这一帧中需要显示的位置计算颜色。

~~~text
一张很小的示意画面

+---+---+---+---+---+
| 蓝| 蓝| 蓝| 蓝| 蓝|
+---+---+---+---+---+
| 蓝| 红| 红| 红| 蓝|
+---+---+---+---+---+
| 红| 红| 红| 红| 红|
+---+---+---+---+---+
~~~

红色区域看起来像三角形。渲染器不是预先保存一张红色三角形图片，而是判断三角形覆盖了哪些位置，再为这些位置计算红色。

### 1.2 3D 表面为什么拆成 Triangle

一个 3D 模型可以先理解为许多 Triangle，中文叫三角形。三角形有三个角，每个角称为 Vertex，中文叫顶点。

~~~text
              Vertex C
                  *
                 / \
                /   \
               /     \
              *-------*
         Vertex A     Vertex B
~~~

游戏中的角色、汽车、树和房屋看起来很复杂，但最终都能拆成大量三角形。

使用三角形有三个重要原因：

1. 三个互不相同且不共线的点能唯一确定一个平面。
2. 三角形内部的数据容易根据三个角平滑计算出来。
3. GPU 针对大量三角形的处理进行了专门设计。

“不共线”是必要条件。若三个点都在同一条直线上，就没有围成面积，这叫退化三角形。

一个立方体有 6 个面，每个四边形面通常拆成 2 个 Triangle，因此至少需要 12 个 Triangle。

### 1.3 Model、Mesh、Vertex 和 Triangle

这几个名词处于不同层级：

~~~text
模型文件 car.gltf
|
+-- 车身 Mesh
|   +-- Vertex 数据
|   +-- Triangle 连接关系
|
+-- 轮胎 Mesh
|   +-- Vertex 数据
|   +-- Triangle 连接关系
|
+-- Material、Texture、节点等其他数据
~~~

| 名词 | 它是什么 | 汽车例子 |
| --- | --- | --- |
| Model File，模型文件 | 磁盘上的资源文件 | `car.gltf` |
| Mesh，网格 | 描述一块几何形状的数据 | 车身形状 |
| Vertex，顶点 | Triangle 一个角的整条属性记录 | 车门表面某个角点的数据 |
| Triangle，三角形 | 三条 Vertex 记录连接成的一小块表面 | 车门表面的一小片 |

一个模型文件可以包含多个 Mesh。Mesh 通常包含两部分：

~~~text
Vertex 数据：有哪些角点记录
Index 数据：按什么编号把记录连接成 Triangle
~~~

现在最重要的是不要把整个模型、一个 Mesh 和一条 Vertex 记录当成同一个东西。

### 1.4 一个 Vertex 里到底有什么

Vertex 不只是一个空间位置。更准确地说：

> Vertex 是送入绘制流程的一整条属性记录。

常见属性包括：

| 属性 | 常见形式 | 回答的问题 |
| --- | --- | --- |
| `position` | 三个数 `(x,y,z)` | 这个角点在哪里？ |
| `normal` | 三个数 `(nx,ny,nz)` | 这个角点附近的表面朝哪里？ |
| `uv` | 两个数 `(u,v)` | 这个角对应 Texture 的哪里？ |
| `color` | 三或四个数 | 这个角自带什么颜色？ |

一条具体记录可以写成：

~~~text
Vertex 0
    position = (-0.6, -0.5, 0.0)
    normal   = ( 0.0,  0.0, 1.0)
    uv       = ( 0.0,  0.0)
    color    = ( 1.0,  0.0, 0.0)
~~~

这里的 `Vertex 0` 表示第 0 条记录，不表示屏幕 Pixel `(0,0)`。

不是所有 Mesh 都必须包含全部属性：

| 想实现的效果 | 最少可能需要的属性 |
| --- | --- |
| 只画固定纯色形状 | `position` |
| 显示顶点渐变色 | `position、color` |
| 显示 Texture | `position、uv` |
| 计算普通光照 | `position、normal`，通常还有 `uv` |

骨骼编号、骨骼权重和 Tangent 也是可能的高级属性，但现在不需要展开。需要什么属性由 Mesh 数据和 Shader 共同决定。

### 1.5 position、normal 和 UV 分别表示什么

#### position：角点在哪里

3D position 通常有 `x、y、z` 三个数，可以先理解为左右、上下、前后。

~~~text
position = (1, 0, -2)
~~~

它通常表示相对于模型自身原点的位置，不是屏幕 Pixel 坐标。

例如轮胎 Vertex 的 position 可以表示“相对轮胎中心向右 0.3 米”。整辆车停在公路哪里，由物体的 Transform 决定；Camera 再把世界位置投影到屏幕。

~~~text
模型局部位置
-> 世界位置
-> 相对于 Camera 的位置
-> 屏幕位置
~~~

第 2、3 章会解释这些位置怎样通过向量和矩阵逐步变化。

#### normal：表面朝哪里

Normal，法线，是一根表示表面朝向的箭头：

~~~text
                 normal
                    ^
                    |
            --------+-------- 表面
~~~

灯光计算会比较表面方向与光线方向。表面正对光时通常更亮，背对光时通常更暗。

Normal 是参与计算的方向数据，不是默认画在屏幕上的线，也不是另一个空间位置。

#### uv：读取 Texture 哪里

Texture，纹理或贴图，入门时可以先理解成一张图片。UV 建立 Vertex 与 Texture 位置之间的对应关系。

~~~text
Texture

(0,1) +-------------------+ (1,1)
      |                   |
      |                   |
(0,0) +-------------------+ (1,0)
~~~

`u` 表示横向，`v` 表示纵向，常用 0 到 1 的相对坐标。

假设三角形三个角的 UV 是：

~~~text
左下角 -> (0,0)
右下角 -> (1,0)
上方   -> (0.5,1)
~~~

GPU 会为三角形内部位置计算中间 UV，再去 Texture 中读取颜色。因此 UV 不是最终颜色，也不是屏幕 Pixel 坐标。

### 1.6 三条 Vertex 怎样组成 Triangle

下面三条 position 能组成一个 Triangle：

| Vertex 编号 | position | color |
| --- | --- | --- |
| 0 | `(-0.6,-0.5,0)` | 红色 |
| 1 | `(0.6,-0.5,0)` | 绿色 |
| 2 | `(0,0.6,0)` | 蓝色 |

~~~text
               Vertex 2：蓝
                     *
                    / \
                   /   \
                  /     \
                 *-------*
        Vertex 0：红     Vertex 1：绿
~~~

只有这些数字存在于内存中时，屏幕还不会自动出现 Triangle。程序还要把数据交给 GPU，并发出绘制命令。

#### Index 是独立的顶点编号列表

相邻 Triangle 经常共用 Vertex。以正方形为例：

~~~text
3 *-----------* 2
  |         / |
  |       /   |
  |     /     |
  |   /       |
0 *-----------* 1
~~~

两个 Triangle 可以写成：

~~~text
Triangle A：0, 1, 2
Triangle B：0, 2, 3
~~~

因此：

~~~text
Vertex 数量 = 4
Index 数量 = 6
Triangle 数量 = 2
~~~

Index 通常保存在独立数组中，不是每条 Vertex 内的属性。

只有整条属性记录都相同时才能直接共享 Vertex。立方体同一个几何角在不同面上可能需要不同 Normal；Texture 接缝两侧可能需要不同 UV。这些情况下，position 相同也可能保存为多条 Vertex 记录。第 9 章会结合模型资产再讲。

### 1.7 Mesh、Material、Texture 和 Scene Object

Mesh 主要描述形状，但要把一个物体画进游戏世界，还需要其他数据：

~~~text
Scene Object：场景中的具体对象，例如“门口那棵树”
Transform：它在世界中的位置、旋转和缩放
Mesh：树的几何形状
Material：树的表面怎样计算颜色
Texture：Material 可以读取的图片或数据
~~~

同一个 Tree Mesh 可以被许多 Scene Object 共享：

~~~text
Tree A：Tree Mesh + 位置 (5,0,2)
Tree B：Tree Mesh + 位置 (-8,0,10)
Tree C：Tree Mesh + 位置 (2,0,20)
~~~

三棵树不需要复制三份全部 Vertex，只需要不同 Transform。

Material 也不是 Texture 的另一个名字：

- Texture 保存可采样的数据，例如树皮图片。
- Material 指定使用什么 Shader、Texture 和参数来计算表面颜色。
- Mesh 决定这些表面位于哪里。

#### 本部分检查点

**能做出来**：在纸上画出一个模型文件的树状结构，标出 Mesh、Vertex、Index、Material、Texture 各在哪一层。

**能回答**：

- 一条 Vertex 里除了 position 还可能有什么？
- Index 为什么不存在 Vertex 里面？
- 同一个 Tree Mesh 怎样出现在场景的三个位置？

**卡在这里就先不要往下读**：回到 1.3 的汽车例子，把“模型文件、Mesh、Vertex”三个词分别用一句话说清，再继续。

## 第二部分：数据怎样走完一帧

### 1.8 加载模型和每帧绘制不是一回事

模型通常先加载一次，之后连续绘制许多帧。

#### 加载阶段

~~~text
读取模型和图片文件
-> 解析 Vertex、Index、Material、Texture
-> 创建 GPU 资源
-> 上传数据
-> 保存资源 Handle 供后续使用
~~~

#### 每一帧

~~~text
更新对象和 Camera
-> 找出当前需要绘制的对象
-> 准备 Transform、Material、灯光等本帧数据
-> 发出 Draw Call
-> GPU 生成 Framebuffer 内容
-> 显示这一帧
~~~

静态 Mesh 一般不会每帧重新从磁盘读取，也不会每帧重新创建全部资源。

### 1.9 一帧的主流程

先看全貌：

~~~text
Mesh 的 Vertex 和 Index
-> CPU 准备本帧对象、Camera、Material
-> CPU 发出 Draw Call
-> GPU 处理 Vertex 位置和属性
-> GPU 组装并投影 Triangle
-> GPU 找出 Triangle 覆盖的位置
-> GPU 为这些位置计算候选颜色
-> 深度等测试决定是否写入
-> Framebuffer 保存结果
-> Present 给窗口和显示器
~~~

下面只解释每一步负责什么，不在本章展开公式和 OpenGL 函数。

### 1.10 CPU、GPU 与 Draw Call

#### CPU

CPU 运行普通 C++ 游戏程序，主要负责：

- 读取输入。
- 更新角色、Camera 和游戏规则。
- 找出哪些对象可能需要画。
- 准备 Transform、Material 和灯光等参数。
- 向 GPU 提交绘制命令。

#### GPU

GPU 擅长同时执行大量相似工作，例如处理许多 Vertex 和 Fragment。

GPU 不会自己猜测要画什么。它执行 CPU 提交的命令和 Shader 程序。

#### Draw Call

Draw Call 是 CPU 通过图形 API 发出的一次绘制命令。它大致表达：

~~~text
使用这份 Mesh 数据
使用这个 Material 和 Shader
使用当前 Camera 与物体 Transform
把这些 Triangle 画到当前 Framebuffer
~~~

一次 Draw Call 可以绘制一个 Triangle，也可以绘制一个 Mesh 的许多 Triangle。一帧通常包含许多 Draw Call。

### 1.11 Vertex 阶段：先确定三角形三个角在哪里

GPU 首先读取 Draw Call 指定的 Vertex。Vertex Shader 会对每条输入 Vertex 运行一次。

它最重要的任务是把模型局部 position 转换成可投影的位置：

~~~text
Local Position
-> Model Transform：放进世界
-> Camera View：改成相对于 Camera
-> Projection：准备投影到屏幕
~~~

Vertex Shader 还可以把 Normal、UV、Color 等属性传给后续阶段。

处理一个 Triangle 时，Vertex Shader 主要处理它的三个角。此时还没有逐个计算三角形内部的 Pixel。

第 2 章学习变换所需的向量和矩阵，第 3 章学习 Camera 和投影，第 8 章再真正编写 GLSL Vertex Shader。

### 1.12 Triangle 组装、裁剪与屏幕映射

三个处理后的 Vertex 会按 Index 组装成 Triangle。

接下来 GPU 要处理三个问题：

1. Triangle 是否完全在 Camera 可见范围外？
2. 若它穿过可见边界，哪些部分应该保留？
3. 保留部分最终映射到 Framebuffer 的什么位置？

Clipping，裁剪，会保留可见范围内的部分。Projection，投影，会产生近大远小的效果。Viewport 映射再把标准位置对应到实际 Framebuffer 尺寸。

这里不能简单地“删除 z”。屏幕覆盖主要由 x、y 决定，但深度值仍会保留，用于判断谁挡住谁。

这些计算在第 3、6 章展开。

### 1.13 Rasterization：找出 Triangle 覆盖哪些位置

投影后，GPU 已经知道 Triangle 三个角在屏幕附近的位置。Rasterization，光栅化，负责判断它覆盖哪些采样位置。

~~~text
连续 Triangle              离散覆盖示意

       *                       . . X . .
      / \                      . X X X .
     /   \                     X X X X X
    /_____\
~~~

Triangle 只有 3 个 Vertex，却可能覆盖几万个屏幕位置。因此 Vertex 与 Pixel 不存在一一对应关系。

光栅化为被覆盖位置产生 Fragment，中文叫片段。Fragment 是候选结果，还不是最终 Pixel。

第 4～6 章会在 CPU 上亲手实现 Framebuffer 和 Triangle 光栅化，以便观察每一步。

### 1.14 Interpolation：为 Triangle 内部补出属性

UV、Normal 和 Color 只存于 Vertex，但 Fragment 出现在 Triangle 内部。GPU 会根据 Fragment 靠近三个角的程度进行 Interpolation，插值。

例如三个角分别为红、绿、蓝：

~~~text
                 蓝
                 *
                / \
               / P \
              /     \
             *-------*
            红       绿
~~~

P 靠近红色角时，红色成分更多；位于三角形中央时，三种颜色都会参与。

假设 P 对三个角的权重分别是：

~~~text
红 50%
绿 25%
蓝 25%
~~~

那么颜色为：

~~~text
0.5*(1,0,0) + 0.25*(0,1,0) + 0.25*(0,0,1)
= (0.5,0.25,0.25)
~~~

同样的思想可以用于 UV 和 Normal。透视下的精确插值会在第 5、6 章讲。

### 1.15 Fragment Shader、Depth Test 与 Framebuffer

#### Fragment Shader 计算候选颜色

Fragment Shader 对光栅化产生的 Fragment 运行。最简单的 Shader 可以总是输出红色；更常见的做法是：

~~~text
使用插值 UV 读取 Texture
-> 使用 Normal 和 Light 计算明暗
-> 得到候选颜色
~~~

#### Depth Test 决定前后遮挡

同一个屏幕位置可能先后收到墙、箱子等多个 Fragment。Depth Buffer 保存当前最近结果的深度。

假设深度越小越近：

~~~text
已有红墙：depth=0.8
新来绿箱：depth=0.3

0.3 比 0.8 近
-> 绿色通过测试
-> 更新颜色和深度
~~~

之后若蓝色 Fragment 深度为 0.6，它比绿箱远，会被丢弃。

#### Framebuffer 保存这一帧

Framebuffer 可以理解为当前渲染结果的一组二维缓冲：

| 缓冲 | 保存什么 | 用途 |
| --- | --- | --- |
| Color Buffer | R、G、B、A | 最终或中间颜色 |
| Depth Buffer | 深度值 | 判断前后遮挡 |

通过测试的 Fragment 才会修改这些缓冲。完成后，最终颜色画面通过 Present 交给窗口系统显示。

### 1.16 用一个三角形完整走一遍

现在把所有名词接起来。

#### 原始数据

~~~text
Vertex 0：position 左下，color 红
Vertex 1：position 右下，color 绿
Vertex 2：position 上方，color 蓝
Index：0, 1, 2
~~~

#### 加载时

1. C++ 程序准备三条 Vertex 和三个 Index。
2. 程序创建 GPU 资源并上传数据。
3. 程序准备处理这些属性的 Shader。

#### 当前帧的 CPU 工作

1. 开始新的一帧并清理颜色、深度。
2. 准备 Camera、Transform 和 Material 数据。
3. 选择三角形资源。
4. 发出一次 Draw Call。

#### GPU 工作

1. 读取 Index 0、1、2 对应的三条 Vertex。
2. Vertex Shader 计算三个角的位置并输出 Color。
3. 三个角组装成一个 Triangle。
4. Projection 已把三个角变到 Clip Space；Triangle 先在这里裁剪，再做透视除法和屏幕映射。
5. Rasterization 找出覆盖的采样位置。
6. 每个 Fragment 得到插值 Color 和深度。
7. Fragment Shader 输出候选渐变色。
8. Depth Test 决定结果能否写入 Framebuffer。

#### 展示

1. 本帧所有绘制完成。
2. 完成的颜色缓冲被 Present。
3. 显示器显示红、绿、蓝渐变 Triangle。

数量关系可能是：

~~~text
3 条 Vertex
-> 3 个 Index
-> 1 个 Triangle
-> 覆盖 20,000 个采样位置
-> 产生许多 Fragment
-> 通过测试的结果修改对应 Pixel
~~~

能够复述这条链，比现在记住任何 OpenGL 函数名更重要。

#### 本部分检查点

**能做出来**：遮住 1.16 节，从“3 个 Vertex + Index 0,1,2”一路讲到显示器亮起来。

**能回答**：

- Draw Call 是谁发给谁的？
- 三个 Vertex 怎么变成两万个 Fragment？
- Fragment 为什么不一定能改变最终 Pixel？

**卡在这里就先不要往下读**：不要往第三部分走。先把 1.9 的那张主流程图默写一遍，缺哪一步就回去读哪一节。

## 第三部分：辨析、排错与练习

### 1.17 Pixel、Sample 与 Fragment 的区别

初学时它们看起来很像，但含义不同：

- Pixel：最终图片中的离散位置。
- Sample：Pixel 内用于覆盖、深度等判断的位置。
- Fragment：Triangle 覆盖某个 Sample 后产生的候选数据。

不开多重采样时，它们常接近一一对应，因此容易混称。开启 MSAA 后，一个 Pixel 可以有多个 Sample。

同一个 Pixel 位置还可能收到多个 Fragment，最后由 Depth、Blend 等规则决定最终颜色。

### 1.18 3D 世界和 2D UI 的关系

3D 物体投影到屏幕与绘制 2D UI 是同一帧中的不同工作，不是“先做 2D，再把它变成 3D”。

~~~text
3D World Pass
-> 可选 Post Process
-> 2D UI Pass
-> Present
~~~

UI 通常最后绘制，因此文字、按钮和血条出现在 3D 世界上方。它的底层也常由 Triangle 和 Fragment 组成，只是坐标和绘制规则不同。

### 1.19 常见误解与排查主线

| 误解 | 正确理解 |
| --- | --- |
| Vertex 就是 position | position 只是 Vertex 的一种属性 |
| 一个 Vertex 对应一个 Pixel | 三个 Vertex 组成的 Triangle 可覆盖大量 Pixel |
| Index 存在 Vertex 内 | Index 通常是独立编号数组 |
| Mesh 就是整个模型文件 | 模型文件可以包含多个 Mesh 和其他资源 |
| Texture 就是 Material | Texture 是数据；Material 规定怎样使用数据着色 |
| Vertex Shader 直接画 Pixel | 它先计算角点，光栅化后才产生 Fragment |
| Fragment 就是最终 Pixel | 它仍可能被深度测试丢弃或参与混合 |
| 一次 Draw Call 等于一帧 | 一帧通常包含许多 Draw Call |
| GPU 会自己决定画什么 | CPU 组织场景并提交命令，GPU 执行 |

以后画面错误时，沿数据流排查：

~~~text
资源数据是否正确？
-> Vertex 是否读对？
-> 位置变换是否正确？
-> Triangle 是否在 Camera 范围内？
-> Rasterization 是否产生 Fragment？
-> Fragment Shader 是否输出可见颜色？
-> Depth/Blend 是否丢弃或改变它？
-> 是否写到正确 Framebuffer 并 Present？
~~~

不要同时随机修改多个环节。先找到第一个已经错误的中间结果。

### 1.20 本章练习

#### 练习 A：拆解一条 Vertex

阅读：

~~~text
position=(2,1,-3)
normal=(0,1,0)
uv=(0.25,0.75)
color=(1,0.5,0,1)
~~~

分别说明四组数回答什么问题，并解释为什么它们还不能直接确定最终 Pixel 颜色。

#### 练习 B：正方形索引

使用：

~~~text
0：左下
1：右下
2：右上
3：左上
~~~

写出组成两个 Triangle 的 6 个 Index，并说出 Vertex、Index 和 Triangle 数量。

#### 练习 C：插值颜色

某 Fragment 对红、绿、蓝三个角的权重为 `50%、25%、25%`，手算插值颜色。

#### 练习 D：深度判断

某位置已有黄色、深度 0.4。依次到来：

~~~text
A：红色，深度 0.7
B：绿色，深度 0.2
C：蓝色，深度 0.3
~~~

假设越小越近，写出每一步是否通过，并给出最终颜色和深度。

#### 练习 E：复述完整流程

遮住第 1.16 节，从以下数据开始讲到显示器：

~~~text
3 个 Vertex + Index 0,1,2
~~~

回答中应出现 CPU、Draw Call、Vertex Shader、Triangle、Rasterization、Fragment、Depth Test、Framebuffer 和 Present，并说明每一步做了什么。

#### 练习 F：故意出错实验（纸上推演）

本章还没有代码，所以这次在纸上做。对下面每种错误，**先写下你预测的画面**，再对照本章的数据流解释它为什么会这样。

1. Vertex Shader 忘记做投影，直接把 Local Position 当成 Clip Position 输出。
2. Index 数组写成了 `0, 1, 1`。
3. Fragment Shader 永远输出黑色。
4. Depth Test 的比较方向反了（越远越通过）。
5. 画完之后忘记 Present。

每一条都要回答：**数据流在哪一步开始偏离？屏幕上会出现什么？**

第 4 到第 8 章会让你真的把这几条做一遍。现在先建立“错误现象 ↔ 出错环节”的对应关系。

### 1.21 本章验收标准
完成本章后，应能用自己的话回答：

1. Model File、Mesh、Vertex 和 Triangle 有什么区别？
2. position、normal、uv 分别表示什么？
3. Index 为什么与 Vertex 属性分开？
4. Mesh、Material 和 Texture 怎样配合？
5. 加载资源与每帧绘制有什么区别？
6. CPU、GPU 和 Draw Call 各自负责什么？
7. 三个 Vertex 怎样产生许多 Fragment？
8. Fragment 为什么不一定改变最终 Pixel？
9. Framebuffer 和 Present 分别做什么？

还应能不看图复述：

~~~text
Vertex / Index
-> Draw Call
-> Vertex 变换
-> Triangle 组装、裁剪和投影
-> Rasterization 与插值
-> Fragment Shader
-> Depth 等测试
-> Framebuffer
-> Present
~~~

下一章学习 position 和 normal 为什么能用向量表示，以及矩阵怎样统一移动、旋转和缩放许多 Vertex。数学会始终服务于本章这条绘制主线。
