# 第 10 章练习：Camera 与视锥裁剪

对应教材：[第 10 章](../../chapters/10-scene-camera-culling.md)  
完整源码：[chapter10.cpp](chapter10.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter10
./build/exercises/chapter10
~~~

Camera 绕场景旋转。程序从 ViewProjection Matrix 提取并归一化六个 Frustum Plane，用每个 Cube 的 World-space Bounding Sphere 做保守裁剪。窗口标题实时显示 Total 和 Submitted 数量。

## 阅读顺序

1. `extractFrustum` 从组合矩阵获得六个 Plane。
2. 每个 Plane Equation 除以 Normal Length，保证 Distance 和 Radius 单位一致。
3. `visible` 只有在 Sphere 完全处于某个 Plane 外部时才拒绝。
4. 通过测试的对象才创建 Model Matrix 并提交 Draw。

## 对照实验

- 删除 Plane 归一化，比较裁剪边界。
- 把 `< -radius` 改成 `< 0`，观察相交对象过早消失。
- 用 `--check` 验证 Origin 可见而远点不可见。

完成标准：开启裁剪只改变 Submitted 数，不改变本应可见的画面。
