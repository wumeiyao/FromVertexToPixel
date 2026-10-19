# 第 9 章练习：模型文件到 GPU Mesh

对应教材：[第 9 章](../../chapters/09-mesh-model-assets.md)  
完整源码：[chapter09.cpp](chapter09.cpp)  
测试资产：[chapter09_pyramid.obj](chapter09_pyramid.obj)

## 构建运行

~~~zsh
cmake --build build --target chapter09
./build/exercises/chapter09
~~~

程序读取 OBJ 的 Position、Normal 和 Face，按 Face Normal 展开 Vertex，将多边形扇形三角化，再上传 Indexed Mesh。窗口使用 Normal 伪颜色显示旋转金字塔；Terminal 打印 Vertex 和 Triangle 数。

当前环境只有 GLFW/GLM，因此可运行样例使用小型 OBJ 解析器，专注验证“文件数据 -> CPU Mesh -> GPU Resource -> Draw Call”边界。教材中的 glTF Scene/Node/Accessor/BufferView 路径仍应使用 `fastgltf` 或 `cgltf` 实现，不应把这个 OBJ Parser 扩展成自制通用资产库。

## 对照实验

- 改坏一个 Face Index，观察 `.at()` 怎样在 CPU 上传前暴露错误。
- 反转一个 Face 的绕行，开启 Back-face Culling 后观察结果。
- 修改 Normal，使用伪颜色定位资产数据问题。

完成标准：能区分磁盘格式、CPU 数组、VAO/VBO/EBO 和 Draw Call 四个阶段。
