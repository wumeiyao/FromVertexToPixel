# 第 9 章：网格、贴图与 glTF 资产

<!-- nav -->
> [上一章：GLSL 与第一条 Draw Call](08-shaders-first-draw.md) ｜ [下一章：场景、相机与可见性](10-scene-camera-culling.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/04_scene_camera_assets/09-mesh-model-assets.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

上一章已经能够把程序中写死的顶点交给 GPU，并画出第一个三角形。本章解决一个更接近真实游戏的问题：磁盘上的 `car.glb`、`character.gltf` 和 `wood.png`，怎样一步步变成 GPU 能绘制的数据。

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 区分磁盘文件、CPU 数组和 GPU 资源三个阶段，并说出每个阶段的数据长什么样。
2. 分清 Model File、Mesh、Primitive、Material、Image、Texture 和 Sampler 分别指什么。
3. 解释 stride 和 offset，并按字节画出一条 Vertex 的内存布局。
4. 说明 glTF 里 Buffer、BufferView 和 Accessor 三层各负责什么。
5. 把一张图片从文件走到 GPU Texture，并解释 Mipmap 解决了什么问题。
6. 用资源 Handle 表达共享关系，说清谁拥有资源、谁只是引用。
7. 按“文件 -> CPU -> GPU -> Draw Call”的顺序定位一个模型不显示的问题。

本章不要求你自己编写 glTF 解析器。我们要学习的是资产加载的完整数据流，以及加载库返回的每个重要概念到底表示什么。

本章分三部分：`9.1～9.4` 区分资产中的对象和数据，`9.5～9.9` 把数据整理成 CPU/GPU 可用格式，`9.10～9.17` 读取 glTF 并走到 Draw Call。

不要从复杂角色模型开始。第一遍只加载一个包含单个 Node、单个 Mesh、单个 Primitive 和一张 Base Color Texture 的静态 glTF。确认这条链正确后，再测试多个 Node、多个 Primitive 和缺失属性。

**本章讲 glTF，练习跑 OBJ，这不是笔误**。glTF 是当前最值得学的交付格式，所以概念部分完整讲它的 Scene/Node/Mesh/Primitive/Accessor/BufferView。但解析 glTF 需要 `fastgltf` 或 `cgltf`，而**本书第一遍不安装额外依赖**。所以[本章的参考实现](../exercises/04_scene_camera_assets/09-mesh-model-assets.md)用一个几十行的 OBJ 解析器跑通同一条链：

~~~text
磁盘文件 -> CPU 数组 -> GPU Buffer/Texture -> Draw Call
~~~

这条链的四个边界是本章真正要学会的东西，OBJ 和 glTF 在这一点上没有区别。区别在于 glTF 还能表达节点层级、材质参数和二进制打包——所以概念仍然按 glTF 讲。

两件事不要做：不要为了"跟上正文"去自己写 glTF 解析器（那是一个月的工作，且学不到渲染）；也不要把那个 OBJ 解析器扩展成自制通用资产库。进入毕业工程时直接接入 `fastgltf`，本章学到的边界划分可以原样复用。

## 第一部分：资产中有哪些对象和数据

### 9.1 先建立最重要的边界：文件、CPU 数据、GPU 数据

“加载了一个模型”其实包含三个不同阶段。

~~~text
磁盘文件
car.glb、wood.png
    |
    | 文件读取、格式解析、图片解码
    v
CPU 内存
顶点数组、索引数组、像素数组、节点层级、材质描述
    |
    | 创建 Buffer/Texture，并上传数据
    v
GPU 内存
Vertex Buffer、Index Buffer、Texture、Sampler
    |
    | Draw Call
    v
屏幕上的模型
~~~

这三份数据不是同一个东西。

- 磁盘文件是一串持久保存的字节。程序退出后，它仍然存在。
- CPU 数据是加载器解析出的普通 C++ 数据。程序运行时，它位于系统内存中。
- GPU 数据是通过 OpenGL 创建并上传的资源。它位于图形驱动管理的内存中。

因此，“模型没显示”不能只归结为“模型加载失败”。更有用的排查问题是：

1. 文件是否成功打开？
2. 文件是否成功解析？
3. 顶点、索引和图片是否转换正确？
4. 数据是否上传到了正确的 GPU 资源？
5. Draw Call 是否绑定了正确的 Mesh、Material 和 Texture？

只要始终按这条链排查，资产问题就不会变成一团无法解释的黑盒。

---

### 9.2 先把容易混淆的名词分开

#### 9.2.1 Model file：模型文件

模型文件是磁盘上的资产容器，例如：

~~~text
robot.gltf
robot.glb
tree.obj
~~~

它不一定只包含一个形状。一个模型文件可能同时包含：

- 多个 Mesh。
- 多个 Material。
- 多张 Texture。
- 父子节点层级。
- 动画和骨骼。
- 相机或灯光描述。

本章先处理静态 Mesh、节点、材质和贴图；动画以后再扩展。

#### 9.2.2 Mesh：网格

Mesh 是物体的几何形状数据。最基本的 Mesh 包含：

~~~text
顶点数据 + 三角形连接关系
~~~

例如，一个汽车文件可以包含车身 Mesh、四个轮胎 Mesh 和玻璃 Mesh。

#### 9.2.3 Primitive：一次可直接绘制的几何部分

glTF 中一个 Mesh 还可以包含多个 Primitive。这里的 Primitive 不是单独一个三角形，而是一批使用相同绘制方式和同一材质的顶点与索引。

例如车身虽然是一个 Mesh，但可能分成：

~~~text
Primitive 0：车漆部分，使用红色车漆材质
Primitive 1：镀铬部分，使用金属材质
Primitive 2：车窗部分，使用透明玻璃材质
~~~

一次普通 Draw Call 通常只能使用一套材质，所以这些 Primitive 往往要分别绘制。

glTF Primitive 还带有 `mode`，说明数据应解释成 Triangle、Triangle Strip、Line 或 Point。第一版只支持普通 `TRIANGLES`：加载到其他 mode 时打印清楚并拒绝，而不是默默按三个索引一组错误绘制。

#### 9.2.4 Material：材质

Material 描述表面怎样被着色。它通常包含：

- 使用哪个 Shader。
- 基础颜色是多少。
- 使用哪些 Texture。
- 表面有多粗糙、多像金属。
- 是否透明、是否双面。

Material 不负责描述形状。换一个 Material 不会把球变成立方体，只会改变球的外观。

#### 9.2.5 Image、Texture 与 Sampler

这三个词经常被混为一谈：

| 名词 | 它是什么 | 例子 |
| --- | --- | --- |
| Image | 图片的像素来源 | `wood.png` 解码出的 RGBA 像素 |
| Texture | GPU 中可被 Shader 读取的数据资源 | 一个 OpenGL 2D Texture |
| Sampler | 读取 Texture 时使用的规则 | 重复、夹取、线性过滤 |

同一张 Image 可以被不同 Texture 或不同材质引用；同一 Texture 也可以用不同 Sampler 规则读取。

#### 9.2.6 Node：场景层级中的节点

Node 表示场景层级中的一个对象。它可以保存局部变换，并可引用一个 Mesh。

~~~text
CarRoot
├── BodyNode       -> Body Mesh
├── FrontLeftWheel -> Wheel Mesh
├── FrontRightWheel-> Wheel Mesh
└── DoorNode       -> Door Mesh
~~~

父 Node 移动时，子 Node 会跟着移动。Node 解决“物体放在哪里、父子关系是什么”，Mesh 解决“物体长什么形状”。

---

### 9.3 Vertex Attribute：加载器实际要取出什么

第 1 章已经解释过：一条 Vertex 记录是同一个角点的全部 Attribute，不只是一个 position。本章关注这些 Attribute 在模型文件中怎样出现，以及第一版加载器怎样处理它们。

~~~cpp
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};
~~~

