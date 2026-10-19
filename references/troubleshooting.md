# 排错索引：按“你看到什么”查

卡住的时候先用这一页。它不解释原理，只负责把你送到**正确的那一节**。

各章的「常见错误」表更详细。这里的作用是：当你还不知道问题属于哪一章时，先定位该翻哪一章。

## 先做这三件事

遇到任何画面问题，在改代码之前先做完这三步。它们能排掉一多半的情况：

1. **只留一个变量。** 关掉阴影、后处理、UI、透明，只画一个不透明物体。错误还在吗？
2. **找到第一个错的中间结果。** 不要从最终画面猜原因。按数据流顺序往前找，直到找到第一处已经错了的数据。
3. **提出一个能被证据推翻的假设。** 「我认为是 UV 颠倒」可以通过显示 UV 渐变来验证；「可能哪里不对」不行。

> 最常见的失败方式不是“找不到原因”，而是**一次改了三个地方**，然后不知道是哪一个起了作用。每次只改一处，改完就看结果。

---

## 1. 还没跑起来

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 找不到 `glad/glad.h` | 本书 macOS 路线不用 GLAD | [7.4](../chapters/07-opengl-context-resources.md)、[环境搭建](../plan/02-vscode-on-macos.md) |
| 找不到 GLFW 或 glm | 依赖没装 | `brew install glfw glm ninja`，见[环境搭建](../plan/02-vscode-on-macos.md) |
| undefined symbol / 链接失败 | 声明有、实现没参与链接 | [零基础前置 §7](beginner-prerequisites.md) |
| CMake 改了没生效 | 需要重新 Configure | [第一个 C++ 工程 §9](../plan/03-first-cpp-project-in-vscode.md) |
| 窗口创建失败（macOS） | 没请求 4.1 Core / forward-compatible | [7.5](../chapters/07-opengl-context-resources.md) |
| 加载 OpenGL 函数失败 | Context 还没 Current | [7.21](../chapters/07-opengl-context-resources.md) |
| 非 macOS 平台编译不过 | 参考实现目前限定 macOS | [环境搭建“不用 macOS 怎么办”](../plan/02-vscode-on-macos.md) |

## 2. 黑屏，什么都没有

这是最常见也最容易乱改的情况。**按顺序**查，不要跳：

| 顺序 | 检查 | 去哪看 |
| --- | --- | --- |
| 1 | Clear Color 是否生效？看不到背景色说明循环或 Swap 有问题 | [7.6](../chapters/07-opengl-context-resources.md) |
| 2 | Shader 编译和链接是否成功？有没有读过状态和日志 | [8.23](../chapters/08-shaders-first-draw.md) |
| 3 | Uniform 位置是不是 -1，Program 是否绑定 | [8.23](../chapters/08-shaders-first-draw.md) |
| 4 | `gl_Position` 是否落在 Clip 范围内，`w` 对不对 | [8.23](../chapters/08-shaders-first-draw.md) |
| 5 | VAO 的 stride / offset / location 是否和 Shader 一致 | [7.21](../chapters/07-opengl-context-resources.md)、[9.17](../chapters/09-mesh-model-assets.md) |
| 6 | 物体是不是全在相机后面，或被视锥裁掉了 | [3.25](../chapters/03-spaces-camera-projection.md)、[10.17](../chapters/10-scene-camera-culling.md) |
| 7 | 是不是写到了错误的 Framebuffer | [13.18](../chapters/13-render-passes-and-queues.md)、[15.26](../chapters/15-framebuffers-postprocess.md) |
| 8 | 矩阵看起来都对还是黑屏：行列约定、乘法顺序、上传转置 | [2.22](../chapters/02-vectors-matrices.md)、[3.25](../chapters/03-spaces-camera-projection.md) |

