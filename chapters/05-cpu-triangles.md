# 第 5 章：CPU 三角形光栅化

<!-- nav -->
> [上一章：CPU Framebuffer 与第一个像素](04-cpu-framebuffer.md) ｜ [下一章：深度、纹理、透视校正与裁剪](06-cpu-depth-texture-clipping.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/02_cpu_rasterizer/05-cpu-triangles.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

上一章已经能把颜色写入任意 Pixel。本章输入三个屏幕顶点，输出它们围成的三角形所覆盖的 Pixel，并让内部颜色从三个顶点平滑过渡。

这个“从连续三角形到离散采样格”的过程叫光栅化（rasterization）。GPU 以后会并行完成同一类工作，所以 CPU 版本不是一次性玩具，而是理解硬件管线的可观察模型。

## 本章导读

### 5.1 本章学习目标

完成本章后，应能：

1. 区分 Vertex、Triangle、Pixel 和 Fragment。
2. 用二维叉积解释边函数为何能判断点在边的哪一侧。
3. 推导重心坐标，并用它插值颜色、深度等属性。
4. 用包围盒限制候选 Pixel 范围。
5. 处理顺逆时针、退化三角形与屏幕边界。
6. 解释共享边为何需要一致的填充规则。

本章分三部分：先判断哪些 Pixel 被覆盖，再计算内部属性，最后处理共享边、背面和退化输入。

## 第一部分：找出三角形覆盖的 Pixel

### 5.2 输入是什么，输出是什么

本章暂时假设三个顶点已经完成第 3 章的投影和 Viewport 映射，位置使用连续 Screen Space 坐标：

~~~cpp
struct ScreenVertex {
    Vec2 position; // 连续屏幕坐标，不是整数 Pixel 索引
    Color color;   // 顶点携带的颜色属性
};
~~~

输入：三个 `ScreenVertex` A、B、C。

输出：若干 Fragment。Fragment 是三角形在某个 Pixel 采样位置产生的候选结果，至少包含：

~~~cpp
struct Fragment {
    int pixelX;
    int pixelY;
    Color color;
};
~~~

为什么不直接叫 Pixel？因为这个候选结果以后可能被深度测试丢弃，也可能与已有颜色混合。只有通过后续测试并写入 Framebuffer，它才改变最终 Pixel。

### 5.3 连续三角形与离散网格

三个顶点连线得到连续几何边界：

~~~text
             C
             *
            / \
           /   \
          /     \
         *-------*
         A       B
~~~

Framebuffer 却是离散格子。最简单采样规则是：对候选 Pixel 使用中心 `(x+0.5,y+0.5)`，中心在三角形内就生成 Fragment，否则不生成。

~~~text
. = Pixel 中心不在三角形内
X = Pixel 中心在三角形内

......
...X..
..XXX.
.XXXXX
~~~

这个规则会产生锯齿，但它明确、可复现，适合作为第一版。

### 5.4 为什么实时渲染选择三角形

三角形有几个关键性质：

1. 三个不共线点唯一确定一个平面。
2. 三角形内部任意点可用三个顶点的权重唯一表示。
3. 凸三角形的内部可由三条边的同侧测试判断。
4. 任意复杂多边形最终都能拆成三角形。
5. GPU 的固定功能光栅化硬件以三角形为主要图元。

“三个点确定平面”必须加上“不共线”。若三点在同一直线上，三角形面积为 0，叫退化三角形，不能正常填充。

### 5.5 顶点绕行顺序与有向三角形

按 A->B->C 连接顶点，在屏幕上可能是：

- Counter-clockwise（CCW）：逆时针。
- Clockwise（CW）：顺时针。

顺序不只影响写法，它定义三角形正面朝向。交换任意两个顶点就会反转绕行方向。

~~~text
逆时针 A->B->C            顺时针 A->C->B

       C                         C
       *                         *
      / \                       / \
     /   \                     /   \
    *-----*                   *-----*
    A     B                   A     B
~~~

本章先约定：Screen Space 为 y 向上，逆时针三角形是正面。后面会说明怎样兼容顺时针输入。

### 5.6 二维叉积与“点在边的哪一侧”

二维向量没有单独的三维方向可输出，但可把它们临时看作 z=0 的三维向量。叉积结果只有 z 分量：

~~~text
cross2D(a,b) = ax*by - ay*bx
~~~

结果是一个标量：

- 大于 0：从 a 转向 b 是逆时针。
- 小于 0：从 a 转向 b 是顺时针。
- 等于 0：a、b 平行或其中一个为零。

对有向边 A->B 和测试点 P，构造：

~~~text
AB = B - A
AP = P - A
edge(A,B,P) = cross2D(AB, AP)
~~~

展开为：

~~~text
edge(A,B,P)
= (Bx-Ax)*(Py-Ay) - (By-Ay)*(Px-Ax)
~~~

符号告诉 P 位于有向边哪一侧：

~~~text
edge > 0：P 在 A->B 的左侧
edge < 0：P 在 A->B 的右侧
edge = 0：P 在边所在直线上
~~~

#### 数值例子

~~~text
A=(1,1), B=(5,1)
AB=(4,0)

P=(2,2), AP=(1,1)
edge = 4*1 - 0*1 = 4 > 0，P 在边上方/左侧

Q=(2,0), AQ=(1,-1)
edge = 4*(-1) - 0*1 = -4，Q 在另一侧
~~~

C++ 实现：

~~~cpp
float edgeFunction(Vec2 a, Vec2 b, Vec2 p) {
    // 输入：有向边 a->b 与测试点 p。
    // 输出：有符号的两倍面积；符号表示 p 位于边的哪一侧。
    return (b.x - a.x) * (p.y - a.y)
         - (b.y - a.y) * (p.x - a.x);
}
~~~

### 5.7 边函数的绝对值还是两倍面积

`edge(A,B,C)` 的绝对值等于三角形 ABC 面积的两倍：

~~~text
triangleArea = abs(edge(A,B,C)) / 2
~~~

为什么是两倍？底为 `|AB|`、高为 C 到直线 AB 的垂直距离时，平行四边形面积是底乘高；叉积长度正是这个平行四边形面积。三角形占一半。

例子：

~~~text
A=(1,1), B=(5,1), C=(1,5)
底 AB=4，高=4，三角形面积=4*4/2=8

edge(A,B,C)=4*4-0*0=16
16/2=8
~~~

符号还编码绕行：`edge(A,B,C)>0` 表示 A->B->C 逆时针；小于 0 表示顺时针；接近 0 表示退化。

### 5.8 三个同侧测试判断三角形内部

逆时针三角形的内部始终位于每条有向边的左侧：

~~~text
E0 = edge(A,B,P)
E1 = edge(B,C,P)
E2 = edge(C,A,P)

若 E0>=0 且 E1>=0 且 E2>=0，则 P 在内部或边界上
~~~

原因是三角形是三个半平面的交集：

- A->B 左侧保留一大片区域。
- B->C 左侧再切掉一部分。
- C->A 左侧再切掉一部分。
- 三者共同剩下的正是三角形。

如果三角形是顺时针，内部会让三个值都小于等于 0。可用两种策略：

1. 开启背面剔除，直接丢弃顺时针三角形。
2. 若要双面绘制，发现面积为负时交换 B、C，使它转成逆时针。

第一版建议先统一为逆时针，减少分支和符号混乱。

### 5.9 包围盒：先排除不可能的 Pixel

最直接做法是测试 Framebuffer 中所有 Pixel。1920×1080 约有两百万个位置，一个只占几十 Pixel 的小三角形没必要检查全屏。

Axis-Aligned Bounding Box（AABB，轴对齐包围盒）是能包住三角形、边缘与 x/y 轴平行的最小矩形：

~~~text
minX = min(A.x, B.x, C.x)
maxX = max(A.x, B.x, C.x)
minY = min(A.y, B.y, C.y)
maxY = max(A.y, B.y, C.y)
~~~

顶点是连续坐标，Pixel 是整数索引。安全候选范围可写成：

~~~text
startX = floor(minX)
endX   = ceil(maxX) - 1
startY = floor(minY)
endY   = ceil(maxY) - 1
~~~

然后夹到 Framebuffer：

~~~text
startX = max(startX, 0)
endX   = min(endX, width-1)
startY = max(startY, 0)
endY   = min(endY, height-1)
~~~

包围盒只筛选候选，不代表矩形中的 Pixel 都在三角形内。每个中心仍要做三个边函数测试。

三角形完全在屏幕外时，夹紧后可能出现 `startX>endX` 或 `startY>endY`，此时直接返回。

### 5.10 第一版覆盖算法

~~~cpp
void rasterizeSolidTriangle(Framebuffer& fb,
                            Vec2 a, Vec2 b, Vec2 c,
                            Color color) {
    float signedAreaTwice = edgeFunction(a, b, c);

    // 面积接近 0：三个点共线或几乎共线，无法稳定填充。
    if (std::abs(signedAreaTwice) < 0.000001f) {
        return;
    }

    // 本函数统一处理逆时针三角形。
    if (signedAreaTwice < 0.0f) {
        std::swap(b, c);
        signedAreaTwice = -signedAreaTwice;
    }

    int minX = std::max(0, static_cast<int>(std::floor(
        std::min({a.x, b.x, c.x}))));
    int maxX = std::min(fb.width - 1, static_cast<int>(std::ceil(
        std::max({a.x, b.x, c.x}))) - 1);
    int minY = std::max(0, static_cast<int>(std::floor(
        std::min({a.y, b.y, c.y}))));
    int maxY = std::min(fb.height - 1, static_cast<int>(std::ceil(
        std::max({a.y, b.y, c.y}))) - 1);

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            Vec2 p = {static_cast<float>(x) + 0.5f,
                      static_cast<float>(y) + 0.5f};

            float e0 = edgeFunction(a, b, p);
            float e1 = edgeFunction(b, c, p);
            float e2 = edgeFunction(c, a, p);

            if (e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f) {
                setPixel(fb, x, y, color);
            }
        }
    }
}
~~~

