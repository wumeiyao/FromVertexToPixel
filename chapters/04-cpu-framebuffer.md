# 第 4 章：CPU Framebuffer 与第一个像素

<!-- nav -->
> [上一章：坐标空间、相机、投影与裁剪](03-spaces-camera-projection.md) ｜ [下一章：CPU 三角形光栅化](05-cpu-triangles.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/02_cpu_rasterizer/04-cpu-framebuffer.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

这一章暂时不使用 OpenGL 和 GPU。我们在普通 CPU 内存中创建一张图片，亲手把二维像素坐标换算成数组索引，并将结果保存为 PPM 文件。

这样做的价值是把最末端问题单独看清：无论 3D 数学和光照多复杂，最终都要把颜色写进某个二维位置。

## 本章导读

### 4.1 本章学习目标

完成本章后，应能：

1. 区分显示器、窗口、Framebuffer 和图片文件。
2. 解释 Pixel、颜色通道和 8-bit 数值。
3. 推导二维坐标到一维数组的索引公式。
4. 正确实现创建、清屏、写像素和边界检查。
5. 输出一张不依赖窗口系统的 PPM 图片。
6. 用渐变、棋盘格和基础图形验证坐标约定。

本章分三部分：先理解画布怎样存储，再实现清屏和单点写入，最后用可预测图案验证坐标与颜色。

## 第一部分：Framebuffer 的数据表示

### 4.2 Pixel 是离散网格中的一个格子

Pixel（像素）是 Framebuffer 中保存最终颜色的最小二维单位。宽 4、高 3 的图片共有 `4*3=12` 个 Pixel：

~~~text
y=2  [  ][  ][  ][  ]
y=1  [  ][  ][  ][  ]
y=0  [  ][  ][  ][  ]
       x=0 x=1 x=2 x=3
~~~

这里先约定 `(0,0)` 在左下角：

- x 向右增加。
- y 向上增加。
- 合法整数 x 为 `0..width-1`。
- 合法整数 y 为 `0..height-1`。

Pixel 是一个有面积的小格，但后续做三角形覆盖判断时，常用格子中心代表它：

~~~text
整数索引 (x,y) 对应的采样中心 = (x+0.5, y+0.5)
~~~

例如 Pixel `(2,1)` 的中心是连续坐标 `(2.5,1.5)`。整数索引和连续几何坐标不是同一概念。

### 4.3 Framebuffer 到底是什么

Framebuffer（帧缓冲）是保存渲染结果的一组二维缓冲。最小版本只有颜色缓冲（color buffer）；后面会增加深度缓冲（depth buffer）。

~~~text
Framebuffer
  宽度 width
  高度 height
  Color Buffer：每个位置的 R、G、B、A
  Depth Buffer：第 6 章加入，每个位置的深度
~~~

它不是显示器，也不一定对应窗口。CPU Framebuffer 只是进程内存中的数组；GPU 也能创建完全不显示到窗口的离屏 Framebuffer。是否显示或保存，是后续步骤。

可以把关系理解为：

~~~text
渲染算法 -> 修改 Framebuffer 数组
Framebuffer -> 可保存成图片文件
Framebuffer -> 也可复制或显示到窗口
~~~

### 4.4 颜色是数字，不是颜色名称

RGB 颜色由三个通道（channel）组成：

- R：red，红色强度。
- G：green，绿色强度。
- B：blue，蓝色强度。

渲染计算中常用 `float` 的 `[0,1]` 范围：

| RGB | 结果 |
| --- | --- |
| `(0,0,0)` | 黑色 |
| `(1,1,1)` | 白色 |
| `(1,0,0)` | 红色 |
| `(0,1,0)` | 绿色 |
| `(0,0,1)` | 蓝色 |
| `(1,1,0)` | 红加绿得到黄色 |
| `(0.5,0.5,0.5)` | 中灰色 |

Alpha（A）是第四个常见通道，通常参与透明混合。它不等于“这个 Pixel 一定透明多少”，而是混合公式的一个输入。本章先保存它但不使用透明功能。

~~~cpp
struct Color {
    float r;
    float g;
    float b;
    float a;
};
~~~

一份 `Color` 对应一个 Pixel。512×512 图片需要 `262144` 份颜色。

### 4.5 float 颜色与 8-bit 颜色

常见图片文件为每个 RGB 通道保存 8 bit，也就是 256 个整数值 `0..255`。把 `[0,1]` 浮点颜色写入文件前，要先夹紧并转换：

~~~text
byte = round(clamp(floatValue, 0, 1) * 255)
~~~

符号解释：

- `clamp(v,0,1)`：小于 0 变成 0，大于 1 变成 1，中间值不变。
- `*255`：把范围 `[0,1]` 映射到 `[0,255]`。
- `round`：四舍五入到最接近的整数。

数值例子：

~~~text
0.0 -> 0
0.5 -> round(127.5) = 128
1.0 -> 255
1.2 -> clamp 后为 1.0 -> 255
-0.3 -> clamp 后为 0.0 -> 0
~~~

C++ 函数：

~~~cpp
#include <algorithm>
#include <cmath>
#include <cstdint>

std::uint8_t toByte(float value) {
    float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<std::uint8_t>(std::lround(clamped * 255.0f));
}
~~~

本章暂不讨论 Gamma 和线性颜色空间。第 11 章会说明为何“计算中的 0.5”不一定等于图片编码中的视觉中灰。

### 4.6 二维图片为什么存进一维数组

计算机内存可理解为一长排连续字节。`std::vector<Color>` 也是一维连续数组。我们约定按行排列：先保存 y=0 的整行，再保存 y=1 的整行。

宽 4、高 3 时：

~~~text
数组索引：  0     1     2     3     4     5     6     7     8     9    10    11
二维坐标：(0,0) (1,0) (2,0) (3,0) (0,1) (1,1) (2,1) (3,1) (0,2) (1,2) (2,2) (3,2)
~~~

坐标 `(x,y)` 前面有 y 个完整行，每行 width 个元素，所以前面共有 `y*width` 个元素；再向右走 x 个：

~~~text
index = y * width + x
~~~

以 `width=4`、`(x,y)=(2,1)` 为例：

~~~text
index = 1*4 + 2 = 6
~~~

反向从索引恢复坐标：

~~~text
x = index % width
y = index / width     // 整数除法
~~~

例如 `index=6,width=4`：`x=6%4=2`，`y=6/4=1`。

#### 本部分检查点

**能做出来**：写出 `(x,y)` 到一维数组下标的公式，并手算宽 4 高 3 的图里 `(2,1)` 落在第几个元素。

**能回答**：

- 为什么二维图片要存进一维数组？
- Pixel 的整数索引和它的采样中心差多少？
- 一个 RGBA8 的 Pixel 占几个字节？

**卡在这里就先不要往下读**：下标算错会让后面所有图案都是乱的。先在纸上把 4×3 的 12 个格子按存储顺序编号。

## 第二部分：创建、清除和写入画布

### 4.7 Framebuffer 数据结构与创建

~~~cpp
#include <vector>

struct Framebuffer {
    int width = 0;
    int height = 0;
    std::vector<Color> color;
};

Framebuffer createFramebuffer(int width, int height) {
    Framebuffer fb;
    fb.width = width;
    fb.height = height;
    fb.color.resize(width * height);
    return fb;
}
~~~

`resize` 创建 `width*height` 个 `Color` 元素。这个最小版本要求调用者传入正数尺寸；完成核心练习后，再为 `width <= 0` 或 `height <= 0` 增加错误处理。

创建成功只代表内存存在，不代表它已显示到屏幕，也不代表每个元素已经是你想要的背景色。

### 4.8 Clear：为新一帧设置初始颜色

Clear（清屏）不是删除 Framebuffer，而是把每个 Pixel 设置成相同初始颜色：

~~~cpp
void clear(Framebuffer& fb, Color clearColor) {
    // & 表示 pixel 引用数组中的真实元素，而不是复制品。
    for (Color& pixel : fb.color) {
        pixel = clearColor;
    }
}
~~~

输入：已有 Framebuffer 和背景颜色。输出通过引用体现在 `fb.color` 被修改；函数没有返回新 Framebuffer。

为什么每帧都要 Clear？上一帧的颜色仍保存在数组里。如果不清除又没有重画所有位置，就会留下旧图像。实时渲染也常在每帧开始清颜色与深度缓冲。

标准库也可以写成：

~~~cpp
std::fill(fb.color.begin(), fb.color.end(), clearColor);
~~~

两种写法含义相同。

### 4.9 SetPixel：一次受保护的内存写入

~~~cpp
bool setPixel(Framebuffer& fb, int x, int y, Color color) {
    // 输入：目标坐标与颜色。
    // 输出：成功写入返回 true；坐标越界返回 false。
    if (x < 0 || y < 0 || x >= fb.width || y >= fb.height) {
        return false;
    }

    int index = y * fb.width + x;
    fb.color[index] = color;
    return true;
}
~~~

执行步骤：

1. 检查 `(x,y)` 是否在合法范围。
2. 用 `y*width+x` 得到一维索引。
3. 用新颜色覆盖该数组元素。

越界写入属于未定义行为，可能覆盖其他数据、随机崩溃，也可能暂时看似正常。不能依赖“这次没崩”。

这里先不增加 `getPixel` 指针接口，因为它会同时引入空指针和对象生命周期问题。需要读取颜色时，先在已经通过边界检查的代码中直接使用 `fb.color[y * fb.width + x]`。等核心流程跑通后，再把重复的读取逻辑封装成函数。

### 4.10 PPM：最适合第一张输出图的格式

PPM 是一种极简单的图片格式。它不压缩，文件大，但无需引入图片库。二进制 P6 格式由两部分组成：

~~~text
头部文本：
P6
宽 高
255

随后紧跟每个 Pixel 的 R、G、B 三个字节
~~~

`255` 表示通道最大整数值。Alpha 不写入普通 PPM。

完整保存函数：

~~~cpp
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

void savePpm(const Framebuffer& fb, const char* path) {
    // 输入：CPU Framebuffer 与目标文件路径。
    // 输出：磁盘上的 P6 PPM 文件；Framebuffer 本身不改变。
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open output file");
    }

    file << "P6\n" << fb.width << ' ' << fb.height << "\n255\n";

    std::vector<std::uint8_t> bytes;
    bytes.reserve(static_cast<std::size_t>(fb.width) * fb.height * 3);

    // 本书 Framebuffer 是左下原点；常见图片查看器把文件第一行看作顶部。
    // 因此从最高 y 写到最低 y，只在文件边界做一次翻转。
    for (int y = fb.height - 1; y >= 0; --y) {
        for (int x = 0; x < fb.width; ++x) {
            const Color& c = fb.color[static_cast<std::size_t>(y) * fb.width + x];
            bytes.push_back(toByte(c.r));
            bytes.push_back(toByte(c.g));
            bytes.push_back(toByte(c.b));
        }
    }

    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));

    if (!file) {
        throw std::runtime_error("Failed while writing PPM file");
    }
}
~~~

