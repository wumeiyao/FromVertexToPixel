# 可运行的逐章参考实现

这里直接提供完整实现。第 0–21 章各自有一个 C++ 源文件，并由同一份 CMake 工程生成独立可执行程序 `chapter00` 到 `chapter21`。

## 第一次配置和构建

在 VS Code 中打开 CodeA 根目录，然后在 Terminal 执行：

~~~zsh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 8
~~~

这里只需配置一次。修改某一章后，只构建对应目标：

~~~zsh
cmake --build build --target chapter08
./build/exercises/chapter08
~~~

第 0–6、19、20 章是终端或文件输出程序；第 7–18、21 章会打开 GLFW 窗口，按 `Esc` 退出。图形章节还支持无窗口检查：

~~~zsh
./build/exercises/chapter12 --check
~~~

在 VS Code 的 CMake Tools 中也可以点击底部当前 Target，选择 `chapter08`，再点 Build 和 Run。它和上面的命令使用同一份 CMake 配置，不需要为每章修改 `tasks.json`。

## 章节索引

| 章节 | 完整实现 | 运行结果 |
| --- | --- | --- |
| 00 | [工程起步](00_setup/00-reading-guide.md) | Hello 与编译链 |
| 01–03 | [渲染流程](01_math/01-rendering-mental-model.md)、[向量](01_math/02-vectors-matrices.md)、[空间](01_math/03-spaces-camera-projection.md) | 终端数值 |
| 04–06 | [Framebuffer](02_cpu_rasterizer/04-cpu-framebuffer.md)、[Triangle](02_cpu_rasterizer/05-cpu-triangles.md)、[Depth/Texture](02_cpu_rasterizer/06-cpu-depth-texture-clipping.md) | PPM 图片 |
| 07–08 | [Context](03_opengl_basics/07-opengl-context-resources.md)、[Draw Call](03_opengl_basics/08-shaders-first-draw.md) | OpenGL 窗口 |
| 09–10 | [模型资产](04_scene_camera_assets/09-mesh-model-assets.md)、[Camera/Culling](04_scene_camera_assets/10-scene-camera-culling.md) | 模型与场景 |
| 11–12 | [光照](05_lighting_shadows/11-color-material-lighting.md)、[阴影](05_lighting_shadows/12-shadow-mapping.md) | 光照与硬阴影 |
| 13–14 | [Pass/Queue](06_frame_renderer/13-render-passes-and-queues.md)、[透明/Sky/Instancing](06_frame_renderer/14-transparency-sky-instancing.md) | 帧组织与特殊路径 |
| 15–16 | [后处理](07_postprocess_ui/15-framebuffers-postprocess.md)、[调试 UI](07_postprocess_ui/16-ui-debug-tools.md) | FBO 与诊断界面 |
| 17–18 | [游戏循环](08_gameplay/17-game-loop-render-snapshot.md)、[物理/音频/资产](08_gameplay/18-physics-audio-assets.md) | 系统整合 |
| 19–20 | [性能](09_profiling_vulkan/19-profiling-optimization.md)、[显式 API](09_profiling_vulkan/20-vulkan-bridge.md) | CSV 与命令状态模拟 |
| 21 | [毕业小样](10_capstone/21-capstone.md) | 可玩的收集关卡 |

## 学习方法

先运行并确认基准结果，再阅读相邻 `.cpp`。每次只改一个变量，先预测现象，运行验证，然后恢复或提交。完整参考实现是起点，不是要求背诵的标准答案。