数据变化顺序：

1. 三个连续顶点 -> 有向面积与绕行方向。
2. 连续顶点 -> 整数候选包围盒。
3. 每个整数 Pixel -> 连续中心点。
4. 中心点 -> 三个边函数值。
5. 通过覆盖测试 -> 写入颜色。

先让这个纯色版本正确，再加入属性插值。

#### 本部分检查点

**能做出来**：画出一个三角形的包围盒，手算某个 Pixel 中心的三个边函数值，判断它在不在里面。

**能回答**：

- 为什么用边函数而不是逐条边求交？
- 有向面积的正负说明什么？
- 为什么要先算包围盒？

**卡在这里就先不要往下读**：三角形形状不对、或者只画出一半时，先打印三个边函数的符号，不要改代码。

## 第二部分：给内部 Pixel 插值属性

### 5.11 重心坐标是什么

三角形内部任一点 P 都能写成三个顶点的加权和：

~~~text
P = alpha*A + beta*B + gamma*C
alpha + beta + gamma = 1
~~~

`alpha、beta、gamma` 叫重心坐标（barycentric coordinates）。它们是相对于三个顶点的权重：

- 在 A 点：`(1,0,0)`。
- 在 B 点：`(0,1,0)`。
- 在 C 点：`(0,0,1)`。
- 三角形中心：约 `(1/3,1/3,1/3)`。
- 越靠近 A，alpha 通常越大。

