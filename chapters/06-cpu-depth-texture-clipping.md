# 第 6 章：深度、纹理、透视校正与裁剪

<!-- nav -->
> [上一章：CPU 三角形光栅化](05-cpu-triangles.md) ｜ [下一章：OpenGL Context 与 GPU 资源](07-opengl-context-resources.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/02_cpu_rasterizer/06-cpu-depth-texture-clipping.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

第 5 章已经能把一个三角形填进颜色缓冲，但它仍像一张彩色纸片：不知道谁挡住谁，也不能稳定地贴图，更无法正确处理穿过相机近平面的几何。

本章会补齐 CPU 光栅器的三个关键能力：可见性、表面图案和视锥边界处理。

本章分三部分：`6.2～6.6` 用 Depth Buffer 解决遮挡；`6.7～6.16` 采样 Texture 并修正透视插值；`6.17～6.21` 裁剪跨越视锥边界的 Triangle。先让纯色三角形遮挡正确，再加 Texture，最后处理裁剪，不要同时调试三类问题。

可以把本章当成三个连续的小项目：

- 最小通过线：完成 Depth Buffer 和 Nearest Texture，能画遮挡正确的贴图 Triangle。
- 建议通过线：再完成 Bilinear 和透视正确插值，倾斜贴图不变形。
- 完整通过线：最后完成近平面裁剪，再扩展到六个 Clip Plane。

如果某一阶段出错，就停在该阶段，不要提前把后面的代码也接进来。

## 本章导读

### 6.1 本章学习目标

完成本章后，应能：

1. 解释颜色缓冲与深度缓冲各保存什么。
2. 实现清深度、深度比较和条件写入。
3. 区分 Texture、texel、UV、Pixel 与 Fragment。
4. 实现 nearest 和 bilinear 两种纹理采样。
5. 从投影造成的非线性解释透视正确插值公式。
6. 对穿过 Clip Plane 的三角形生成交点和新三角形。

## 第一部分：用深度决定谁在前面

### 6.2 一个 Pixel 位置可以收到多个 Fragment

假设红墙和绿箱投影后都覆盖 Pixel `(100,80)`。光栅器会为同一位置产生两个 Fragment。颜色缓冲只能保存一个最终结果，所以还需要知道哪个 Fragment 更靠近相机。

~~~text
相机 -> 绿箱 -> 红墙

同一个屏幕位置：
绿 Fragment 更近，应保留
红 Fragment 更远，应丢弃
~~~

只依靠绘制顺序会得到“后画的总在上面”，这对三维遮挡不可靠。深度测试让结果由空间远近决定。

### 6.3 Depth Buffer 是与颜色缓冲同尺寸的数组

Depth Buffer（深度缓冲）为每个 Pixel 保存当前已通过测试的最近深度值：

~~~cpp
struct Framebuffer {
    int width = 0;
    int height = 0;
    std::vector<Color> color;
    std::vector<float> depth;
};
~~~

两个数组使用相同索引 `y*width+x`：

~~~text
color[index]：当前可见颜色
depth[index]：产生该颜色的深度比较值
~~~

在本书的普通 OpenGL 深度约定中，Viewport 之后深度位于 `[0,1]`，0 靠近 near，1 靠近 far。因此每帧开始把深度清为 1：

~~~cpp
void clear(Framebuffer& fb, Color clearColor, float clearDepth = 1.0f) {
    std::fill(fb.color.begin(), fb.color.end(), clearColor);
    std::fill(fb.depth.begin(), fb.depth.end(), clearDepth);
}
~~~

Depth Buffer 必须在创建时也分配 `width*height` 个元素。

### 6.4 深度测试与深度写入是两个动作

对一个新 Fragment：

~~~cpp
bool depthTestAndWrite(Framebuffer& fb,
                       int x, int y,
                       float fragmentDepth,
                       Color fragmentColor) {
    if (x < 0 || y < 0 || x >= fb.width || y >= fb.height) {
        return false;
    }

    std::size_t index = static_cast<std::size_t>(y) * fb.width + x;

    // Less：较小深度表示较近。
    if (fragmentDepth < fb.depth[index]) {
        fb.depth[index] = fragmentDepth;
        fb.color[index] = fragmentColor;
        return true;
    }
    return false;
}
~~~

步骤不能调换：先比较，只有通过时才同时写深度和颜色。

数值例子：

~~~text
初始 depth=1.0，color=背景
红墙 depth=0.8：0.8<1.0，通过，写红色与 0.8
绿箱 depth=0.3：0.3<0.8，通过，写绿色与 0.3
蓝物 depth=0.6：0.6<0.3 不成立，丢弃

最终颜色=绿色，最终深度=0.3
~~~

即使按蓝、绿、红的顺序绘制，最终仍是绿色。这就是不透明物体通常不依赖提交顺序的原因。

### 6.5 深度值不是世界距离

常见透视投影会把 View Space 深度非线性映射到深度缓冲。Depth `0.5` 不表示“在 near 与 far 的米数正中间”。更多精度集中在相机附近。

本书 OpenGL 映射为：

~~~text
depth = ndcZ * 0.5 + 0.5
~~~

- `ndcZ=-1`（near）映射为 0。
- `ndcZ=1`（far）映射为 1。

光栅化时可对三个顶点的 NDC z 使用屏幕重心权重线性插值：

~~~text
fragmentNdcZ = alpha*z0 + beta*z1 + gamma*z2
fragmentDepth = fragmentNdcZ*0.5 + 0.5
~~~

这里的 `z0..z2` 必须都是同一阶段的 NDC z，不能混入世界米数或 Clip z。

若忘记每帧清 Depth Buffer，上一帧的近值会挡住本帧新位置，出现洞、拖影或物体莫名缺块。

### 6.6 Z-fighting 与深度精度

两个表面投影后的深度几乎相同时，有限精度 Depth Buffer 可能无法稳定区分它们，画面会交替出现斑点，叫 z-fighting。

常见原因：

- 两个多边形完全或几乎共面。
- near 设得极小，far 设得极大。
- 使用精度较低的深度格式。

先从几何上避免重复共面表面，并让 near 尽可能远、far 满足场景需要即可。不要随意给世界坐标加随机偏移掩盖问题。

进入下一部分前，先验证：交换两个重叠 Triangle 的绘制顺序，最终遮挡关系保持不变；把 Depth Buffer 显示成灰度时，数值变化符合远近关系。

#### 本部分检查点

**能做出来**：两个三角形互相穿插时，遮挡关系正确，并且交换绘制顺序后画面不变。

**能回答**：

- Depth Buffer 每个位置存的是什么？
- 深度测试是在 Fragment Shader 之前还是之后？
- Z-fighting 长什么样，为什么会发生？

**卡在这里就先不要往下读**：交换顺序画面就变，说明深度没写入或没比较。先把深度值当颜色输出看一眼。

## 第二部分：把 Texture 贴到三角形上

### 6.7 Texture、texel、UV、Pixel 的区别

| 名词 | 含义 |
| --- | --- |
| Texture | 用作渲染输入的一组二维或多维数据，最常见是图片 |
| texel | Texture 中的一个格子，texture element 的缩写 |
| UV | 在 Texture 上定位的相对坐标 |
| Pixel | 最终 Framebuffer 中的一个格子 |
| Fragment | 准备竞争某个 Pixel 的候选结果 |

一个 1024×1024 木纹 Texture 可以贴到只占屏幕 50×20 Pixel 的远处地板。一个 Pixel 可能综合多个 texel；一个 texel 在近处也可能覆盖许多 Pixel。

最小纹理类型：

~~~cpp
struct Texture {
    int width = 0;
    int height = 0;
    std::vector<Color> texels;
};
~~~

它与 Color Buffer 都是二维颜色数组，但角色不同：Texture 是读取的输入，Color Buffer 是本次渲染写入的输出。

### 6.8 UV 是与分辨率无关的相对坐标

UV 通常使用 `[0,1]`：

~~~text
v=1   (0,1) +------------+ (1,1)
             |            |
             |  Texture   |
v=0   (0,0) +------------+ (1,0)
            u=0          u=1
~~~

u 是横向，v 是纵向。它们不用 x、y 命名，是为了不与模型空间坐标混淆。

UV 原点并非所有图片/API 都一致。有的图片数据第一行是顶部，有的约定 v=0 在底部。与 Framebuffer 相同，应在资源导入边界统一翻转，而不是在每个 Shader 或采样函数中猜测。

三角形三个顶点各有 UV。内部 Fragment 的 UV 由插值得到，再用 UV 从 Texture 取颜色。

### 6.9 Address Mode：UV 超出 0 到 1 怎么办

UV 可能小于 0 或大于 1。采样前要选择寻址模式（address mode）：

#### Clamp

夹到边缘：

~~~text
uClamped = clamp(u,0,1)
~~~

适合不希望重复的 UI 图片等。

#### Repeat

保留小数循环：

~~~text
uRepeated = u - floor(u)
~~~

例子：`1.25 -> 0.25`，`-0.2 -> 0.8`。地砖和墙面常利用大于 1 的 UV 重复图案。

注意 Repeat 下 `u=1` 会回到 0；Clamp 下 `u=1` 是最右边缘。模式不同，边界结果就不同。

### 6.10 Nearest Sampling：选一个最近 texel

Nearest Neighbor（最近邻采样）只取一个 texel。对 Clamp 模式，可用：

~~~cpp
Color sampleNearestClamp(const Texture& texture, Vec2 uv) {
    float u = std::clamp(uv.x, 0.0f, 1.0f);
    float v = std::clamp(uv.y, 0.0f, 1.0f);

    int x = std::min(static_cast<int>(u * texture.width),
                     texture.width - 1);
    int y = std::min(static_cast<int>(v * texture.height),
                     texture.height - 1);

    return texture.texels[static_cast<std::size_t>(y) * texture.width + x];
}
~~~

输入是 Texture 和 UV，输出是一份颜色，不会修改 Texture。

`min(...,width-1)` 专门处理 u=1 导致 `int(width)` 越界。函数还应在更外层保证纹理尺寸为正。

Nearest 在放大时能看到清晰方块，适合像素风；普通图片会显得生硬。

### 6.11 Lerp：两个值之间的线性插值

Bilinear Sampling 需要先理解 `lerp`（linear interpolation，线性插值）：

~~~text
lerp(a,b,t) = (1-t)*a + t*b
~~~

- t=0，结果是 a。
- t=1，结果是 b。
- t=0.25，结果从 a 向 b 走了四分之一。

数字例子：

~~~text
lerp(10,20,0.25)=0.75*10+0.25*20=12.5
~~~

颜色也是多个数字，可以逐通道 lerp。

### 6.12 Bilinear Sampling：混合四个邻居

Bilinear Filtering（双线性过滤）在 x 方向各插值一次，再沿 y 插值：

~~~text
c00 ---- c10
 |        |
 |   P    |
 |        |
c01 ---- c11
~~~

将 UV 映射到 texel 中心坐标的一种简化学习实现：

~~~text
x = u*(width-1)
y = v*(height-1)
x0=floor(x), x1=min(x0+1,width-1), tx=x-x0
y0=floor(y), y1=min(y0+1,height-1), ty=y-y0

topOrBottom0 = lerp(c00,c10,tx)
topOrBottom1 = lerp(c01,c11,tx)
result       = lerp(topOrBottom0,topOrBottom1,ty)
~~~

数值例子：2×2 纹理四角分别为 0、10、20、30，采样正中心 `u=v=0.5`：

~~~text
横向：lerp(0,10,0.5)=5
横向：lerp(20,30,0.5)=25
纵向：lerp(5,25,0.5)=15
~~~

结果是四角平均。真实 GPU 对 texel 中心和边界有严格定义；CPU 学习版只要明确并测试自己的坐标约定即可。

### 6.13 为什么普通 UV 插值会扭曲

第 5 章按屏幕距离计算重心坐标。问题是透视投影已经把远处压缩、近处放大，屏幕上的中点通常不是三维表面上的中点。

想象一条从近处延伸到远处的地板边。远端很长一段三维距离可能只占少量屏幕 Pixel。若 UV 仍按屏幕位置简单平均，纹理会在远处被错误拉伸，移动相机时看起来像在表面游动。

这叫 affine interpolation（仿射插值）错误。早期缺少透视校正的 3D 游戏常能看到倾斜多边形上的纹理扭曲。

### 6.14 透视正确插值公式

设屏幕重心权重为 `alpha、beta、gamma`；三个顶点的属性为 `a0、a1、a2`；Projection 后保留的齐次分量为 `w0、w1、w2`。

透视正确结果是：

~~~text
numerator = alpha*(a0/w0) + beta*(a1/w1) + gamma*(a2/w2)
denominator = alpha*(1/w0) + beta*(1/w1) + gamma*(1/w2)

a = numerator / denominator
~~~

对 UV，a 是二维向量，u、v 都按同样权重计算：

~~~text
uv = (alpha*uv0/w0 + beta*uv1/w1 + gamma*uv2/w2)
     / (alpha/w0 + beta/w1 + gamma/w2)
~~~

每个符号：

- alpha/beta/gamma：第 5 章在投影后屏幕三角形中算出的权重。
- `ai`：投影前随顶点携带的属性，例如 UV、世界位置或颜色。
- `wi`：对应顶点 Clip Position 的 w，常与相机距离相关。
- 分母：对投影压缩进行归一化，让最终权重重新加和为 1。

#### 两点简化数值例子

把一条边的屏幕中点看成权重各 0.5：

~~~text
a0=0, w0=1   近端
a1=1, w1=4   远端
alpha=beta=0.5
~~~

错误线性结果：

~~~text
0.5*0 + 0.5*1 = 0.5
~~~

透视正确结果：

~~~text
numerator   = 0.5*0/1 + 0.5*1/4 = 0.125
denominator = 0.5/1   + 0.5/4   = 0.625
result      = 0.125/0.625 = 0.2
~~~

屏幕中点对应属性 0.2，更靠近近端属性 0，这是因为远端在投影中被压缩得更多。

### 6.15 高效保存 a/w 与 1/w

顶点阶段可预先准备：

~~~cpp
struct RasterVertex {
    Vec2 screenPosition;
    float ndcZ;
    float invW;     // 1 / clip.w
    Vec2 uvOverW;   // uv / clip.w
};
~~~

Fragment 中：

~~~cpp
float invW = alpha * v0.invW
           + beta  * v1.invW
           + gamma * v2.invW;

Vec2 uvOverW = alpha * v0.uvOverW
             + beta  * v1.uvOverW
             + gamma * v2.uvOverW;

Vec2 uv = uvOverW / invW;
~~~

输入是三个顶点预处理属性和屏幕权重，输出是当前 Fragment 的正确 UV。若 `invW` 接近 0，通常说明裁剪或相机前后处理有问题，不能盲目继续除法。

深度可使用三个顶点 NDC z 的屏幕线性插值，再映射到 `[0,1]`。不要把 UV 的透视校正公式和 depth 的表示阶段混在一起。

### 6.16 一个完整 Fragment 的数据流程

在第 5 章覆盖测试通过后：

~~~text
屏幕重心权重
  -> 插值 ndcZ，得到 fragmentDepth
  -> 计算 invW 和 uvOverW，恢复透视正确 UV
  -> 用 UV 采样 Texture
  -> 深度测试
  -> 通过后同时写 Depth 与 Color
~~~

伪代码：

~~~cpp
if (covered) {
    float ndcZ = alpha*v0.ndcZ + beta*v1.ndcZ + gamma*v2.ndcZ;
    float depth = ndcZ * 0.5f + 0.5f;

    float invW = alpha*v0.invW + beta*v1.invW + gamma*v2.invW;
    Vec2 uvOverW = alpha*v0.uvOverW
                 + beta*v1.uvOverW
                 + gamma*v2.uvOverW;
    Vec2 uv = uvOverW / invW;

    Color shadedColor = sampleBilinear(texture, uv);
    depthTestAndWrite(framebuffer, x, y, depth, shadedColor);
}
~~~

“Shading” 在这里极简单，只是采样纹理；以后它还会计算灯光。

进入下一部分前，先验证：正面棋盘格不扭曲，Nearest 与 Bilinear 的差别可见，倾斜表面开启透视校正后不再出现明显的 UV 滑动。

#### 本部分检查点

**能做出来**：一张棋盘格贴图贴在斜放的三角形上，格子随透视正确收缩，而不是均匀拉伸。

**能回答**：

- 为什么 UV 不能直接线性插值？
- 要插值的是 `u` 还是 `u/w`？
- Texture 的 `(0,0)` 在哪个角？

**卡在这里就先不要往下读**：格子看起来“折了一下”就是没做透视校正。这一节必须过，第 8 章之后 GPU 会替你做，但你要能认出这个现象。

## 第三部分：在投影前裁剪三角形

### 6.17 Clipping 不是把越界顶点删除

若三角形一个顶点在近平面外、两个在内，可见部分是一个四边形：

~~~text
原三角形：             裁剪后：
      外部顶点              新交点----新交点
         *                    |          |
        / \         ->        | 可见部分 |
-------/---\---近平面         *----------*
     内部   内部              原内部顶点
~~~

直接删除外部顶点只剩两个点，整个可见区域都会丢失。正确做法是求两条跨边与近平面的交点，得到四边形，再拆成两个三角形。

Clipping（裁剪）发生在透视除法前的 Clip Space。因为此时六个边界可统一写成线性条件，且还保留 w。

### 6.18 OpenGL 六个齐次裁剪条件

一个 Clip Position `(x,y,z,w)` 在视锥内需要满足：

~~~text
左：x + w >= 0
右：w - x >= 0
下：y + w >= 0
上：w - y >= 0
近：z + w >= 0
远：w - z >= 0
~~~

每个式子可看成点到某个齐次裁剪平面的带符号值 `d`：

- `d>=0`：在该平面内侧。
- `d<0`：在外侧。

例如近平面的函数为：

~~~text
dNear(v) = v.clip.z + v.clip.w
~~~

近平面只是六次相同算法中的一次。

### 6.19 求一条边与裁剪平面的交点

边从顶点 A 到 B，可参数化为：

~~~text
V(t) = A + t(B-A),  0<=t<=1
~~~

- t=0 在 A。
- t=1 在 B。
- 中间值位于线段上。

设 A、B 对某裁剪平面的带符号值为 `dA、dB`。因为沿直线该值线性变化，交点满足 d=0：

~~~text
dA + t(dB-dA) = 0
t = dA / (dA-dB)
~~~

数值例子：A 在内侧 `dA=2`，B 在外侧 `dB=-1`：

~~~text
t = 2 / (2-(-1)) = 2/3
~~~

交点从 A 向 B 走三分之二。

位置和所有顶点属性都必须用同一个 t 插值：

~~~cpp
ClipVertex intersect(const ClipVertex& a,
                     const ClipVertex& b,
                     float distanceA,
                     float distanceB) {
    float t = distanceA / (distanceA - distanceB);
    ClipVertex out;
    out.clipPosition = lerp(a.clipPosition, b.clipPosition, t);
    out.uv = lerp(a.uv, b.uv, t);
    out.color = lerp(a.color, b.color, t);
    return out;
}
~~~

这里插值原始 UV，不是在此处先做屏幕重心插值。裁剪生成的是新的顶点，之后它会像普通顶点一样经历透视除法并计算 `uv/w`。

### 6.20 逐平面裁剪多边形

Sutherland-Hodgman 算法可以让一个顶点列表依次通过六个平面。对多边形每条边“前一顶点 S -> 当前顶点 E”，有四种情况：

| S | E | 输出 |
| --- | --- | --- |
| 内 | 内 | 输出 E |
| 内 | 外 | 输出交点 |
| 外 | 内 | 输出交点，再输出 E |
| 外 | 外 | 什么也不输出 |

输入三角形经过一次裁剪可能变成 0、3 或 4 个顶点；继续经过其他平面后可能更多。最终若顶点数少于 3，全部不可见；否则用 triangle fan（三角扇）重新三角化：

~~~text
多边形 v0,v1,v2,v3,v4

输出：
(v0,v1,v2)
(v0,v2,v3)
(v0,v3,v4)
~~~

CPU 第一版可以先只实现近平面，再扩展到六个平面，但数据结构最好从一开始允许输出多个顶点。

### 6.21 为什么相机后方顶点尤其危险

相机前方点通常有正的 `clip.w=-view.z`；相机后方可能让 w 为负。直接做 `x/w` 会改变符号，后方几何可能翻到屏幕前方；靠近 w=0 时还会得到极大坐标，形成覆盖整屏的尖刺。

正确顺序始终是：

~~~text
生成 Clip Position
-> 对完整图元做齐次裁剪
-> 对裁剪后顶点做透视除法
-> Viewport 映射
-> 光栅化
~~~

“若顶点 w<=0 就删除整个三角形”也不正确，因为三角形可能横跨相机平面，仍有一部分位于可见区域。

### 6.22 纹理缩小时的闪烁与 Mipmap 预告

远处地板可能让一个 Pixel 覆盖 Texture 中几十个甚至几百个 texel。Nearest 或普通 Bilinear 仍只查看很少 texel，会在相机移动时跳来跳去，出现闪烁和摩尔纹。

Mipmap 是预先生成的一组逐级缩小纹理，例如：

~~~text
1024x1024 -> 512x512 -> 256x256 -> ... -> 1x1
~~~

采样时根据一个 Pixel 在纹理中覆盖的大致范围选择合适层级。OpenGL 章节会使用硬件 Mipmap；CPU 第一版可先完成 nearest/bilinear，并把远处闪烁记录为已知限制。

### 6.23 推荐实现顺序

1. 为 Framebuffer 分配 Depth Buffer，并做两个重叠纯色三角形。
2. 交换绘制顺序，确认结果不变。
3. 创建 2×2 四色 Texture，手测几个固定 UV。
4. 用错误的普通 UV 插值画倾斜四边形并截图。
5. 保存 `invW、uvOverW`，加入透视正确插值并对比。
6. 只实现近平面裁剪，用不同颜色显示新生成顶点。
7. 扩展六个 Clip Plane。
8. 最后画旋转带纹理立方体。

### 6.24 常见错误

| 现象 | 可能原因 | 检查 |
| --- | --- | --- |
| 后画物体总在上面 | 没有深度测试或 Depth 未写 | 打印每次比较的 old/new depth |
| 所有 Fragment 都失败 | Clear Depth 与比较方向不匹配 | 普通 Less 应清为 1 |
| 移动物体留下洞 | 每帧未清 Depth Buffer | 在帧开头同时 clear color/depth |
| 纹理上下颠倒 | UV 原点与图片行顺序不同 | 在资源导入边界统一翻转 |
| UV=1 时越界 | 映射得到 width/height | Clamp 到最后合法索引 |
| 倾斜表面纹理游动 | 使用普通屏幕线性 UV | 保存并插值 uv/w 与 1/w |
| 相机靠近出现巨大尖刺 | 透视除法前没裁剪 | 检查六个齐次条件 |
| 裁剪后纹理断裂 | 新顶点只插值 position | 用同一个 t 插值全部属性 |
| 边界偶发 NaN（Not a Number，非数值） | w 或透视校正分母接近 0 | 先检查裁剪和分母 |

### 6.25 随读自检

1. 为什么 Color Buffer 不能自己判断谁挡住谁？
2. Texture 中的小格为什么叫 texel 而不是 Pixel？
3. 屏幕中点的 UV 为什么未必是两个顶点 UV 的平均值？
4. 裁剪为什么会创造原 Mesh 中不存在的新 Vertex？
5. 求交点时为什么 UV 必须使用与位置相同的 t？

参考答案：

1. 它只保存颜色，没有当前颜色对应的远近数据。
2. 两者属于不同数据源：texel 是纹理输入，Pixel 是 Framebuffer 输出。
3. 透视投影对远近做了不同压缩，屏幕距离不再对应三维表面距离。
4. 边与视锥边界的交点代表仍可见部分的新角点。
5. 新顶点的所有属性必须对应同一个几何位置，否则纹理或颜色会断裂。

### 6.26 编程练习

#### 练习 A：深度顺序独立

绘制三个互相重叠、深度分别为 0.2、0.5、0.8 的三角形。枚举至少三种绘制顺序，最终颜色必须相同。

#### 练习 B：深度可视化

把通过测试的深度作为灰度颜色输出：`Color(depth,depth,depth,1)`。说明 near/far 在图片中分别更黑还是更白。

#### 练习 C：纹理采样单元测试

创建 2×2 四色 Texture，测试四角、中心、超界 UV，并分别验证 Clamp、Repeat、Nearest、Bilinear。

#### 练习 D：透视校正对比

同一张倾斜棋盘地面输出两张图：普通 UV 插值与透视正确插值。笔记中标出远端最明显的差异。

#### 练习 E：近平面裁剪

让一个三角形缓慢穿过近平面。显示裁剪前后的顶点数，并确保没有整屏尖刺或突然整块消失。

#### 练习 F：旋转立方体

绘制 12 个三角形组成的立方体，开启深度、贴图、背面剔除和六平面裁剪。相机与立方体互相穿越时程序仍应稳定。

#### 练习 G：故意出错实验（破坏深度与采样）

每次只改一处，先预测，再运行，然后恢复：

1. 每帧不清 Depth Buffer，让相机绕物体转一圈。
2. 把 Clear Depth 从 1 改成 0，但保持 Less 比较。
3. 深度测试通过时不更新 Depth Buffer（只写颜色）。
4. 直接线性插值 `u`、`v`，不做透视校正，看一个大角度倾斜的平面。
5. 把 UV 采样的 clamp 去掉，然后用 `u=1.0` 采样。
6. 跳过近平面裁剪，让相机慢慢穿过一个三角形。

第 4 条产生的“贴图游动”是本章最该记住的现象。它不崩溃、不报错，只是看起来“有点怪”——而 GPU 替你做了这件事之后，你只有见过它才认得出来。

### 6.27 本章验收标准

最低通过线：

- Depth Buffer 尺寸与 Color Buffer 一致，每帧正确清除。
- 交换不透明三角形绘制顺序，遮挡结果不变。
- 能解释并手算 Nearest Texture Sample，贴图 Triangle 能正确显示。

推荐通过线：

- 能手算 Bilinear 和一组透视正确插值数值。
- 倾斜地板贴图没有明显仿射扭曲。

完整通过线：

- 能从 `dA、dB` 推导交点参数 `t=dA/(dA-dB)`。
- Triangle 穿过 Near Plane 时能保留可见部分，位置、UV 和颜色连续。
- 三角形穿过 near、far、left、right、top、bottom 时都能保留可见部分。
- 裁剪生成顶点的位置、UV 和颜色连续，没有 NaN 或越界。

至此，CPU 光栅器已经把“顶点变成 Pixel”的核心路径完整走过。下一章开始用 OpenGL 将相同的数据和规则交给 GPU 并行执行。