## 3. 形状不对

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 模型像尖刺一样爆炸 | stride、offset 或索引类型错 | [9.17](../chapters/09-mesh-model-assets.md) |
| 三角形完全不画 | 绕序与边函数符号不匹配 | [5.20](../chapters/05-cpu-triangles.md) |
| 所有面被剔除 | CCW/CW 约定或 y 翻转 | [8.23](../chapters/08-shaders-first-draw.md)、[5.20](../chapters/05-cpu-triangles.md) |
| 只画出包围盒一角 | 三条边方向不统一 | [5.20](../chapters/05-cpu-triangles.md) |
| 两个三角形之间有缝或重叠 | 共享边归属规则不一致 | [5.20](../chapters/05-cpu-triangles.md) |
| 极细三角形出现 NaN | 面积接近 0 还在算权重 | [5.20](../chapters/05-cpu-triangles.md) |
| 靠近相机出现整屏尖刺 | 透视除法前没裁剪 | [3.25](../chapters/03-spaces-camera-projection.md)、[6.24](../chapters/06-cpu-depth-texture-clipping.md) |
| 只出现了一部分模型 | 多 Primitive 没遍历 | [9.17](../chapters/09-mesh-model-assets.md) |
| 实例全挤在同一位置 | Attribute Divisor 没设成 1 | [14.24](../chapters/14-transparency-sky-instancing.md) |

## 4. 位置或大小不对

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 物体绕世界原点转圈 | 平移与旋转顺序错 | [2.22](../chapters/02-vectors-matrices.md) |
| 相机向右，世界也向右 | View Matrix 没取逆 | [3.25](../chapters/03-spaces-camera-projection.md) |
| 圆变成椭圆 | aspect 错，或 resize 后没更新 | [3.25](../chapters/03-spaces-camera-projection.md)、[7.7](../chapters/07-opengl-context-resources.md) |
| 远近大小完全相反 | 透视除法符号或矩阵约定混用 | [3.25](../chapters/03-spaces-camera-projection.md) |
| 画面上下颠倒 | y 原点约定不一致，或翻转了两次 | [4.17](../chapters/04-cpu-framebuffer.md)、[3.25](../chapters/03-spaces-camera-projection.md) |
| 父节点动了，子 Mesh 不动 | World Matrix 没递归传播 | [10.17](../chapters/10-scene-camera-culling.md) |
| 各部件位置散开 | Node 父子矩阵顺序错 | [9.17](../chapters/09-mesh-model-assets.md) |
| 旋转角度完全不对 | 把度直接传给了需要弧度的函数 | [2.22](../chapters/02-vectors-matrices.md) |
| 相机右方向反了 | 叉积顺序反了 | [2.22](../chapters/02-vectors-matrices.md) |
| Instance 矩阵扭曲 | Attribute offset 或列顺序错 | [14.24](../chapters/14-transparency-sky-instancing.md) |

## 5. 颜色不对

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 模型全黑 | `N·L` 全负、灯强为 0、Texture 没绑 | [11.19](../chapters/11-color-material-lighting.md) |
| 整体过亮、发白 | 做了两次 Gamma/sRGB 编码 | [11.19](../chapters/11-color-material-lighting.md)、[15.26](../chapters/15-framebuffers-postprocess.md) |
| 整体过暗 | 忘了最终 sRGB 编码，或直接用 sRGB 做光照 | [11.19](../chapters/11-color-material-lighting.md) |
| 模型从内部亮 | `L` 方向正负号反了 | [11.19](../chapters/11-color-material-lighting.md) |
| 物体旋转，光照跟着转 | Local Normal 和 World Light 混用 | [11.19](../chapters/11-color-material-lighting.md) |
| 非均匀缩放后明暗扭曲 | 没用 inverse-transpose Normal Matrix | [11.19](../chapters/11-color-material-lighting.md) |
| 高光粘在固定位置 | `V` 算错或空间不一致 | [11.19](../chapters/11-color-material-lighting.md) |
| 光照像碎片一样断裂 | normal 读错或硬边顶点共享错 | [9.17](../chapters/09-mesh-model-assets.md) |
| 亮度忽明忽暗或大于 1 | 点积前没归一化 | [2.22](../chapters/02-vectors-matrices.md) |
| 颜色溢出或绕回 | float 没 clamp 就转整数 | [4.17](../chapters/04-cpu-framebuffer.md) |
| 缩放到 0 后出现 NaN | Normal Matrix 不可逆 | [11.19](../chapters/11-color-material-lighting.md) |