| glTF 语义 | 数据含义 | 第一版用途 |
| --- | --- | --- |
| `POSITION` | Local Space 中的位置 `(x,y,z)` | 必须有；决定几何形状 |
| `NORMAL` | 表面方向 `(x,y,z)` | 基础光照；缺失时拒绝或明确生成 |
| `TEXCOORD_0` | 第一套 UV `(u,v)` | 采样 Base Color Texture |
| `TANGENT` | UV 的 U 方向和副切线符号 `(x,y,z,w)` | Normal Map；第一版可暂不支持 |
| `COLOR_0` | 顶点颜色 | 可选着色或调试 |

不要给缺失数据随便填一个看似能运行的值。例如所有 Normal 都填 `(0,1,0)`，模型可能显示出来，但侧面光照一定错误。第一版更清楚的策略是：规定支持 `POSITION + NORMAL + TEXCOORD_0`，缺少必需项就输出模型名、Primitive 编号和缺失语义。

#### 同一 position 为什么可能对应多条 Vertex 记录

两个角点只有在全部 Attribute 相同的情况下才能共享一条 Vertex。立方体一个几何角连接三个面：position 相同，但三个面需要不同 Normal，因此要拆成三条记录。Texture 接缝两侧也可能 position 相同、UV 不同，同样必须拆开。