这里的 y 翻转是格式约定转换，不是渲染算法的一部分。如果输出图上下颠倒，应先检查到底在哪一层定义了原点，不要在 `setPixel`、三角形算法和保存函数中重复翻转。

#### 本部分检查点

**能做出来**：程序能创建 Framebuffer、清成一个纯色、往指定位置写一个像素，并存成能打开的 PPM。

**能回答**：

- clear 和 setPixel 各改变了哪份数据？
- 越界写入会发生什么，你怎么防？
- PPM 里的 y 翻转是在哪一层做的？

**卡在这里就先不要往下读**：图片打不开或全黑时，先用 2×2 的图片调试，不要用 1920×1080。

## 第三部分：用图案验证实现

### 4.11 第一张可预测图片

先生成 8×8 图片，而不是立刻使用 1920×1080。小图片容易把每个格子与数组内容一一对应。

~~~cpp
Framebuffer fb = createFramebuffer(8, 8);
clear(fb, {0.0f, 0.0f, 0.0f, 1.0f});

setPixel(fb, 0, 0, {1.0f, 0.0f, 0.0f, 1.0f}); // 左下红色
setPixel(fb, 7, 0, {0.0f, 1.0f, 0.0f, 1.0f}); // 右下绿色
setPixel(fb, 0, 7, {0.0f, 0.0f, 1.0f, 1.0f}); // 左上蓝色
setPixel(fb, 7, 7, {1.0f, 1.0f, 1.0f, 1.0f}); // 右上白色