内部点三个权重都不小于 0；外部点至少有一个权重为负。

### 5.12 用子三角形面积推导权重

总三角形 ABC 被点 P 分成三个子三角形：

~~~text
alpha = area(PBC) / area(ABC)
beta  = area(PCA) / area(ABC)
gamma = area(PAB) / area(ABC)
~~~

三个子面积相加等于总面积，所以三个权重之和为 1。

边函数正好给出两倍面积，分子分母中的 `/2` 会抵消。对逆时针三角形：

~~~text
total = edge(A,B,C)

alpha = edge(B,C,P) / total
beta  = edge(C,A,P) / total
gamma = edge(A,B,P) / total
~~~

注意每个权重对应“对面的边”：alpha 使用 BC，beta 使用 CA，gamma 使用 AB。

#### 完整数值例子

~~~text
A=(1,1), B=(5,1), C=(1,5), P=(2,2)
total=edge(A,B,C)=16

alpha=edge(B,C,P)/16=8/16=0.5
beta =edge(C,A,P)/16=4/16=0.25
gamma=edge(A,B,P)/16=4/16=0.25

验证位置：
0.5*(1,1)+0.25*(5,1)+0.25*(1,5)
=(0.5,0.5)+(1.25,0.25)+(0.25,1.25)
=(2,2)
~~~