~~~text
position 相同 + normal 不同 -> 两条 Vertex
position 相同 + uv 不同     -> 两条 Vertex
全部 Attribute 相同         -> 可以共享
~~~

这就是为什么“立方体有 8 个几何角”不代表模型文件一定只有 8 条 Vertex 记录。

---

### 9.4 Index：资产格式与 Draw Call 必须使用同一种解释

Index 的基础作用已在第 1、7、8 章使用过：Index Array 保存 Vertex 编号，每连续三个编号组成一个 Triangle。本章新增的重点是，模型文件可能使用不同宽度的无符号整数：

| 索引格式 | 一个 Index 占用 | OpenGL Draw 类型 |
| --- | ---: | --- |
| 8-bit unsigned | 1 byte | `GL_UNSIGNED_BYTE` |
| 16-bit unsigned | 2 bytes | `GL_UNSIGNED_SHORT` |
| 32-bit unsigned | 4 bytes | `GL_UNSIGNED_INT` |

加载器可以保留原格式，也可以统一转换为 `std::uint32_t`。无论选择哪种，GPU Buffer 中真实字节、保存的 `indexCount` 和 `glDrawElements` 的类型参数必须一致。

把 16-bit 数据当作 32-bit 读取时，GPU 会按错误步长组合编号，常见画面是三角形拉成尖刺。上传前至少验证：

~~~text
indexCount 能构成当前 Primitive mode
每个 index < vertexCount
Draw 类型与上传后的索引类型一致
~~~

---

#### 本部分检查点

**能做出来**：用自己的话写出一张表：Model File、Mesh、Primitive、Material、Image、Texture、Sampler 各是什么，各举一个汽车上的例子。

**能回答**：

- 一个 Mesh 为什么可能要分成多个 Primitive？
- Image 和 Texture 的区别是什么？
- 为什么同一个 position 可能要保存成多条 Vertex？

**卡在这里就先不要往下读**：这七个词分不清，后面读 Accessor 和 BufferView 只会更糊。回到 9.2 重读一遍。

## 第二部分：把数据整理成运行时资源

### 9.5 顶点数据在内存中怎样排列

GPU Buffer 本质上是一段连续字节。OpenGL 不知道其中哪些字节是 position、哪些是 normal，必须由程序描述布局。

假设每个 `float` 占 4 字节：

~~~text
一个 Vertex

字节 0                                  字节 31
| position: 3 floats | normal: 3 floats | uv: 2 floats |
|      12 bytes      |     12 bytes     |   8 bytes    |

一个 Vertex 共 32 bytes
~~~

这里有两个关键名词：

- `stride`：从一个 Vertex 的起点走到下一个 Vertex 的起点，要跨过多少字节。
- `offset`：某个 Attribute 距离当前 Vertex 起点有多少字节。

本例中：

~~~text
stride          = 32 bytes
position offset = 0 bytes
normal offset   = 12 bytes
uv offset       = 24 bytes
~~~

OpenGL 配置示意：

~~~cpp
glVertexAttribPointer(
    0,                          // Shader 中 position 的 location
    3,                          // position 有 x、y、z 三个分量
    GL_FLOAT,                   // 每个分量是 float
    GL_FALSE,                   // 不做整数归一化
    sizeof(Vertex),             // stride：相邻 Vertex 的字节距离
    reinterpret_cast<void*>(offsetof(Vertex, position))
);

glVertexAttribPointer(
    1,
    3,
    GL_FLOAT,
    GL_FALSE,
    sizeof(Vertex),
    reinterpret_cast<void*>(offsetof(Vertex, normal))
);

glVertexAttribPointer(
    2,
    2,
    GL_FLOAT,
    GL_FALSE,
    sizeof(Vertex),
    reinterpret_cast<void*>(offsetof(Vertex, uv))
);
~~~

`offsetof` 比手写 `12` 或 `24` 更可靠，因为 C++ 结构体可能因对齐规则产生额外填充。

#### 9.5.1 Interleaved 与 Separate

上面的布局把一个 Vertex 的所有 Attribute 放在一起，称为 interleaved layout，交错布局。

