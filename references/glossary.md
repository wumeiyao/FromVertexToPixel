# 术语表

这个文件用于快速回查，不替代各章节的完整解释。英文术语保留，因为图形 API、调试工具和资料通常使用英文。

## A-D

- AABB：Axis-Aligned Bounding Box，轴对齐包围盒；各边始终平行于坐标轴的盒子，比较最小/最大坐标即可快速判断是否可能重叠。
- Accessor：glTF 中描述“怎样把一段 Buffer 字节解释成一组标量或向量”的结构，包含分量类型、元素类型、数量和偏移等信息。
- Accumulator：Game Loop 中累积“还没有被固定更新消耗掉的时间”的变量；它决定这一帧执行几次 Fixed Time Step。
- Alpha：颜色的第四个通道，常用于表示透明覆盖比例。Alpha 为 0 通常完全透明，为 1 通常完全不透明。
- Alpha Blending：将新 Fragment 颜色与 Framebuffer 已有颜色按 Alpha 混合。
- API：程序调用某套系统功能的接口。OpenGL、Vulkan 和 Metal 都是图形 API。
- Asset：由文件或程序生成、可被游戏系统复用的资源，例如 Mesh、Texture、Shader 和声音。
- Asset Handle：应用用来间接查找 Asset 的轻量编号；它可以引用共享资源，但通常不代表独占所有权。
- Asset Manager：负责加载、缓存、按 Handle 查找并统一释放 Asset 的模块。
- Attachment：挂接到 Framebuffer、供 Draw 写入或测试使用的 Color、Depth、Stencil 存储。
- Attribute：每个 Vertex 自己携带的数据，例如 position、normal、UV。
- Back Buffer：双缓冲中正在被 GPU 绘制、暂时还没有显示的那张画面。
- Back-face Culling：根据 Triangle 顶点绕序跳过背向相机的面。
- Barrier：显式图形 API 中声明“这段访问必须等前一段完成、或需要转换资源状态”的同步与转换命令。
- Base Color：材质的基础颜色，常由一张 sRGB Texture 和一个颜色系数共同决定；不含光照结果。
- Batch：被合并为一次或少数几次提交的一组绘制工作，例如共享 Mesh/Material 的 Instance 集合。
- Bias：为抵消浮点或深度误差而加入的小偏移；阴影中常用于避免表面错误遮挡自己。
- Blend State：控制新 Fragment 如何与已有颜色组合的 GPU 状态。
- Blinn-Phong：用半角向量 H 近似镜面高光的经典光照模型；比 PBR 简单，适合先看清每份数据怎样参与计算。
- Bloom：把画面中超出显示范围的高亮部分提取、模糊后叠回原图，模拟强光在眼睛和镜头中扩散的后处理效果。
- Broad Phase：物理碰撞检测的粗筛阶段，使用简单 Bounds 快速找出可能相交的对象对。
- Buffer：连续存放数据的内存区域。GPU Buffer 可保存 Vertex、Index、Uniform 等数据。
- BufferView：glTF 中对某个 Buffer 连续字节范围的描述，Accessor 在它的基础上继续说明元素格式。
- Camera：描述从哪里看、朝哪里看、使用什么投影和 Viewport 的数据集合。
- Clip Space：顶点 Shader 输出位置所在的齐次坐标空间，用于裁剪可见范围。
- Command Buffer：显式图形 API 中预先记录的一组 GPU 命令。
- Context：OpenGL 的运行环境，保存当前线程使用的 OpenGL 状态并连接驱动。
- Coordinate Space：用一个原点和若干坐标轴规定“坐标相对于谁描述”的参考系。
- CPU：执行游戏逻辑、组织数据并提交 GPU 命令的通用处理器。
- Cubemap：由六个面组成的 Texture，用一个方向向量而不是普通 UV 采样；常用于 Skybox 和环境反射。
- Culling：在绘制前排除不可能可见的对象或 Triangle。
- Delta Time：两次更新之间经过的秒数。
- Depth Buffer：每个 Sample 保存当前最近深度的缓冲。
- Depth Test：比较新 Fragment 深度与 Depth Buffer，决定它是否被遮挡。
- Depth Write：是否把通过测试的 Fragment 深度写回 Depth Buffer；透明绘制通常开启 Depth Test 但关闭 Depth Write。
- Draw Call：CPU 提交给 GPU 的一次绘制命令。
- Draw Framebuffer：当前 Draw 命令写入的 Framebuffer；可以是窗口的默认 Framebuffer，也可以是离屏 FBO。
- Dynamic Body：由物理模拟更新位置、速度并参与碰撞响应的物体。

## E-M

