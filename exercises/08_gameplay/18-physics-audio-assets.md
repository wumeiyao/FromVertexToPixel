# 第 18 章练习：物理、音频与共享资产

对应教材：[第 18 章](../../chapters/18-physics-audio-assets.md)  
完整源码：[chapter18.cpp](chapter18.cpp)  
音频辅助实现：[audio.hpp](../common/audio.hpp)

## 构建运行

~~~zsh
cmake --build build --target chapter18
./build/exercises/chapter18
~~~

一个 Dynamic Box 按 60 Hz 重力下落并与 Static Ground 接触，碰撞时播放程序启动时生成并预加载的短 WAV。Ground 与 Box 两个 Entity 从 `MeshCache` 获取同一个 GPU Mesh，Terminal 会打印“两个对象、一份资源”。

这个样例只实现单 Dynamic Body 与平面/边界碰撞，用来演示固定步、Transform 权威、一次性事件、预加载音频和共享资源生命周期，不是假装替代通用 Physics Engine。进入毕业工程后应把 `fixedPhysicsStep` 的接口替换为 Jolt 等成熟库。

## 对照实验

- 把 Physics 推进移到渲染帧，比较不同帧率下结果。
- 让两次 `loadCube` 使用不同 Key，观察 GPU Resource 数。
- 把 AudioSystem 创建移到碰撞分支，解释重复加载问题。

完成标准：能区分 Entity、Body、Collision Shape、Mesh、Audio Buffer 和播放事件的职责。