## 6. 前后遮挡不对

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 后画的物体总在上面 | 没开深度测试，或没写深度 | [6.24](../chapters/06-cpu-depth-texture-clipping.md) |
| 所有 Fragment 都被丢弃 | Clear Depth 与比较方向不匹配 | [6.24](../chapters/06-cpu-depth-texture-clipping.md) |
| 移动的物体留下残影或洞 | 每帧没清 Depth Buffer | [6.24](../chapters/06-cpu-depth-texture-clipping.md) |
| 表面闪烁（Z-fighting） | near 太小、far 太大，或几乎共面 | [6.6](../chapters/06-cpu-depth-texture-clipping.md)、[3.25](../chapters/03-spaces-camera-projection.md) |
| 交换绘制顺序画面就变 | 深度没正确写入或比较 | [6.24](../chapters/06-cpu-depth-texture-clipping.md) |
| 立方体各面互相覆盖错误 | 没开或没清 Depth | [8.23](../chapters/08-shaders-first-draw.md) |
| 屏幕中的对象突然消失 | 裁剪平面法线反向，或 bounds 太小 | [10.17](../chapters/10-scene-camera-culling.md) |

## 7. 贴图不对

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 形状对但全白 | Texture 没绑定，Sampler uniform 错 | [9.17](../chapters/09-mesh-model-assets.md) |
| 贴图上下颠倒 | 图片行顺序与 UV 原点约定不同 | [9.17](../chapters/09-mesh-model-assets.md)、[6.24](../chapters/06-cpu-depth-texture-clipping.md) |
| 倾斜表面上贴图游动 | 用了屏幕线性 UV，没做透视校正 | [6.24](../chapters/06-cpu-depth-texture-clipping.md) |
| 远处贴图闪烁 | 没生成 Mipmap，或 min filter 不对 | [9.17](../chapters/09-mesh-model-assets.md) |
| UV=1 时越界 | 索引算到了 width/height | [6.24](../chapters/06-cpu-depth-texture-clipping.md) |
| 裁剪后贴图断裂 | 新顶点只插值了 position | [6.24](../chapters/06-cpu-depth-texture-clipping.md) |
| Normal Map 凹凸反向 | 绿色通道约定或 TBN 手性不同 | [11.19](../chapters/11-color-material-lighting.md) |

## 8. 阴影不对

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 完全没有阴影 | FBO 不完整、Caster 没画、Sampler 绑错 | [12.18](../chapters/12-shadow-mapping.md) |
| 整个场景都黑 | 比较方向反了，或 Depth Texture 全 0 | [12.18](../chapters/12-shadow-mapping.md) |
| 表面出现密集条纹（Shadow Acne） | Bias 太小 | [12.11](../chapters/12-shadow-mapping.md)、[12.12](../chapters/12-shadow-mapping.md) |
| 阴影和物体脱开（Peter-Panning） | Bias 太大 | [12.12](../chapters/12-shadow-mapping.md) |
| 阴影边缘被切断 | Light 正交范围太小 | [12.18](../chapters/12-shadow-mapping.md) |
| 阴影贴在错误位置 | 忘了 Model Matrix，或 Light VP 顺序错 | [12.18](../chapters/12-shadow-mapping.md) |
| 相机移动时阴影抖动 | Light Projection 每帧微动 | [12.16](../chapters/12-shadow-mapping.md) |
| Resize 后主画面对、阴影错 | Shadow Viewport 泄漏 | [12.18](../chapters/12-shadow-mapping.md)、[13.18](../chapters/13-render-passes-and-queues.md) |