另一种方式是分别保存：

~~~text
Buffer A：所有 position
Buffer B：所有 normal
Buffer C：所有 uv
~~~

这称为 separate layout。两种方式都能工作。学习项目可以统一使用交错布局，减少资源数量并便于理解。

---

### 9.6 CPU Mesh 与 GPU Mesh 应当分开

建议明确区分加载阶段的数据和绘制阶段的数据：

~~~cpp
struct CpuMeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct GpuMesh {
    GLuint vao = 0;
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    GLsizei indexCount = 0;
};
~~~

`CpuMeshData` 可以被日志、测试或碰撞生成使用；`GpuMesh` 保存 OpenGL Handle，供 Draw Call 使用。

典型上传流程是：

~~~text
CpuMeshData.vertices -> Vertex Buffer
CpuMeshData.indices  -> Index Buffer
布局描述             -> VAO
~~~

上传完成后是否释放 CPU 顶点数组，要看程序是否还需要它。不要因为 GPU 已有一份，就在不清楚用途时立刻删除所有 CPU 数据；也不要永远保留每份数据造成重复占用。

---

### 9.7 Texture 的完整旅程

#### 9.7.1 压缩图片不能直接当像素读

PNG 和 JPEG 是磁盘存储格式。文件中的字节经过了压缩，并不是按屏幕像素顺序直接排列。

~~~text
wood.png
    -> 图片解码器
width = 1024
height = 1024
channels = 4
pixels = CPU 上的 RGBA 字节数组
    -> glTexImage2D
GPU Texture
~~~

PNG 文件、CPU 像素数组和 GPU Texture 有各自独立的生命周期。

#### 9.7.2 Texel 与 Pixel 的区别

- Pixel 是最终 Framebuffer 或屏幕中的一个图像元素。
- Texel 是 Texture 中的一个图像元素。

一个屏幕 Pixel 可能采样一个或多个 Texel；一个 Texel 也可能被许多屏幕 Pixel 放大显示。

#### 9.7.3 Channel：通道

常见图片通道有：

~~~text
R       ：一个通道
RG      ：两个通道
RGB     ：红、绿、蓝
RGBA    ：红、绿、蓝、Alpha
~~~

上传格式必须和解码后的数据相符。把 RGB 数据按 RGBA 上传，会让 GPU 以错误步长读取后续像素。

#### 9.7.4 Wrap：UV 超出 0 到 1 后怎么办

常用方式：

| 模式 | 行为 | 常见用途 |
| --- | --- | --- |
| Repeat | 重复纹理 | 地砖、墙面 |
| Clamp to edge | 使用边缘 Texel | UI、后处理纹理 |
| Mirrored repeat | 镜像重复 | 避免明显重复方向 |

#### 9.7.5 Filter：采样点落在 Texel 之间怎么办

常见方式：

- Nearest：选最近 Texel，边缘像马赛克，适合像素风。
- Linear：邻近 Texel 加权平均，看起来更平滑。

放大过滤和缩小过滤可以分别设置，因为它们面对的问题不同。

---

### 9.8 颜色 Texture 与数据 Texture

不是所有 Texture 都表示“颜色”。这会直接影响颜色空间设置。

#### 9.8.1 颜色 Texture

Base Color、Albedo 等给人看的颜色图片通常使用 sRGB 编码。上传时应使用 sRGB 内部格式，例如带 Alpha 的图片可使用 `GL_SRGB8_ALPHA8`。

GPU 采样时会把 sRGB 编码值解码为线性值，之后才能正确做光照。第 11 章会详细解释原因。

#### 9.8.2 数据 Texture

下列贴图保存的是数值，不是显示颜色：

- Normal Map。
- Roughness Map。
- Metallic Map。
- Occlusion Map。
- Depth Texture。

这些 Texture 必须保持线性读取。若错误地按 sRGB 解码，法线方向和材质参数都会改变。

最实用的规则是：

~~~text
人眼要看的颜色图 -> 通常按 sRGB
Shader 要使用的数值图 -> 按线性数据
~~~

---

### 9.9 Mipmap：物体变远后为什么需要多级小图

假设一张 1024 x 1024 的棋盘格 Texture，被投影到屏幕上的 20 x 20 Pixel。此时一个屏幕 Pixel 大约覆盖许多 Texel。

如果 GPU 仍从原始大图中只取很少的采样点，稍微移动相机就可能取到完全不同的黑白格，造成闪烁和摩尔纹。

