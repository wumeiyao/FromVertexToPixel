# 第 5 章练习：CPU Triangle 光栅化

对应教材：[第 5 章](../../chapters/05-cpu-triangles.md)  
完整源码：[chapter05.cpp](chapter05.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter05
./build/exercises/chapter05
open build/exercise_output/chapter05_triangle.ppm
~~~

程序计算 Triangle 的 Bounding Box，用三个 Edge Function 判断 Pixel Center 是否在内部，再用归一化 Edge 值作为重心权重插值顶点颜色。

## 阅读顺序

1. `edge` 返回点在有向边哪一侧。
2. `drawTriangle` 统一 Clockwise/Counter-clockwise 绕行。
3. Bounding Box 只减少候选 Pixel，不改变几何结果。
4. `wa + wb + wc = 1`，同一组权重同时插值 RGB。

## 对照实验

- 交换两个 Vertex，确认绕行处理后图像不消失。
- 把采样点从 `(x+0.5,y+0.5)` 改成整数角点，观察边缘差异。
- 三个顶点设成相同颜色，确认插值后整面一致。

完成标准：能手算 Triangle 内一个点的三个权重，并解释最终颜色。
