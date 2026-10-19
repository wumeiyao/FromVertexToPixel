# 第 3 章练习：Local、World、View、Clip 与 NDC

对应教材：[第 3 章](../../chapters/03-spaces-camera-projection.md)  
完整源码：[chapter03.cpp](chapter03.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter03
./build/exercises/chapter03
~~~

示例对 Local 点依次应用旋转和平移组成的 Model Matrix、`lookAt` View Matrix 和 Perspective Projection，并打印每一步。

## 重点代码

- Model 使用 `translation * rotation`，右侧 Rotation 先作用于点。
- `lookAt` 用 Camera Position、Target 和 Up 构造 View。
- Perspective 需要 FOV、Aspect、Near、Far。
- NDC 必须由 Clip xyz 除以 Clip w 得到。

## 对照实验

- 交换平移与旋转的矩阵乘法顺序。
- 把 Aspect 从 `4/3` 改成 `16/9`，只比较 NDC x。
- 让 Near 接近 Far，记录深度数值怎样失去可用范围。

完成标准：能判断某个错误最早出现在 Model、View、Projection 还是 Perspective Divide。
