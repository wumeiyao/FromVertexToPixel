# 第 14 章练习：透明、Skybox 与 Instancing

对应教材：[第 14 章](../../chapters/14-transparency-sky-instancing.md)  
完整源码：[chapter14.cpp](chapter14.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter14
./build/exercises/chapter14
~~~

同一窗口包含三条独立路径：100 个 Cube 由一次 Instanced Draw 提交；六面不同颜色的 Cubemap 作为 Skybox；两张半透明 Quad 按远到近混合。

## 重点代码

- 一个 Instance `mat4` 占用四个 Attribute Location，四列 Divisor 都是 1。
- Sky View Matrix 移除 Camera Translation，Sky Vertex 写入最远 Depth。
- Sky Pass 关闭 Depth Write，使用 `LEQUAL`，从 Cube 内部看时 Cull Front Face。
- Transparent Pass 保持 Depth Test，关闭 Depth Write，开启 Straight Alpha Blend。

## 对照实验

- 把透明两层绘制顺序交换，和 `--check` 的手算 RGB 对照。
- 删除一个 Matrix Column 的 Divisor。
- 把完整 View Matrix 交给 Skybox，观察 Camera 平移造成的错误。

完成标准：能说明 Instancing 只减少提交次数，不会减少 100 个 Cube 的 Triangle 数。