Mipmap 是同一张 Texture 的多级缩小版本：

~~~text
Level 0: 1024 x 1024
Level 1:  512 x  512
Level 2:  256 x  256
...
Level 10:   1 x    1
~~~

GPU 根据 Texture 在屏幕上的缩小程度选择合适级别。`GL_LINEAR_MIPMAP_LINEAR` 还会在相邻 Mipmap 级别之间插值，常被称为 trilinear filtering。

创建 Texture 后可以生成 Mipmap：

~~~cpp
glGenerateMipmap(GL_TEXTURE_2D);
~~~

但这只有在 Level 0 已正确上传、尺寸和内部格式有效时才有意义。若缩小过滤设置为需要 Mipmap 的模式，却没有提供完整 Mipmap，Texture 可能采样为黑色或被判定为不完整。

---

#### 本部分检查点

**能做出来**：程序能把一个模型文件读成 CPU 数组，再上传成 GPU Buffer 和 Texture，并打印顶点数、三角形数和 Attribute 布局。

**能回答**：

- stride 和 offset 分别是什么？
- CPU Mesh 和 GPU Mesh 为什么要分开？
- Mipmap 解决了什么现象？

**卡在这里就先不要往下读**：模型不显示时，用 9.1 的五问依次排查，不要同时改解析器和 Shader。

## 第三部分：从 glTF 到 Draw Call

### 9.10 为什么本书选择 glTF

glTF 的目标是高效传输和加载 3D 场景，适合作为运行时资产格式。它可以描述 Mesh、Material、Texture、Node、动画等数据之间的关系。

常见扩展名：

| 格式 | 内容组织 |
| --- | --- |
| `.gltf` | JSON 描述，可能引用外部 `.bin` 和图片 |
| `.glb` | JSON、二进制数据和图片可以打包在一个文件中 |

本书建议用 `cgltf` 或 `fastgltf` 读取格式，而不是自己编写 JSON、Base64、图片和二进制解析器。使用库并不会隐藏渲染原理；库只负责把合法文件解释成结构化数据，最终怎样转换成你的 Vertex、Mesh 和 Material，仍由你决定。

---

### 9.11 glTF 的核心结构怎样连接

第一次看到 glTF API 时，最容易困惑的是 Buffer、BufferView 和 Accessor。先看整体关系：

~~~text
Scene
  -> Node
       -> Mesh
            -> Primitive
                 -> Attribute Accessor，例如 POSITION、NORMAL、TEXCOORD_0
                 -> Index Accessor

Accessor
  -> BufferView
       -> Buffer 中的一段连续字节

Material
  -> Texture
       -> Image
       -> Sampler
~~~

#### 9.11.1 Buffer：原始字节仓库

Buffer 是一大片原始二进制字节。它本身不说明里面的数据是 position 还是 index。

#### 9.11.2 BufferView：Buffer 中的一段范围

BufferView 表示 Buffer 的某一段：从哪个字节开始、有多长、相邻元素可能隔多远。

可以把 Buffer 想成一本大书，BufferView 像“第 20 页到第 30 页”。

#### 9.11.3 Accessor：怎样解释这一段字节

Accessor 说明 BufferView 中的数据应被解释成什么：

- `count`：有多少个元素。
- `type`：每个元素是标量、Vec2、Vec3、Vec4 还是矩阵。
- `componentType`：每个分量是 float、无符号短整数等哪种类型。
- `byteOffset`：Accessor 自己在 BufferView 内的起始偏移。
- `normalized`：整数是否要映射到 0 到 1 或 -1 到 1。
- `min/max`：可选范围信息，常用于 bounds。

例如 POSITION Accessor 可能表示：

~~~text
count = 1200
type = VEC3
componentType = FLOAT
~~~

意思是“一共有 1200 个位置，每个位置由 3 个 float 组成”。

#### 9.11.4 最终地址不是一个 offset

读取某个元素时，真实地址通常由几部分组成：

~~~text
元素地址 = Buffer 起点
         + BufferView.byteOffset
         + Accessor.byteOffset
         + elementIndex * stride
~~~

`stride` 若由 BufferView 提供，就必须使用它；不能永远假设数据紧密排列。

这正是成熟 glTF 库的辅助访问函数很重要的原因。优先使用库提供的迭代或 unpack 接口，不要在不了解布局时直接把字节 `reinterpret_cast` 成自己的 `Vertex*`。

