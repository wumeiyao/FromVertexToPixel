# 第 7 章练习：OpenGL Context 与 Clear

对应教材：[第 7 章](../../chapters/07-opengl-context-resources.md)  
完整源码：[chapter07.cpp](chapter07.cpp)  
共享 OpenGL 辅助层：[gl_support.hpp](../common/gl_support.hpp)

## 构建运行

~~~zsh
cmake --build build --target chapter07
./build/exercises/chapter07
~~~

窗口会显示缓慢变化的 Clear Color。Terminal 同时打印真实 OpenGL、GLSL 和 GPU Renderer。按 `Esc` 退出。

不打开窗口时可运行：

~~~zsh
./build/exercises/chapter07 --check
~~~

## 阅读顺序

1. `Window` 先初始化 GLFW，再请求 macOS OpenGL 4.1 Core Context。
2. Context 当前化之后才调用 `glViewport` 和其他 OpenGL 函数。
3. 每帧执行 Clear、Swap Buffers、Poll Events。
4. Resize Callback 使用 Framebuffer Pixel 尺寸更新 Viewport。

完成标准：能区分 Window、Context、默认 Framebuffer、Viewport 和 Present。
