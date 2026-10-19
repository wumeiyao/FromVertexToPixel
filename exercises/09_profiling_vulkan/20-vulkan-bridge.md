# 第 20 章练习：显式 API 的命令和资源状态

对应教材：[第 20 章](../../chapters/20-vulkan-bridge.md)  
完整源码：[chapter20.cpp](chapter20.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter20
./build/exercises/chapter20
~~~

当前环境没有 Vulkan SDK 或 Metal C++ 封装，因此本章提供完整可运行的显式状态模拟器，而不是伪造 `vk*` 调用。它录制三个 Frame 的 Command List，输出 Shadow Depth 与 Swapchain Image 的状态转换、Pass 命令、Queue Submit 和 Present 依赖。

示例故意让：

~~~text
Frame Slot       = 0, 1, 0
Swapchain Image  = 1, 0, 2
~~~

由此证明 Frame Slot 是 CPU/GPU 临时资源复用槽，Swapchain Image 是本帧实际获得的显示图片，两者不能混为一个索引。

完成标准：能逐条解释 `Depth Write -> Shader Read` 为什么是 RAW 依赖，以及 Barrier、Semaphore、Fence 分别控制什么。

安装 Vulkan SDK 或选择 Metal 后，下一步是把同一命令顺序翻译成真实的 Clear + Triangle 原型；不需要重写 Game、Physics、Audio 或 Snapshot。