Accessor 还可能使用 Sparse Storage，稀疏存储：先提供一份基础数据，再只记录少量被替换的元素。若直接读基础 BufferView，会漏掉这些覆盖值。成熟库的 unpack/iterate 接口通常会帮你合并；选择加载库时要确认该能力。

---

### 9.12 把 glTF 数据转换成自己的格式

渲染器不应在每次 Draw 时到 glTF 对象中查找 Accessor。加载阶段就应把不同来源统一成自己的运行时格式。

~~~cpp
struct CpuPrimitive {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    MaterialHandle material;
};
~~~

转换伪代码：

~~~cpp
CpuPrimitive convertPrimitive(const GltfPrimitive& source) {
    CpuPrimitive result;

    const Accessor* positions = findAttribute(source, "POSITION");
    const Accessor* normals   = findAttribute(source, "NORMAL");
    const Accessor* uvs       = findAttribute(source, "TEXCOORD_0");

    result.vertices.resize(positions->count);

    for (std::size_t i = 0; i < positions->count; ++i) {
        result.vertices[i].position = readVec3(*positions, i);
        result.vertices[i].normal = normals
            ? readVec3(*normals, i)
            : Vec3{0.0f, 1.0f, 0.0f};
        result.vertices[i].uv = uvs
            ? readVec2(*uvs, i)
            : Vec2{0.0f, 0.0f};
    }

    result.indices = source.indices
        ? readAndConvertIndices(*source.indices)
        : makeSequentialIndices(positions->count);
    result.material = convertMaterial(source.material);
    return result;
}
~~~

逐段解释：

- `findAttribute` 根据 glTF 语义找数据，而不是假定 Attribute 顺序。
- `resize` 为最终统一顶点格式分配空间。
- `readVec3` 必须尊重 Accessor 的 offset、stride 和分量类型。
- POSITION、NORMAL、TEXCOORD 的 count 应匹配；不匹配时应报告资产错误，不能越界读取。
- 可选 Attribute 不存在时要有明确策略，而不是访问空指针。
- glTF 的 Index Accessor 可以不存在；这表示按顶点顺序直接绘制。这里生成 `0..vertexCount-1`，统一成索引路径。
- 所有索引统一转换成 `uint32_t`，让运行时路径更简单；代价是占用稍多内存。
- Material 也应转换成自己的 Handle，而不是让 Renderer 依赖 glTF 库结构。

“缺少 normal 时写固定向上法线”只适合先让模型显示。更合理的后续方案是根据三角形计算法线，或明确拒绝不满足资源约定的模型。

---

### 9.13 Node 变换与层级

一个 Node 的变换通常以以下两种方式之一保存：

- 一张完整的 4 x 4 matrix。
- Translation、Rotation、Scale，简称 TRS。

若使用 TRS，常见局部矩阵组合为：

~~~text
M_local = T * R * S
~~~

其中：

- `T` 是平移矩阵。
- `R` 是旋转矩阵。
- `S` 是缩放矩阵。

子节点的世界矩阵为：

~~~text
M_world_child = M_world_parent * M_local_child
~~~

根节点没有父节点，可把父世界矩阵视为单位矩阵。

递归遍历示意：

~~~cpp
void visitNode(const Node& node, const Mat4& parentWorld) {
    Mat4 world = parentWorld * node.localMatrix;

    if (node.hasMesh()) {
        createRenderItems(node.mesh, world);
    }

    for (const Node& child : node.children) {
        visitNode(child, world);
    }
}
~~~

矩阵相乘顺序取决于你的数学库使用列向量还是行向量。本书后续示例以常见 GLSL 列向量形式 `clip = P * V * M * position` 为准。不要只把乘法顺序反过来“试到能显示”，应统一整个项目的约定。

#### 9.13.1 坐标系约定只能转换一次

glTF 使用右手坐标系、`+Y` 为上方，长度单位按米理解，正面三角形默认使用逆时针顶点顺序。glTF Camera 的局部观察方向是 `-Z`。

OpenGL API 本身不会替你的世界规定“哪边是前、一个单位有多长”。如果项目采用另一套约定，应在资产导入边界统一转换 Position、Normal、Tangent、Node Transform 和 Triangle Winding，之后运行时只使用项目自己的规则。

只翻转 Position 的一个轴，却忘记同步处理 Normal、Tangent 或索引绕序，会造成镜像、光照反向或背面剔除错误。不要在加载器、Renderer 和 Shader 三处各自偷偷翻一次轴。