- EBO / Index Buffer：保存顶点索引的 GPU Buffer。OpenGL 中常称 Element Buffer Object。
- Entity：场景中一个具体游戏对象的标识或实例，通常由 Transform 和若干 Component 组成。
- FBO：OpenGL Framebuffer Object，用于把 Texture 或 Renderbuffer 组织为绘制目标。
- Fence：用于确认 GPU 某段工作是否已经完成的同步对象。
- Fixed Time Step：用固定时长推进一次游戏或物理模拟的策略；一次渲染帧可执行零次、一次或多次固定更新。
- FOV：Field of View，视野角；透视相机张开的角度，决定画面的广角或长焦感受。
- FPS：Frames Per Second，每秒完成的 Frame 数。
- Fragment：Triangle 覆盖某个 Sample 后产生的候选数据，尚未必成为最终 Pixel。
- Frame：一次完整渲染得到的画面。
- Frame in Flight：已提交但 GPU 尚未完成的帧；它决定同一份资源多久之后才能安全改写。
- Frame Slot：为每个在途帧准备的一套独立资源（Command Buffer、Uniform、Fence），避免 CPU 改写 GPU 正在读的数据。
- Framebuffer：一组颜色、深度等绘制附件；可以对应窗口，也可以离屏。
- Frustum：相机的可见体积，透视相机通常像截去顶端的四棱锥。
- GLAD：OpenGL 函数加载器，在 Windows/Linux 上运行时查询函数地址；macOS 直接使用系统 OpenGL 头，不需要它。
- GLFW：跨平台的窗口、Context 与输入库；不负责渲染、模型加载和游戏逻辑。
- GLSL：OpenGL Shading Language，编写 OpenGL Shader 的语言；本书使用 GLSL 410。
- GPU：大量并行执行 Vertex 和 Fragment 工作的处理器。
- HDR：High Dynamic Range，可暂存大于普通显示范围的亮度。
- Image：显式图形 API 中保存一维、二维或三维 texel 数据的资源；Texture 的底层存储概念之一。
- Index：指向 Vertex 数组元素的编号；每三个 Index 常组成一个 Triangle。
- Instance：共享 Mesh/Material、但拥有不同 Transform 等实例数据的一次对象副本。
- Interpolation：根据端点或顶点数据计算中间值；渲染中可用于颜色、UV、Normal 等属性。
- Kinematic Body：由游戏代码给出运动目标、但仍参与物理查询或碰撞的物体。
- Linear Color Space：颜色数值与物理光强近似成线性关系的计算空间。
- Load / Store：Pass 开始时如何取得 Attachment 已有内容（Load 或 Clear），以及结束时是否保留写入结果（Store）。
- Local Space：相对于模型自身原点描述位置的坐标空间。
- LOD：Level of Detail，细节层级；远处对象换用更少三角形的版本以减少开销。
- Material：Shader 加参数和 Texture 的组合，描述表面怎样着色。
- Matrix：按行列排列的数字，可表示旋转、缩放、平移和投影等变换。
- Mesh：Vertex 与 Index 组成的几何形状数据。
- Mipmap：同一 Texture 的多级缩小版本，用于远处采样和减少闪烁。
- Model：语境可能指模型文件或渲染对象；应结合是否包含 Mesh、Material、节点来判断。
- MSAA：在一个 Pixel 内使用多个 Sample 改善 Triangle 边缘锯齿。
- Narrow Phase：物理碰撞检测的精确阶段，对 Broad Phase 筛出的对象对使用具体 Shape 求接触或重叠。

## N-S

