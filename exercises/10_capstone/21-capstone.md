# 第 21 章练习：可玩的毕业小样

对应教材：[第 21 章](../../chapters/21-capstone.md)  
完整源码：[chapter21.cpp](chapter21.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter21
./build/exercises/chapter21
~~~

操作：

- `Enter`：从 Title 开始。
- `WASD`：移动玩家。
- `P`：暂停/恢复。
- `R`：完整重置。
- `Esc`：退出。

目标是在障碍之间收集 5 个橙色方块。每次收集播放预加载短音效，顶部 UI 更新进度；全部收集后进入 Finished。窗口标题始终显示 State 和 Score。

## 已形成的闭环

~~~text
Input -> Intent -> Fixed Game Update -> Collision/Collect Event
-> Audio + Game State -> immutable RenderSnapshot -> Scene/UI Pass -> Present
~~~

源码包含 Title、Playing、Paused、Finished、Restart，固定更新、障碍碰撞、一次性收集、插值 Snapshot、方向光基础着色、HUD、音效和退出资源释放。`--check` 会无窗口收集全部 5 个对象并断言最终状态为 Finished。

这是核心 Vertical Slice，不是整本书所有增强项的堆叠。Shadow、透明、Skybox、HDR、Instancing 和真实性能报告已经在各自章节有独立实现，应按项目范围选择性整合，而不是一次全部复制进这里。

完成标准：从 Title 玩到 Finished，Pause/Restart/Exit 均正确，并能从一次收集事件追踪到声音、状态、Snapshot、画面和 UI。