图片原点也要统一。若图片解码器做了垂直翻转，就不应再在 Shader 中写 `uv.y = 1 - uv.y`。用带有方向文字的测试 Texture 明确验证一次，并记录项目约定。

---

### 9.14 一个模型从文件到 Draw Call 的完整流程

~~~text
打开 glTF 或 GLB
    |
    v
解析 JSON 与二进制关系
    |
    v
遍历 Scene 和 Node
    |
    v
读取 Mesh Primitive
    |
    v
Accessor 转成统一 Vertex 与 Index
    |
    v
图片解码成 CPU Pixels
    |
    v
创建 GPU Buffer 与 Texture
    |
    v
创建 Material 与 Mesh Handle
    |
    v
Node 世界矩阵生成 RenderItem
    |
    v
Renderer 提交 Draw Call
~~~

可以把加载器拆成三层：

~~~text
格式层：理解 glTF、Accessor、Image 引用
转换层：生成项目自己的 CpuMesh、CpuImage、MaterialDesc
上传层：创建 OpenGL Buffer、Texture、VAO
~~~

这样做的好处是，可以单独测试“格式读对了吗”和“GPU 上传对了吗”。

---

### 9.15 资源 Handle 与生命周期

`GLuint` 是 OpenGL 对象编号，不等于完整资源。实际 Texture 还需要知道：

~~~cpp
struct GpuTexture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    TextureFormat format = TextureFormat::RGBA8;
    bool isSrgb = false;
    int mipLevels = 1;
};
~~~

记录这些信息能回答：

- Resize 或重载时该创建什么格式？
- 这张图是否按 sRGB 读取？
- 是否真的生成了 Mipmap？
- 内存统计应怎样计算？

资源生命周期至少要有清晰的拥有者：

~~~text
加载 -> 创建 -> 被 Material/Mesh 引用 -> 不再使用 -> 删除 GPU 对象
~~~

常见错误有两种：

- 太早删除：Material 仍引用 Texture，但 Texture 已被 `glDeleteTextures` 删除。
- 永不删除：反复切换关卡或重载资产后，GPU 内存不断增长。

第一版可以让一个 ResourceManager 统一拥有 Mesh 和 Texture，程序结束时再集中释放。后续再增加引用计数或异步加载。

---

### 9.16 推荐的增量实现顺序

不要第一次就加载带骨骼、动画、透明和多种 PBR 贴图的大型角色。PBR 是 Physically Based Rendering，基于物理的渲染，它会使用金属度、粗糙度等参数描述材质。按下面顺序，每一步都保留可观察结果：

1. 读取一个只有 POSITION 的单 Primitive 模型，使用固定白色绘制。
2. 支持索引，并打印顶点数、索引数和三角形数。
3. 支持 NORMAL，用伪颜色显示法线。
4. 支持 TEXCOORD_0，用棋盘格 Texture 验证 UV。
5. 加载 Base Color Image，验证 sRGB 设置和上下方向。
6. 支持一个 Mesh 的多个 Primitive 和不同 Material。
7. 支持 Node 层级和世界矩阵。
8. 最后才增加 Normal Map、Metallic/Roughness 或动画。

每增加一层，只改变一个变量。这样出现黑屏时，你知道刚刚加入了什么。

---

### 9.17 常见现象、可能原因与检查顺序

| 现象 | 常见原因 | 首先检查 |
| --- | --- | --- |
| 完全看不到模型 | 文件失败、Draw 未提交、矩阵或 bounds 错 | 打印加载数量并用固定 Shader |
| 模型像尖刺一样爆炸 | stride、offset、索引类型错误 | 打印前 10 个 position/index |
| 形状正确但全白 | Texture 未绑定、Sampler uniform 错 | 显示固定 UV 棋盘格 |
| 贴图上下颠倒 | 图片原点与 UV 约定不同 | 检查解码器是否翻转图片 |
| 模型全黑 | normal 缺失/错误、光方向或颜色空间错误 | 先关闭光照输出 albedo |
| 光照像碎片一样断裂 | normal 读取错或硬边顶点共享错误 | 把 normal 映射成 RGB |
| 远处贴图闪烁 | 未生成 Mipmap 或 min filter 错 | 检查 Mipmap 级别与过滤模式 |
| 部件位置散开 | Node 父子矩阵顺序错误 | 绘制每个 Node 的坐标轴 |
| 只有部分模型出现 | 多 Primitive 未遍历或材质为空 | 输出每个 Mesh 的 Primitive 数 |