## 9. 透明和天空不对

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 透明层颜色顺序错 | 没从远到近排序 | [14.24](../chapters/14-transparency-sky-instancing.md) |
| 透明物体挡掉了后面的透明 | Depth Write 没关 | [14.24](../chapters/14-transparency-sky-instancing.md) |
| 透明显示在墙前面 | Depth Test 被关掉了 | [14.24](../chapters/14-transparency-sky-instancing.md) |
| 透明边缘发黑 | Premultiplied 与 Blend State 不匹配 | [14.7](../chapters/14-transparency-sky-instancing.md) |
| 不透明物体看起来半透明 | Blend 状态没关 | [13.18](../chapters/13-render-passes-and-queues.md) |
| 相机移动时天空跟着靠近 | View Matrix 没移除平移 | [14.15](../chapters/14-transparency-sky-instancing.md) |
| 天空覆盖整个场景 | Depth Function 或 Depth Write 错 | [14.15](../chapters/14-transparency-sky-instancing.md) |
| Cubemap 六个面方向错乱 | 面顺序或旋转错 | [14.15](../chapters/14-transparency-sky-instancing.md) |

## 10. 后处理不对

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 离屏后完全黑屏 | FBO 不完整，或全屏 Texture 没绑 | [15.26](../chapters/15-framebuffers-postprocess.md) |
| 场景只显示一部分 | Viewport 还是上一个 Pass 的尺寸 | [15.26](../chapters/15-framebuffers-postprocess.md)、[13.18](../chapters/13-render-passes-and-queues.md) |
| 后处理随机闪烁 | 同一张 Texture 同时读写 | [15.26](../chapters/15-framebuffers-postprocess.md) |
| HDR 亮处全是一样的白 | Scene Color 实际是 RGBA8 | [15.26](../chapters/15-framebuffers-postprocess.md) |
| Bloom 全屏泛光 | Threshold 太低，或颜色链错 | [15.26](../chapters/15-framebuffers-postprocess.md) |
| Bloom 完全没效果 | 没有超过 Threshold 的 HDR 值 | [15.26](../chapters/15-framebuffers-postprocess.md) |
| UI 被 Bloom 模糊了 | UI 画在了 Post Process 之前 | [13.18](../chapters/13-render-passes-and-queues.md) |
| UI 颜色被压暗 | UI 画在了 Tone Mapping 之前 | [15.26](../chapters/15-framebuffers-postprocess.md) |

## 11. 窗口缩放之后不对

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 画面只占窗口左下角 | 没更新 `glViewport` | [7.7](../chapters/07-opengl-context-resources.md)、[7.21](../chapters/07-opengl-context-resources.md) |
| 比例错误 | 用了窗口逻辑尺寸而非 Framebuffer 尺寸 | [7.7](../chapters/07-opengl-context-resources.md) |
| 画面被拉伸 | 离屏 Texture 没随 resize 重建 | [15.26](../chapters/15-framebuffers-postprocess.md) |
| 最小化后崩溃 | 高度为 0 还在做除法 | [7.7](../chapters/07-opengl-context-resources.md) |
| Resize 后边缘裁剪错误 | Projection 更新了，Frustum 没更新 | [10.17](../chapters/10-scene-camera-culling.md) |

