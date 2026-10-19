# 第 15 章练习：离屏 FBO 与后处理

对应教材：[第 15 章](../../chapters/15-framebuffers-postprocess.md)  
完整源码：[chapter15.cpp](chapter15.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter15
./build/exercises/chapter15
~~~

按 `1` 显示 Gray，按 `2` 显示 HDR + Reinhard，按 `3` 显示 Clamp Copy。程序先把高亮 Cube 写入 `RGBA16F` Scene Color 和 Depth Renderbuffer，再用无 Vertex Buffer 的 Fullscreen Triangle采样 Scene Color。

## 重点代码

- `SceneTargets::resize` 成对销毁并重建所有分辨率相关资源。
- Color 与 Depth Attachment 尺寸一致，创建后检查 FBO Complete。
- Scene Pass 的 Output 是 Scene Color；Post Pass 读取它并写默认 Framebuffer。
- 输入 Texture 与输出 Framebuffer 不会形成同一 Texture 的 Feedback Loop。

## 对照实验

- 拖动、最小化和恢复窗口，观察重建日志。
- 将 Color Attachment 改成不同尺寸，阅读 FBO 检查结果。
- 比较 Copy Clamp 与 Tone Mapping 对大于 1 的 HDR 值的处理。

完成标准：能明确写出两个 Pass 的 Input、Output、Viewport 和资源生命周期。
