# 第 1 章练习：跟踪一个 Vertex 到屏幕

对应教材：[第 1 章](../../chapters/01-rendering-mental-model.md)  
完整源码：[chapter01.cpp](chapter01.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter01
./build/exercises/chapter01
~~~

程序会打印同一个点的 Local、World、View、Clip、NDC 和 `1280 x 720` Screen 坐标。每行都是前一行经过一个明确变换后的结果。

## 从哪里开始读

1. `localPosition` 是模型文件或 Mesh 中的原始顶点。
2. `model * localPosition` 把它放进世界。
3. `view * worldPosition` 把世界改写成 Camera 眼中的坐标。
4. `projection * viewPosition` 得到 Clip Position。
5. 除以 `clip.w` 得到 NDC，最后映射到 Pixel。

## 对照实验

- 把 Model 平移从 x=2 改成 x=-2，先预测 Screen x 向哪边变化。
- 把 Camera 从 z=5 改成 z=10，观察 NDC x/y 为什么更靠近中心。
- 暂时跳过除以 `w`，记录结果为何不能直接当 NDC。

完成标准：能拿输出中的一个具体数字，说明它由上一空间的哪些数据计算而来。
