# 第 16 章练习：最小调试 UI 与状态开关

对应教材：[第 16 章](../../chapters/16-ui-debug-tools.md)  
完整源码：[chapter16.cpp](chapter16.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter16
./build/exercises/chapter16
~~~

按 `1` 切换 Depth Test，`2` 切换 Back-face Culling，`3` 停止旋转，`4` 隐藏调试 Overlay。绿色块表示开，红色块表示关；窗口标题显示 FPS 和全部状态。

这个样例没有引入尚未安装的 Dear ImGui，而是直接用 Triangle、Alpha Blend 和 Scissor 实现最小 Immediate-mode Overlay，因此能清楚看到 UI 最终仍走 GPU 管线。完整项目可把这层替换为 Dear ImGui，Renderer 诊断数据结构不变。

## 阅读顺序

1. Scene Pass 每帧显式建立 3D State。
2. UI Pass 关闭 Depth/Cull，开启 Blend/Scissor。
3. 按键使用边沿检测，按住不会每帧反复切换。
4. Window Size 和 Framebuffer Size 不混用。

完成标准：能通过开关制造并解释一个画面错误，而不是随机修改 Shader。
