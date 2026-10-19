# 第 4 章练习：CPU Framebuffer

对应教材：[第 4 章](../../chapters/04-cpu-framebuffer.md)  
完整源码：[chapter04.cpp](chapter04.cpp)  
共享图片类：[image.hpp](../common/image.hpp)

## 构建运行

~~~zsh
cmake --build build --target chapter04
./build/exercises/chapter04
open build/exercise_output/chapter04_framebuffer.ppm
~~~

程序创建 `512 x 512` 的 CPU RGB 数组，用 `y * width + x` 定位 Pixel，写入渐变和棋盘格，再编码为 PPM 文件。

## 阅读顺序

1. `Image` 构造函数怎样分配 `width * height` 个 Pixel。
2. `Image::set` 怎样拒绝越界坐标。
3. 两层循环怎样把归一化坐标 `u/v` 转成颜色。
4. `writePpm` 怎样把浮点颜色 Clamp 后写成 8-bit RGB。

## 对照实验

- 把索引想成 `x * height + y`，预测图像怎样错乱后再验证。
- 把棋盘格单元从 32 改为 8。
- 将输出尺寸改成非正方形，检查代码是否仍使用正确宽度。

完成标准：能解释“图片”本质上为什么可以只是一段按规则排列的 Pixel 数据。