权重求和为 1，并还原了 P，说明计算正确。

### 5.13 插值：让顶点属性延伸到内部

Interpolation（插值）是根据已知端点值求中间值。若顶点颜色为 `cA、cB、cC`，P 点颜色为：

~~~text
color(P) = alpha*cA + beta*cB + gamma*cC
~~~

每个颜色通道分别计算。例如：

~~~text
cA=红=(1,0,0)
cB=绿=(0,1,0)
cC=蓝=(0,0,1)
P 的权重=(0.5,0.25,0.25)

color(P)
=0.5*(1,0,0)+0.25*(0,1,0)+0.25*(0,0,1)
=(0.5,0.25,0.25)
~~~

同一组权重还能插值：

- 屏幕深度。
- UV 贴图坐标。
- 法线。
- 世界位置。
- 任何从顶点传给片段的连续属性。

不过经过透视投影后，UV 等属性不能直接做简单屏幕线性插值；第 6 章会加入 `1/w` 修正。本章的顶点颜色插值主要用于理解重心权重。

~~~cpp
Color interpolateColor(Color a, Color b, Color c,
                       float alpha, float beta, float gamma) {
    return {
        alpha * a.r + beta * b.r + gamma * c.r,
        alpha * a.g + beta * b.g + gamma * c.g,
        alpha * a.b + beta * b.b + gamma * c.b,
        alpha * a.a + beta * b.a + gamma * c.a
    };
}
~~~

在覆盖循环内：

~~~cpp
float alpha = edgeFunction(b.position, c.position, p) / total;
float beta  = edgeFunction(c.position, a.position, p) / total;
float gamma = edgeFunction(a.position, b.position, p) / total;

Color fragmentColor = interpolateColor(a.color, b.color, c.color,
                                       alpha, beta, gamma);
setPixel(fb, x, y, fragmentColor);
~~~

#### 本部分检查点

**能做出来**：三角形内部出现三个顶点颜色的平滑渐变，而不是纯色。

**能回答**：

- 重心坐标三个权重加起来等于多少？
- 权重是怎么从边函数值得到的？
- 插值和“取最近顶点的颜色”有什么区别？

**卡在这里就先不要往下读**：颜色出现硬边界或者超出范围，说明权重没归一化。回到 5.13 手算一个中心点。

## 第三部分：边界规则、剔除与验证

### 5.14 为什么边界规则必须统一

这一节和下一节属于第二遍完善。第一次实现时，先让一个三角形正确显示；只有开始拼接多个三角形后，共享边归属才成为必须解决的问题。

用两个三角形拼成矩形时，它们共享一条对角线。若边上的 Pixel 同时属于两边，会被处理两次；若两边都排除，会留下裂缝。

~~~text
A-------B
|     / |
|   /   |   共享边 AC
| /     |
D-------C
~~~

纯色且无混合时，重复写入可能看不出来；开启透明混合、模板计数或不同计算后就会出错。因此光栅器需要半开规则：共享边只归其中一个三角形。

### 5.15 Top-left 规则

常见做法叫 Top-left Rule（顶边/左边规则）：边界样本只在该边被规定为“包含边”时通过。具体谓词会随 y 轴方向和边函数约定变化。