savePpm(fb, "corners.ppm");
~~~

这四个角是坐标约定测试。若颜色位置不符合注释，先修正原点和索引，不能继续画复杂图形。

### 4.12 渐变：让坐标变成颜色

横向渐变可以让红色通道从左边 0 变到右边 1：

~~~cpp
for (int y = 0; y < fb.height; ++y) {
    for (int x = 0; x < fb.width; ++x) {
        float u = static_cast<float>(x) /
                  static_cast<float>(fb.width - 1);
        setPixel(fb, x, y, {u, 0.0f, 0.0f, 1.0f});
    }
}
~~~

输入 x 是整数索引，输出 u 是 `[0,1]` 相对位置：

~~~text
x=0 -> u=0
x=width-1 -> u=1
~~~

若 `width=1`，分母为 0。通用代码要特殊处理；本练习可要求宽高至少为 2。

二维渐变可让：

~~~text
r = x / (width-1)
g = y / (height-1)
b = 0
~~~

这样左下为黑色，右下偏红，左上偏绿，右上为黄色。它同时验证 x、y 与颜色通道是否对应。

### 4.13 棋盘格：理解整数除法与取余

设每个方格边长为 `cellSize`：

~~~cpp
int cellX = x / cellSize;
int cellY = y / cellSize;
bool isWhite = ((cellX + cellY) % 2) == 0;
~~~

