# 第 8 章练习：第一条 GPU Draw Call

对应教材：[第 8 章](../../chapters/08-shaders-first-draw.md)  
完整源码：[chapter08.cpp](chapter08.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter08
./build/exercises/chapter08
~~~

程序上传三个 Vertex，每个 Vertex 包含二维 Position 与 RGB Color。Vertex Shader 输出 Clip Position 和颜色，Fragment Shader 接收插值颜色，最终显示彩色 Triangle。

## 阅读顺序

1. `vertices` 在 CPU 上的交错字节布局。
2. `Mesh` 怎样创建 VAO/VBO 并描述 Location、Stride、Offset。
3. `Program` 怎样分别检查 Shader Compile 和 Program Link。
4. 每帧绑定 Program/VAO 后由 `glDrawArrays` 发出 Draw Call。

## 故障实验

- 把颜色 Attribute Offset 改错一个 float。
- 在 Shader 中写一个不存在的标识符，阅读完整 Compile Log。
- 把一个 Vertex 移出 NDC，观察 Triangle 被 Clip。

完成标准：能从一个 C++ Vertex 的具体字节追到 GLSL Attribute，再追到 Framebuffer Pixel。