对本章的 y 向上、逆时针、内部 `edge>0` 约定，可把有向边 A->B 定义为包含边：

~~~text
dy = B.y - A.y
dx = B.x - A.x

isTopLeft = (dy < 0) || (dy == 0 && dx < 0)
~~~

也就是向下的边，或水平向左的边。对一个逆时针外轮廓，它们对应几何上的左边界和顶边界。共享边在相邻三角形中的方向相反，所以恰好只有一个方向满足条件。

边测试变为：

~~~text
edge > 0：通过
edge < 0：失败
edge == 0：只有 isTopLeft(edge) 时通过
~~~

~~~cpp
bool isTopLeft(Vec2 a, Vec2 b) {
    float dy = b.y - a.y;
    float dx = b.x - a.x;
    return (dy < 0.0f) || (dy == 0.0f && dx < 0.0f);
}

bool acceptsEdge(float value, bool inclusive) {
    return value > 0.0f || (value == 0.0f && inclusive);
}
~~~

浮点值是否精确等于零可能受误差影响。学习版本若顶点和采样中心落在整数或半整数网格，较容易观察；生产光栅器常使用固定点边函数或更严格的数值规则，保证相邻三角形计算完全一致。

重要的不是死背这一条谓词，而是理解：

1. 每条无向共享边在两个三角形里方向相反。
2. 用方向决定边界归属。
3. 两个三角形必须使用完全相同的规则。

### 5.16 背面剔除

Back-face Culling（背面剔除）是在光栅化前，根据有向面积跳过背向相机的三角形。

~~~text
signedArea = edge(A,B,C)

若约定 CCW 为正面：
signedArea > 0 -> 绘制
signedArea < 0 -> 背面，跳过
~~~

封闭模型从外部看时，多数背面最终不可见，提前跳过能减少光栅化和片段工作。

但纸张、树叶、布片等可能需要两面可见。此时可禁用剔除或为材质提供双面处理。背面剔除也不能替代深度测试：两个都朝向相机的物体仍可能互相遮挡。

投影和 y 翻转可能改变绕行方向。如果明明正确的模型全部被剔除，应检查 Screen Space 变换是否反转了 y，而不是随意交换所有索引。

### 5.17 退化三角形

当三个点共线、重合或极其接近共线时，有向面积接近 0：

~~~text
abs(edge(A,B,C)) < epsilon
~~~

此时：

- 没有可填充的二维面积。
- 重心坐标会除以接近 0 的 total。
- 数值可能放大成巨大权重或 NaN。NaN 是 Not a Number（非数值）的缩写，表示浮点运算产生了无法作为普通数字使用的结果，例如 `0/0`。

所以必须在除法前跳过。退化三角形可能来自坏模型，也可能是投影后一个侧面恰好变成一条线，不一定意味着程序崩坏。

### 5.18 性能直觉：算法做了多少工作

假设包围盒宽 100、高 50，就检查 5000 个 Pixel。每个候选大致计算三个边函数。

进一步优化可以利用边函数是线性函数：x 每增加 1，边值增加一个固定量；y 每增加 1，也增加固定量。因此可在行间增量更新，而不是每个 Pixel 重新做完整乘法。

第一版不要立即优化。先保证：

- 覆盖正确。
- 权重正确。
- 边界一致。
- 越界安全。

有截图和测试后再优化，才能判断优化是否改变结果。

### 5.19 推荐实现顺序

每一步保存一张图片：

1. 只画三个顶点为大一点的方块，确认屏幕位置。
2. 画三条边，确认绕行和坐标方向。
3. 只计算包围盒，把它画成灰色矩形。
4. 在包围盒内做边测试，被覆盖位置写白色。
5. 加入重心颜色插值。
6. 用两个三角形拼矩形。
7. 加入一致边界规则。
8. 最后加入背面剔除开关。

同时调试所有功能会让黑屏原因难以判断。

### 5.20 常见错误

