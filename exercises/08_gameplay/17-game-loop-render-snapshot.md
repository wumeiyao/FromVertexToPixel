# 第 17 章练习：固定更新与 Render Snapshot

对应教材：[第 17 章](../../chapters/17-game-loop-render-snapshot.md)  
完整源码：[chapter17.cpp](chapter17.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter17
./build/exercises/chapter17
~~~

使用 `WASD` 移动，`P` 暂停/恢复，暂停后按 `N` 推进一个 Fixed Tick。窗口标题显示本帧 Fixed Step 数。

## 数据流

~~~text
GLFW Input -> PlayerIntent -> 60 Hz Fixed Update
-> previous/current Transform -> interpolated RenderSnapshot -> Renderer
~~~

GameWorld 不包含 OpenGL Handle，Renderer 也不读取 GLFW 或 GameWorld。`buildSnapshot` 是两层之间唯一的数据翻译点。

程序限制超大 Frame Delta 和单帧最大 Fixed Step，避免断点恢复后长时间追赶。`--check` 会打印 `10 ms、25 ms、8 ms` 三帧对应的 Tick、剩余 Accumulator 和 Alpha。

完成标准：能解释插值为什么只影响显示位置，不能写回模拟状态。