- NDC：Normalized Device Coordinates，透视除法后的标准坐标；OpenGL 中各轴常在 -1 到 1。
- Normal：表示表面朝向的单位方向，用于光照。
- Normal Map：把表面细节朝向编码成 Texture 的数据贴图；属于数据 Texture，不能按 sRGB 解释。
- Normal Matrix：变换 Normal 用的矩阵，等于 Model Matrix 左上 3×3 的逆转置；非均匀缩放时必须用它，否则明暗会扭曲。
- Offset：某个字段或 Attribute 相对一条记录起点的字节距离。
- Opaque：不透明；通常写颜色和深度，不依赖已有颜色进行混合。
- Overdraw：同一个 Pixel 被多个后来又被遮住的 Fragment 反复着色，属于被浪费掉的片段开销。
- Pass：拥有明确输入、输出和 GPU 状态的一段渲染工作。
- PBR：Physically Based Rendering，基于更接近物理规律的材质和光照模型。
- PCF：Percentage-Closer Filtering，通过多次阴影深度比较柔化阴影边缘。
- PCM：Pulse-Code Modulation，用离散时间 Sample 数值表示数字声音的常见形式。
- Perspective Division：将 Clip 坐标的 x、y、z 除以 w 得到 NDC。
- Pipeline：显式图形 API 中预先组合 Shader、Vertex Layout、Raster、Depth、Blend 和目标格式等状态的对象。
- Pixel：图片或 Framebuffer 中最终保存结果的离散位置。
- PPM：一种极简的图片格式，头部是文字、像素是原始数值；适合作为 CPU 光栅器的第一种输出。
- Primitive：GPU 组装和光栅化的基本几何单元，例如 Point、Line 或 Triangle。
- Program：OpenGL 中多个 Shader Stage 编译并链接后的可执行组合。
- RAII：Resource Acquisition Is Initialization，资源获取即初始化；拥有资源的 C++ 对象生命周期结束时自动释放资源。
- Rasterization：根据 Triangle 覆盖关系生成 Fragment 的过程。
- Render Item：Renderer 绘制一个对象所需的简化数据，如 Mesh、Material、Transform、Bounds。
- Render Queue：等待按某种规则绘制的一组 Render Item，例如 Opaque Queue 和 Transparent Queue。
- Render Snapshot：游戏更新完成后交给 Renderer 的本帧只读表现数据，不是整个 Game World 的深拷贝。
- Render Target：当前 Pass 写入的颜色或深度目标。
- Renderbuffer：可作为 Framebuffer 附件、但通常不能像 Texture 一样被 Shader 采样的存储。
- RenderDoc：常用的 GPU 截帧调试工具，可逐条查看命令和资源；不支持 macOS 的 OpenGL 路径。
- Sample：Pixel 内进行覆盖、深度和模板判断的位置。
- Scene：某一时刻世界中的对象、灯光、Camera 等集合。
- Scene Color：主场景 Pass 写出的颜色结果，通常是一张离屏 Texture，供透明、后处理和调试读取。
- Scissor：限制绘制只影响 Framebuffer 中某个矩形区域的 GPU 状态；常用于 UI 裁剪。
- Semaphore：显式 API 中安排不同 GPU 工作先后关系的同步对象。
- Shader：运行在 GPU 上的小程序。
- Shader Descriptor：显式 API 中声明或绑定 Shader 所读 Buffer、Image 与 Sampler 的资源接口；Vulkan 常使用 Descriptor Set。
- Shadow Acne：表面因深度比较误差而错误遮挡自己，表现为密集条纹或噪点。
- Shadow Map：从光源视角渲染得到的深度 Texture；主 Pass 用它查询某个表面是否被遮挡。
- Skybox：表现无限远背景的绘制路径；采样 Cubemap，并移除 View Matrix 的平移部分。
- Snapshot：游戏更新结束后交给 Renderer 的只读场景状态。
- Solver：物理引擎根据接触和约束修正速度、位置并计算碰撞响应的阶段。
- Sort Key：把排序规则（Pass、材质、深度等）编码成一个可比较数值，用于 Render Queue 排序。
- sRGB：用于图片和显示器的非线性颜色编码；通常不能直接进行物理光照计算。
- SSBO：Shader Storage Buffer Object，OpenGL 4.3 引入的可读写大型 Shader Buffer；macOS OpenGL 4.1 不支持。
- Static Body：固定不动、通常不受力推动的物理对象，例如地面和墙。
- Stride：从一条 Vertex 记录开头走到下一条同类记录开头的字节数。
- Submesh：一个 Mesh 中使用特定 Material 或独立索引范围绘制的部分。
- Sweep and Prune：Broad Phase 的一种做法，沿坐标轴排序区间并排除不重叠的对象对。

## T-Z

- Texel：Texture 中的一个数据格，类似 Texture 自己的 Pixel。
- Texture：可由 Shader 按坐标读取的二维或多维数据，可保存图片、深度或中间结果。
- Texture Unit：OpenGL Context 中用于把 Texture 绑定给 Shader Sampler 的编号槽位。
- Tick：一次固定步长的游戏或物理更新；一个渲染帧可能对应零次、一次或多次 Tick。
- Tone Mapping：把 HDR 颜色压缩到普通显示范围。
- Transform：对象的位置、旋转和缩放，或表示这些变换的矩阵。
- Transparent：透明或半透明，通常需要颜色混合和排序。
- Triangle：由三个不共线 Vertex 构成的基本图元。
- Trigger：只用于检测进入/离开、不产生碰撞阻挡的物理体，常用于收集物和区域事件。
- Uniform：一次 Draw Call 中对许多 Vertex/Fragment 共享的小量 Shader 参数。
- UV：Texture 上的相对坐标，常使用 0 到 1。
- VAO：Vertex Array Object，OpenGL 中记录 Vertex Attribute 如何从 Buffer 读取的对象。
- VBO：Vertex Buffer Object，OpenGL 中保存 Vertex 数据的 Buffer。
- Vertex：Triangle 的角及其属性数据，不是 Pixel。
- Vertex Fetch：GPU 按 Index、Vertex Layout、Offset 和 Stride 从 Buffer 取得 Vertex Attribute 的阶段。
- Vertex Shader：对每个输入 Vertex 执行，最重要的输出是 Clip Space 位置。
- Vertical Slice：包含最终玩法全部环节、但内容量极小的可玩版本；用于先验证整条链再扩充内容。
- View Space：以 Camera 为参考系描述对象位置的坐标空间。
- Viewport：把 NDC 映射到 Framebuffer 某个矩形区域的规则。
- VSync：垂直同步；让 Buffer Swap 等待显示器刷新边界，以减少画面撕裂。
- World Space：所有场景对象共享的世界坐标空间。

