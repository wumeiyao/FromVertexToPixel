# 第 11 章练习：颜色、材质与方向光

对应教材：[第 11 章](../../chapters/11-color-material-lighting.md)  
完整源码：[chapter11.cpp](chapter11.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter11
./build/exercises/chapter11
~~~

窗口绘制一个带非均匀缩放的旋转 Cube。Vertex Shader 输出 World Position/Normal；Fragment Shader 在线性空间组合 Ambient、Lambert Diffuse 和 Blinn-Phong Specular。

## 重点代码

- 所有光照方向统一在 World Space。
- `directionToLight` 表示从表面指向光源的方向。
- 非均匀缩放使用 `inverseTranspose(mat3(model))` 变换 Normal。
- `N`、`L`、`V`、`H` 在 Dot 前全部 Normalize。

## 对照实验

- 用 `mat3(model)` 代替 Normal Matrix，观察扁长 Cube 的亮度错误。
- 反转 `directionToLight`，比较受光面。
- 依次把 Ambient、Diffuse、Specular 系数设为零。

`--check` 会单独打印一个可手算的 `N dot L`。完成标准是能从某个 Fragment 的 World Normal 推导它的三项光照贡献。
