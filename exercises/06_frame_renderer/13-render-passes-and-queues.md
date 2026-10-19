# 第 13 章练习：Render Pass 与 Opaque Queue

对应教材：[第 13 章](../../chapters/13-render-passes-and-queues.md)  
完整源码：[chapter13.cpp](chapter13.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter13
./build/exercises/chapter13
~~~

程序先建立 CPU `DrawItem` Queue，计算 Camera Distance，再按 Material 和 Front-to-Back 规则稳定排序。Terminal 打印 `M0 M0 M1 M1 M2 M2`，窗口标题显示 Draw 和 Material Bind 数。

`beginOpaquePass` 是这一章的核心：它在 Pass 入口完整建立 Framebuffer、Viewport、Depth、Blend、Cull 和 Clear，而不是依赖上一段代码遗留的 OpenGL 状态。

## 对照实验

- 注释 `glDisable(GL_BLEND)`，先在前一帧末尾打开 Blend。
- 删除 Material 排序，比较状态切换数，确认画面仍正确。
- 将 Opaque 改成 Back-to-Front，说明正确性为何不变但 Early-Z 机会可能变化。

完成标准：能分别说明 Queue 决定“画谁和顺序”，Pass 决定“写哪里和使用什么状态”。