## 12. 运动和时间不对

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 帧率变了，移动速度也变了 | 用 deltaTime 直接驱动逻辑 | [17.23](../chapters/17-game-loop-render-snapshot.md) |
| 卡顿后越来越卡（死亡螺旋） | 固定步补做没有上限 | [17.11](../chapters/17-game-loop-render-snapshot.md) |
| 按一次键跳了多次 | 一次性事件被多个 Tick 重复消费 | [17.23](../chapters/17-game-loop-render-snapshot.md) |
| 按键偶尔失效 | 事件在固定更新前被丢弃 | [17.23](../chapters/17-game-loop-render-snapshot.md) |
| 画面抖动但数值正确 | 相机和物体用了不同的插值 alpha | [17.23](../chapters/17-game-loop-render-snapshot.md) |
| 物体穿墙 | 固定步太大，或 Shape 太薄 | [18.32](../chapters/18-physics-audio-assets.md) |
| 物体不断抖动 | Game 和 Physics 同时写 Transform | [18.32](../chapters/18-physics-audio-assets.md) |
| 模型和碰撞体错位 | Pivot、导入缩放或局部偏移 | [18.32](../chapters/18-physics-audio-assets.md) |
| 画面撕裂 | VSync 没开，或交换发生在扫描中途 | [7.8](../chapters/07-opengl-context-resources.md) |

## 13. 声音不对

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 完全无声 | Device、Master Gain、Buffer 是否有效 | [18.33](../chapters/18-physics-audio-assets.md) |
| 只响一次就再也不响 | Source 状态和回收池 | [18.33](../chapters/18-physics-audio-assets.md) |
| 一次事件响了好几声 | 事件消费次数不对 | [18.20](../chapters/18-physics-audio-assets.md) |
| 左右方向相反 | Listener 坐标系的前/上方向 | [18.33](../chapters/18-physics-audio-assets.md) |
| 爆音或失真 | 多 Source 相加削波，或格式/采样率错 | [18.33](../chapters/18-physics-audio-assets.md) |

## 14. 退出或资源问题

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 程序退出时崩溃 | Context 先销毁，之后还在调 GL | [7.21](../chapters/07-opengl-context-resources.md)、[18.30](../chapters/18-physics-audio-assets.md) |
| 内存或显存持续增长 | 资源重复创建没释放 | [9.15](../chapters/09-mesh-model-assets.md) |
| 每帧明显卡顿 | 每帧重建并上传静态资源 | [7.21](../chapters/07-opengl-context-resources.md) |
| 改了 Shader 但结果没变 | 路径错，或链接失败后仍继续运行 | [8.23](../chapters/08-shaders-first-draw.md) |
| 改了像素但文件没变 | 修改发生在保存之后 | [4.17](../chapters/04-cpu-framebuffer.md) |

## 15. 性能不对

| 你看到什么 | 最先怀疑 | 去哪看 |
| --- | --- | --- |
| 不知道瓶颈在哪 | 还没测量就开始改 | [19.17](../chapters/19-profiling-optimization.md) |
| 降分辨率帧率大涨 | 瓶颈在片段处理或带宽 | [19.17](../chapters/19-profiling-optimization.md) |
| 减少对象数量没变化 | 瓶颈不在 CPU 提交 | [19.17](../chapters/19-profiling-optimization.md) |
| Draw Call 降了但 FPS 不变 | 瓶颈不在提交次数 | [14.22](../chapters/14-transparency-sky-instancing.md)、[19.17](../chapters/19-profiling-optimization.md) |
| 开了裁剪但 Draw Call 没降 | World Bounds 没更新或半径过大 | [10.17](../chapters/10-scene-camera-culling.md) |
| 两次测量数字差很多 | 测试场景不受控 | [19.8](../chapters/19-profiling-optimization.md) |

---

## 还是定位不了

说明你缺的不是这一页，而是**观察手段**。补这几样，比继续猜更快：

1. 把中间结果直接显示出来：Normal 映射成 RGB、UV 映射成渐变、Depth 映射成灰度、Shadow Map 缩略图。见[第 16 章](../chapters/16-ui-debug-tools.md)。
2. 逐 Pass 开关。一次只留一个 Pass，找出第一个出错的。见[13.17](../chapters/13-render-passes-and-queues.md)。
3. 用已知答案的最小输入。单位矩阵、纯平移矩阵、2×2 的图片、一个正对相机的三角形。
4. 把现象、假设、证据、结论写进 [plan/progress.md](../plan/progress.md)。同一个坑踩第二次时，这份记录比记忆可靠。