| 现象 | 可能原因 | 检查 |
| --- | --- | --- |
| 三角形完全不画 | 输入是顺时针但测试只接受正边值 | 打印 `edge(A,B,C)` |
| 只画包围盒一角 | 三条边方向不统一 | 确保使用 AB、BC、CA |
| 颜色权重对应错顶点 | alpha 使用了错误的对边 | alpha 对 BC，beta 对 CA，gamma 对 AB |
| 颜色超出 0..1 | 权重为负或和不为 1 | 打印 alpha、beta、gamma |
| 屏幕外三角形崩溃 | 包围盒未夹到 Framebuffer | 检查 min/max 和空范围 |
| 两个三角形有缝或重叠 | 共享边归属不一致 | 实现同一 Top-left 规则 |
| 背面剔除方向反了 | y 轴翻转或 CCW/CW 约定不同 | 用一个已知三角形验证有向面积 |
| 极细三角形出现 NaN | 面积接近 0 仍计算权重 | 除法前做退化检查 |

### 5.21 手算自检

使用：

~~~text
A=(1,1), B=(5,1), C=(1,5)
~~~

回答：

1. `edge(A,B,C)` 是多少，绕行是什么？
2. P=(2,2) 的三个权重是多少？
3. Q=(4,4) 是否在三角形内？
4. 若交换 B、C，有向面积怎样变化？

参考答案：

1. 16，逆时针。
2. `(0.5,0.25,0.25)`。
3. 不在，因为它越过斜边 BC，至少一个边值/权重为负。
4. 变成 -16，面积绝对值不变但绕行反转。

### 5.22 编程练习

#### 练习 A：边函数可视化

固定一条有向边，把 `edge>0` 的 Pixel 画绿、`edge<0` 画红、接近 0 画白。交换边端点，颜色区域应完全反转。

#### 练习 B：纯色三角形

实现包围盒与三个边测试。测试三角形部分越过屏幕四条边，程序不得越界。

#### 练习 C：重心坐标图

把输出颜色直接设为 `(alpha,beta,gamma)`，三个顶点应分别接近红、绿、蓝，内部平滑过渡。

#### 练习 D：共享边

用两个三角形拼成矩形。为每个 Pixel 额外保存被覆盖次数，验证内部每个采样最多一次、没有中间零覆盖裂缝。

#### 练习 E：绕行与剔除

加入 `CullMode { None, Back, Front }`，交换顶点顺序并预测三种模式的结果。

#### 练习 F：退化输入

测试三个重合点、三个共线点、面积非常小的点。程序必须稳定返回，不产生除零、NaN 或越界。

#### 练习 G：故意出错实验（破坏光栅化规则）

每次只改一处，先预测，再运行，然后恢复：

1. 把覆盖测试从“三个边函数同号”改成“只要有一个为正”。
2. 把三条边写成 AB、BC、**AC**（最后一条方向反了）。
3. 把包围盒的 `clamp` 去掉，然后画一个一半在屏幕外的三角形。
4. 把重心坐标的 alpha 和 beta 对调。
5. 把顶点顺序改成顺时针，但保持原来的符号判断。

对照本章 5.20 常见错误：你造出来的现象，应该能在那张表里找到对应行。如果找不到，说明你发现了一种表里没写的失败方式，把它记进 [plan/progress.md](../plan/progress.md)。

### 5.23 本章验收标准

第一次完成 Triangle Rasterizer 时：

- 能从二维叉积解释边函数符号，而不是只会调用代码。
- 能从子三角形面积推导三个重心权重。
- 能手算一个 Pixel 中心的覆盖与插值结果。
- 纯色、插值色和屏幕外三角形都能正确输出。
- 退化三角形被明确跳过。
- 能解释背面剔除、包围盒和覆盖测试分别节省或决定什么。

第二遍再完成共享边验收：两个三角形组成的矩形没有裂缝，边界样本按同一 Top-left 规则只归属一边。

下一章会为 Fragment 加入深度与 UV。那时重心坐标仍是核心，但透视投影会要求对某些属性做额外的 `1/w` 校正。