整数除法把连续若干 Pixel 分到同一个格子；`%2` 判断格子编号和是奇数还是偶数。

例如 `cellSize=8`：x 为 `0..7` 时 `cellX=0`，x 为 `8..15` 时 `cellX=1`。

棋盘格能快速暴露：

- x/y 是否交换。
- 行宽索引是否写错。
- 图片是否上下翻转。
- 循环边界是否少画一行或多画一行。

### 4.14 矩形和圆的覆盖测试

填充轴对齐矩形只需判断范围：

~~~text
minX <= x < maxX
minY <= y < maxY
~~~

使用半开区间 `[min,max)` 能让宽度刚好是 `max-min`，也方便相邻矩形无重叠拼接。

圆可用“像素中心到圆心的距离”判断：

~~~text
dx = (x+0.5) - centerX
dy = (y+0.5) - centerY

若 dx² + dy² <= radius²，则中心位于圆内
~~~

这里比较长度平方，避免每个 Pixel 都求平方根。

~~~cpp
void drawFilledCircle(Framebuffer& fb,
                      float centerX, float centerY,
                      float radius, Color color) {
    float radiusSquared = radius * radius;
    for (int y = 0; y < fb.height; ++y) {
        for (int x = 0; x < fb.width; ++x) {
            float dx = (static_cast<float>(x) + 0.5f) - centerX;
            float dy = (static_cast<float>(y) + 0.5f) - centerY;
            if (dx * dx + dy * dy <= radiusSquared) {
                setPixel(fb, x, y, color);
            }
        }
    }
}
~~~

这段代码遍历全图，暂时不追求快。以后可先求圆的包围盒，减少明显不可能在圆内的检查。

### 4.15 连续几何与离散 Pixel 为什么产生锯齿

理想圆和直线是连续图形，Pixel 网格是离散采样。一个 Pixel 中可能只有 10% 被圆覆盖，但本章只用中心点决定“全填或全不填”，边缘自然呈台阶状，这叫 aliasing（走样），视觉上常表现为锯齿。

这不是圆公式错误。更精细的方法会在一个 Pixel 内采多个点，或计算覆盖比例，再让边缘颜色部分混合，称为抗锯齿。本阶段先保留锯齿，确保最基础覆盖规则稳定。

