# 第 2 章练习：Dot、Cross 与 Normalize

对应教材：[第 2 章](../../chapters/02-vectors-matrices.md)  
完整源码：[chapter02.cpp](chapter02.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter02
./build/exercises/chapter02
~~~

本章没有用 GLM 代替核心计算。源码完整实现了 `dot`、`cross`、`length`、`normalize`，以及按 `m[row][column]` 保存的 `Mat4`。程序会打印单位矩阵、缩放矩阵、绕 Z 轴旋转矩阵、平移矩阵和组合后的 Model Matrix，并分别对 `w=1` 的点与 `w=0` 的方向做变换。

## 阅读顺序

1. 先看 `Vec3`，理解三个分量只是三个带含义的数。
2. 看 `dot` 为什么返回一个标量，以及零为什么表示垂直。
3. 看 `cross` 的分量公式，运行验证 `X cross Y = Z`。
4. 看 `normalize` 为什么要先防止零向量除法。
5. 再看 `Mat4` 的 `m[row][column]`，对照 `multiply(matrix, vector)` 的四条输出公式。
6. 观察同一张 Model Matrix 作用于点和方向时，第四列为什么只影响点。

## 对照实验

- 交换 `cross(right, up)` 的两个输入，观察方向反转。
- 把 `(3,4,0)` 改成 `(0,0,0)`，阅读明确的失败信息。
- 使用两个同向单位向量做 Dot，再改成反向，比较 `1` 与 `-1`。
- 把 `model` 的组合顺序改成 `scale * rotation * translation`，先预测点的位置，再运行比较。
- 把 `direction.w` 从 `0` 改成 `1`，观察平移如何错误地进入方向。

完成标准：不依赖图形 API，能用数字解释 Dot 衡量朝向关系、Cross 产生垂直方向。