#### 9.17.1 法线伪颜色

法线范围通常是 -1 到 1，而颜色显示范围是 0 到 1，因此可这样转换：

~~~glsl
vec3 debugColor = normalize(vNormal) * 0.5 + 0.5;
~~~

若颜色出现随机噪点或随相机移动跳变，先修复 normal 数据，不要继续调灯光。

#### 9.17.2 UV 伪颜色

~~~glsl
outColor = vec4(fract(vUv), 0.0, 1.0);
~~~

它能直接显示 UV 是否连续、是否超出 0 到 1，以及是否完全为零。

---

### 9.18 练习

#### 练习 A：手算顶点布局

给定：

~~~cpp
struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};
~~~

写出 position、normal、uv 的分量数、类型、stride 和 offset。再说明如果把 uv offset 错写成 20 字节，GPU 会读到什么数据。

#### 练习 B：读取模型统计

加载一个简单 glTF，输出：

- Scene 数量。
- Node 数量。
- Mesh 数量。
- Primitive 数量。
- 每个 Primitive 的 Vertex、Index、Triangle 数量。
- 每张 Image 的尺寸和通道数。
- 每个 Attribute 的 component type、count 和 stride。

#### 练习 C：分层显示

为同一个模型依次实现四种调试模式：

1. 固定纯色。
2. Normal 伪颜色。
3. UV 伪颜色。
4. Base Color Texture。

记录每种模式分别能证明数据链的哪一段是正确的。

#### 练习 D：Mipmap 对比

让一个高频棋盘格平面延伸到远处，分别使用：

- Nearest，无 Mipmap。
- Linear，无 Mipmap。
- Linear + Mipmap。

移动相机并观察远处闪烁差异。

#### 练习 E：故意出错实验（破坏资产数据流）

在你的加载器上，每次只改一处，先预测，再运行，然后恢复：

1. 把 Vertex 的 stride 改成实际值的一半。
2. 把 normal 的 offset 和 uv 的 offset 对调。
3. 索引类型按 `GL_UNSIGNED_SHORT` 上传，但 Buffer 里存的是 uint32。
4. 把一个 Face 的索引改成超出顶点数量的值（用 `.at()` 而不是 `[]`，看它在哪一层报出来）。
5. 上传 Texture 后不生成 Mipmap，但 min filter 仍设为 mipmap 模式。
6. 只遍历每个 Mesh 的第 0 个 Primitive。

每一条都要说明：这个错误是在**文件、CPU 数组、GPU 资源还是 Draw Call** 哪一层引入的？这正是本章 9.1 那条「文件 -> CPU -> GPU -> Draw Call」链的用处。

第 5 条值得注意：它不会报错，画面在近处也完全正常，只有远处才闪烁。

---

### 9.19 本章验收标准

**最低验收**（第一次必须完成）：加载一个**只有单个 Node、单个 Mesh、单个 Primitive 和一张 Base Color Texture** 的模型，形状和贴图方向正确，并能说出数据在文件、CPU、GPU 三个阶段分别长什么样。只做到这一步就可以进入第 10 章。

**推荐验收**：下面全部七条。

**扩展验收**（可留到第二遍）：多 Primitive、缺失属性的容错、Normal Map、接入 `fastgltf` 替换示例解析器。

完整的推荐验收——你的程序应能加载一个包含索引、normal、UV 和 Base Color Texture 的静态模型，并满足：

1. 模型形状正确，没有尖刺或缺面。
2. Node 层级中的各部件位置正确。
3. 贴图方向正确，远处使用 Mipmap 后明显减少闪烁。
4. 可以切换纯色、normal、UV、texture 四种调试显示。
5. 控制台能输出模型统计和 Attribute 布局。
6. 你能从磁盘文件开始，完整解释数据怎样进入 CPU、GPU，最后参与 Draw Call。

在进入下一章前，请确认自己能回答：

- Mesh 和模型文件有什么区别？
- Primitive 为什么通常对应一次 Draw Call？
- stride 和 offset 分别是什么？
- Buffer、BufferView 和 Accessor 各负责什么？
- 为什么同一 position 可能需要多个 Vertex？
- 为什么 Base Color 与 Normal Map 不能使用相同的颜色空间设置？

下一章会把已加载的 Mesh 放入一个真正的场景，并学习相机如何从大量对象中筛选本帧可能看见的部分。
