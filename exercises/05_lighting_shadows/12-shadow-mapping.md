# 第 12 章练习：方向光硬阴影

对应教材：[第 12 章](../../chapters/12-shadow-mapping.md)  
完整源码：[chapter12.cpp](chapter12.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter12
./build/exercises/chapter12
~~~

程序创建 `1024 x 1024` Depth Texture 和只含 Depth Attachment 的 FBO。每帧先从 Directional Light 绘制 Shadow Pass，再从 Camera 绘制 Main Pass并采样上一 Pass 的深度。

## 阅读顺序

1. `ShadowMap` 创建 Texture、FBO，设置边界并检查完整性。
2. Shadow Pass 使用 Light ViewProjection，只写 Depth。
3. Main Vertex Shader 同时产生 Camera Clip 与 Light Clip Position。
4. Fragment Shader做 Perspective Divide、`-1..1` 到 `0..1` 映射、范围检查和深度比较。
5. Slope-aware Bias 只为浮点误差留余量。

## 对照实验

- Shadow Pass 后不恢复 Viewport。
- Main Pass 使用不同的 Light Matrix。
- 将 Bias 调大十倍，观察阴影与物体分离。

当前实现是固定范围硬阴影。PCF 和 Texel Snapping 是建立在正确深度查询上的后续扩展。