### 4.16 调试数组问题的方法

出现图片异常时，按从小到大排查：

1. 使用 4×3 或 8×8 小尺寸。
2. 打印每个 `(x,y)->index`。
3. 给四角写四种固定颜色。
4. 检查循环条件是 `<width`，不是 `<=width`。
5. 开启 Address Sanitizer 检测越界。
6. 最后才检查图片查看器或文件格式。

Address Sanitizer 是编译器提供的内存错误检测功能。Clang 调试构建可加入：

~~~text
-fsanitize=address -fno-omit-frame-pointer
~~~

它不能证明程序绝对正确，但能很快报告许多越界和释放后使用问题。

### 4.17 常见错误

| 现象 | 原因 | 修正 |
| --- | --- | --- |
| 图片像被转置 | 索引写成 `y+x*width` 或循环 x/y 混用 | 固定使用 `y*width+x` |
| 末行崩溃 | 循环使用 `<=width` 或 `<=height` | 合法索引最大是尺寸减 1 |
| 图片上下颠倒 | Framebuffer 与文件行顺序不同 | 只在保存时翻转 y |
| 渐变不到纯白 | 分母用了 width 而非 width-1 | 明确是否要让最后一个样本等于 1 |
| 颜色溢出或绕回 | float 未 clamp 就转整数 | 转换前夹到 `[0,1]` |
| 清屏没效果 | range-for 中写成 `Color pixel`，修改了复制品 | 使用 `Color& pixel` |
| 修改 Pixel 后文件没变化 | 修改发生在保存之后或看的是旧文件 | 打印路径与保存时间，确保重新运行 |

### 4.18 随读自检

1. 512×512 的颜色缓冲包含多少个 Pixel？
2. 宽为 10 时，坐标 `(3,2)` 的一维索引是什么？
3. 为什么 Pixel `(3,2)` 的几何中心写成 `(3.5,2.5)`？
4. `clear` 是不是删除并重建 Framebuffer？
5. 为什么 CPU Framebuffer 即使没有窗口也能生成图片？

参考答案：

1. `262144`。
2. `2*10+3=23`。
3. 整数坐标标识格子，中心位于格子起点再加半个单位处。
4. 不是，只是覆盖所有颜色元素。
5. 它本质是普通内存，保存文件只需按格式读取这些数字。

### 4.19 练习

#### 练习 A：四角测试

生成 8×8 PPM，四角分别为红、绿、蓝、白，其余黑色。验收时明确指出图片中哪个角对应 `(0,0)`。

#### 练习 B：二维渐变

生成 512×512 图片：r 随 x 增加，g 随 y 增加，b 固定为 0.25。四个角的预期颜色必须先写在笔记里。

#### 练习 C：棋盘格

实现可配置 `cellSize` 的棋盘格。测试 `cellSize=1、8、64`，解释为什么 1 会让相邻每个 Pixel 都变色。

#### 练习 D：矩形与圆

实现填充矩形、矩形边框和填充圆。图形允许部分位于 Framebuffer 外，程序仍不得越界。

#### 练习 E：故意出错实验（破坏索引与边界）

临时将索引改为 `x*height+y`、将保存循环取消 y 翻转、将循环终点改成 `<=width`。记录三种错误的现象，并在恢复后用 Address Sanitizer 再运行。

### 4.20 本章验收标准

- 能从“前面有 y 行”推导 `index=y*width+x`，而不是只背公式。
- 能解释整数 Pixel 索引与连续 Pixel 中心坐标的区别。
- `setPixel` 对负坐标和超出右上边界都不会写内存。
- 能生成并查看 PPM 的四角、渐变、棋盘格、矩形与圆。
- 图片原点约定写在代码或笔记中，且只在一个明确边界做 y 翻转。
- Address Sanitizer 运行练习时没有越界报告。

下一章会把第 3 章得到的屏幕三角形放进这个 Framebuffer：先求它附近有哪些 Pixel，再判断每个 Pixel 中心是否位于三角形内部。
