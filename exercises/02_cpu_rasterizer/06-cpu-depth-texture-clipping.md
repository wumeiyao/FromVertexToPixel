# 第 6 章练习：Depth、Texture 与透视校正

对应教材：[第 6 章](../../chapters/06-cpu-depth-texture-clipping.md)  
完整源码：[chapter06.cpp](chapter06.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter06
./build/exercises/chapter06
open build/exercise_output/chapter06_depth_texture.ppm
~~~

示例绘制两个重叠贴图 Triangle。每个 Pixel 先比较 Depth，再插值 `uv/w` 与 `1/w`，最后相除得到透视正确 UV。

## 重点代码

- Depth Buffer 初始化为正无穷，示例约定越小越近。
- Depth Test 失败时不修改 Color。
- Checker Texture 由 UV 直接生成，所以无需外部图片。
- `uvOverW / inverseW` 是透视校正的关键步骤。

## 对照实验

- 交换两个 Triangle 的提交顺序，最终遮挡应相同。
- 临时直接线性插值原始 UV，比较倾斜区域的形变。
- 注释每帧 Depth Clear 的等价步骤，理解旧深度为何会污染下一帧。

Near Plane 裁剪的完整推导保留在教材中，本可运行样例集中验证 Depth、Texture 与透视校正三条最容易混淆的数据链。
