# 第 14 章：透明、天空与实例化

<!-- nav -->
> [上一章：Render Pass 与渲染队列](13-render-passes-and-queues.md) ｜ [下一章：离屏 Framebuffer 与后处理](15-framebuffers-postprocess.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/06_frame_renderer/14-transparency-sky-instancing.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

本章包含三个看似无关、却都会改变普通 Opaque 绘制路径的主题：

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 手算一次 Alpha 混合，并解释为什么透明的绘制顺序会改变结果。
2. 说明透明物体为什么开 Depth Test 但关 Depth Write。
3. 区分 Straight Alpha、Premultiplied Alpha 和 Alpha Cutout 三种处理方式。
4. 解释天空为什么不能当成一个普通远处盒子，以及怎样移除 Camera 平移。
5. 用方向向量从 Cubemap 取样，并把 Skybox 放在正确的绘制位置。
6. 说明 Draw Call 数量为什么会成为瓶颈，Instancing 又节省了什么。
7. 上传 Per-Instance 数据完成一次实例化绘制，并说明一个 mat4 为什么占四个 Attribute Location。

- 透明表面不能简单覆盖旧颜色，必须读取并混合已有颜色。
- 天空看起来无限远，不应像普通世界 Mesh 那样参与平移和深度写入。
- 大量相同 Mesh 若逐个 Draw，会产生不必要的 CPU 提交成本，可以使用 Instancing。

三部分都建立在上一章的 Pass 与 Queue 之上。透明是一个特殊 Queue，天空是一个特殊 Pass，实例化改变的是 Queue 里多个 Draw Item 的提交方式。

阅读顺序是：`14.1～14.9` 透明混合，`14.10～14.15` Skybox，`14.16～14.23` Instancing。透明解决颜色怎样叠加，Skybox 解决背景方向采样，Instancing 解决 CPU 提交次数，三者应分别实现和验证。

三个专题有三个独立完成点，不要同时改三条路径：

1. 先用两张固定为 50% Alpha 的四边形调通透明排序、Depth Test、Depth Write 和 Blend。
2. 恢复一个稳定版本，再用六张带方向标签的图片调通 Skybox。
3. 最后复制 1000 个相同 Cube，把逐个 Draw 改成 Instanced Draw。

任何一步出错时，都回到该部分开始前的稳定画面；不要让透明、Cubemap 朝向和 Instance Buffer 三种问题同时存在。

## 第一部分：透明混合

### 14.1 不透明覆盖与透明混合

不透明表面通过深度测试后，可以直接替换 Pixel 旧颜色：

~~~text
newFramebufferColor = objectColor
~~~

透明玻璃不同。我们既要看见玻璃颜色，也要保留玻璃后面的场景。因此需要把新颜色和旧颜色组合。

先定义两个术语：

- Source，源：当前 Fragment Shader 输出的新颜色。
- Destination，目标：Framebuffer 中该位置已经存在的颜色。

标准 Straight Alpha 公式：

~~~text
Cout = Cs * As + Cd * (1 - As)
~~~

符号含义：

- `Cs`：Source RGB。
- `As`：Source Alpha。
- `Cd`：Destination RGB。
- `Cout`：混合后的 RGB。

Alpha 越接近 1，新颜色占比越大；越接近 0，背景占比越大。

---

### 14.2 一次透明混合的数字计算

已有蓝色背景：

~~~text
Cd = (0, 0, 1)
~~~

现在画一层 Alpha 为 0.25 的红色玻璃：

~~~text
Cs = (1, 0, 0)
As = 0.25
~~~

计算：

~~~text
Cout = (1,0,0) * 0.25 + (0,0,1) * 0.75
     = (0.25, 0, 0.75)
~~~

最终偏蓝，因为玻璃只贡献 25%。

OpenGL Straight Alpha 设置：

~~~cpp
glEnable(GL_BLEND);
glBlendEquation(GL_FUNC_ADD);
glBlendFuncSeparate(
    GL_SRC_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_ONE,
    GL_ONE_MINUS_SRC_ALPHA
);
~~~

`glBlendFuncSeparate` 让 RGB 和 Alpha 使用不同 Source Factor。最终 Alpha 常按：

~~~text
Aout = As + Ad * (1 - As)
~~~

混合应在线性颜色空间进行。启用正确的 sRGB Framebuffer 写入时，OpenGL 会为混合读取线性 Destination，再将最终结果编码回 sRGB。若直接在 sRGB 编码值上混合，边缘和颜色过渡会偏暗。

---

### 14.3 为什么透明顺序会改变结果

混合公式依赖当前 `Cd`。先画哪个透明层，后一个看到的 Destination 就不同。

假设背景为黑，远处蓝玻璃和近处红玻璃 Alpha 都是 0.5。

#### 正确：先远蓝，再近红

~~~text
蓝与黑混合 -> (0, 0, 0.5)
红再混合   -> (0.5, 0, 0.25)
~~~

#### 错误：先近红，再远蓝

~~~text
红与黑混合 -> (0.5, 0, 0)
蓝再混合   -> (0.25, 0, 0.5)
~~~

两个结果不同。因此普通 Alpha Blending 通常要求：

~~~text
先画所有 Opaque
再把 Transparent 从远到近绘制
~~~

这叫 Back-to-Front Sorting，后向前排序。

---

### 14.4 透明物体的 Depth Test 与 Depth Write

这两个开关必须分开理解。

#### Depth Test 保持开启

透明物体仍应被不透明墙挡住。Depth Test 读取 Opaque Pass 已写好的 Depth Buffer，防止墙后的玻璃显示在墙前。

#### Depth Write 通常关闭

透明表面不能像不透明表面一样轻易阻止后续透明 Fragment。

~~~cpp
glEnable(GL_DEPTH_TEST);
glDepthMask(GL_FALSE);  // 仍测试，但不更新 Depth Buffer
glEnable(GL_BLEND);
~~~

Pass 结束后必须恢复：

~~~cpp
glDepthMask(GL_TRUE);
glDisable(GL_BLEND);
~~~

为什么不写深度？假设近玻璃先写入 Depth，后面的透明层即使应该透过它可见，也会在 Depth Test 中被完全丢弃，连混合机会都没有。

“从远到近排序”与“关闭 Depth Write”必须一起使用。只做其中一项仍可能出现错误。

---

### 14.5 透明排序怎样计算距离

最简单做法是用对象 Bounds Center 到 Camera 的距离：

~~~cpp
float distanceSquared = lengthSquared(
    item.worldBounds.center - camera.position
);
~~~

按 `distanceSquared` 从大到小排序，不需要开平方。

更贴近观察方向的方式是 View Space Depth：

~~~text
viewPosition = V * worldCenter
~~~

再按相机前向深度排序。它比球形距离更符合屏幕前后关系。

无论哪种方法，排序的都是“对象代表点”，不是每个 Triangle。因此只是近似。

---

### 14.6 为什么普通透明排序仍不完美

#### 14.6.1 相交物体

两张透明四边形互相穿过时，一部分 A 在 B 前，另一部分 A 在 B 后，无法用“整个对象谁更远”得到唯一顺序。

~~~text
\  /  两张透明面相交
 \/
 /\
/  \
~~~

#### 14.6.2 单个 Mesh 内部

一个玻璃球的后表面和前表面属于同一 Mesh。一次 Draw 内 Triangle 顺序通常不是为当前 Camera 排好的，对象级排序无法修复内部顺序。

#### 14.6.3 循环遮挡

可能出现 A 的一部分在 B 前、B 在 C 前、C 又在 A 前，没有全局线性顺序。

解决方向包括：

- 拆分 Mesh，使排序单元更小。
- 对简单玻璃分别绘制背面与正面。
- 使用 Order-Independent Transparency，简称 OIT，也就是尽量不依赖传统远近排序的透明算法。
- 使用 Weighted Blended OIT 等近似算法。它把透明颜色和权重先累积到额外的 Texture，最后统一合成。好处是不再需要严格逐对象排序，代价是结果只是近似值。

这些方案有额外成本和限制。本章的验收标准是普通远到近透明，不要求解决所有相交情况。

---

### 14.7 Straight Alpha 与 Premultiplied Alpha

#### 14.7.1 Straight Alpha

Texture RGB 保存原始颜色，Alpha 单独保存透明度：

~~~text
RGB = (1, 0, 0)
A   = 0.25
~~~

混合时再乘 Alpha：

~~~text
Cout = Cs * As + Cd * (1 - As)
~~~

对应 RGB Blend Factor：

~~~text
Source = SRC_ALPHA
Destination = ONE_MINUS_SRC_ALPHA
~~~

#### 14.7.2 Premultiplied Alpha

Texture 或 Shader 输出前，RGB 已经乘过 Alpha：

~~~text
Cp = Cs * As = (0.25, 0, 0)
A  = 0.25
~~~

混合公式变为：

~~~text
Cout = Cp + Cd * (1 - As)
~~~

OpenGL：

~~~cpp
glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
~~~

Premultiplied Alpha 常能减少 Texture 透明边缘的颜色渗漏，也更自然地组合某些加法与普通透明效果。但资产制作、Shader 输出和 Blend State 必须保持一致。

最常见错误是：Texture 已 Premultiply，却仍使用 `SRC_ALPHA`，于是 RGB 被 Alpha 乘两次，边缘过暗。

---

### 14.8 Alpha Cutout 不是半透明

树叶、铁丝网常使用一张带 Alpha 的 Texture，但只需要“有”或“没有”，不需要 30% 透明。

~~~glsl
float alpha = texture(uBaseColor, vUv).a;
if (alpha < uAlphaCutoff) {
    discard;
}
~~~

保留下来的 Fragment 完全不透明，可以：

- 开启 Depth Test。
- 开启 Depth Write。
- 不启用 Alpha Blending。

这叫 Alpha Cutout、Alpha Test 或 Masked Material。它的边缘可能有锯齿，但排序与深度行为比半透明稳定。

分类规则：

~~~text
Alpha 只有 0 或 1 的视觉需求 -> 优先考虑 Cutout
需要连续半透明 -> Transparent Blend
~~~

Cutout Caster 的 Shadow Pass 也必须执行相同 Alpha Cutoff，否则透明区域会投出完整矩形阴影。

---

### 14.9 透明调试顺序

1. 暂时关闭 Blend，把透明材质当 Opaque 画，确认 Mesh、Texture 和 UV 正确。
2. 固定 Alpha 为 0.5，开启 Blend，确认混合公式。
3. 保持 Depth Test 开，确认物体能被墙遮挡。
4. 关闭 Depth Write，放置两层透明面。
5. 显示排序距离和 Draw 顺序。
6. 最后才读取 Texture Alpha、加入粒子或折射。

若透明边缘有黑边，先检查 Straight/Premultiplied 是否匹配，再检查 Texture 透明区域的 RGB 和颜色空间。

---

进入下一部分前，先验证：透明物体保持 Depth Test、关闭 Depth Write，并按远到近绘制；交换顺序时能解释颜色为何变化。

#### 本部分检查点

**能做出来**：两个半透明面片重叠时颜色正确，并且从另一侧看过去结果也对。

**能回答**：

- 手算一次 `src*a + dst*(1-a)`。
- 透明为什么开 Depth Test 但关 Depth Write？
- Premultiplied Alpha 解决了什么问题？

**卡在这里就先不要往下读**：透明物体互相穿插时永远无法完全正确。先确认“排序后的常见情况”是对的，再往下走。

## 第二部分：天空盒

### 14.10 天空为什么不是一个普通远处盒子

天空应满足：

- Camera 旋转时，看到不同方向。
- Camera 平移时，天空不会靠近或远离。
- 它永远位于场景物体后方。
- 它只填充还没有被场景覆盖的 Pixel。

若真的在世界坐标中放一个巨大 Cube，Camera 可能移动到边界，Far Plane 也可能裁掉它，还会产生不必要的距离精度问题。

Skybox 的“无限远”主要通过 View Matrix 与 Depth State 制造，而不是靠极大坐标。

---

### 14.11 Cubemap：用方向而不是普通 UV 取样

Cubemap 是由六个正方形面组成的 Texture：

~~~text
+X, -X, +Y, -Y, +Z, -Z
~~~

普通 2D Texture 用 `(u,v)` 采样；Cubemap 用一个三维方向采样：

~~~glsl
vec3 color = texture(uSkyCubemap, normalize(direction)).rgb;
~~~

GPU 根据方向最大分量自动选择六个面中的一个，并计算面内坐标。

六张图片必须具有一致尺寸、格式和边缘方向。面顺序或朝向错误会让天空接缝、旋转或上下颠倒。

天空图片是颜色数据：LDR 是 Low Dynamic Range，低动态范围，通常只能保存普通显示范围，因此 LDR Cubemap 常按 sRGB 读取；HDR 环境贴图则保留大于 1 的线性浮点亮度。

---

### 14.12 移除 Camera 平移

普通 View Matrix 同时包含相机旋转与平移。Skybox 只需要旋转：

~~~glsl
mat4 skyView = mat4(mat3(cameraView));
~~~

取左上 3 x 3 再恢复为 4 x 4，会去除平移部分。

于是：

~~~text
Camera 旋转 -> 采样方向改变，天空转动
Camera 平移 -> Sky View 不变，天空保持无限远
~~~

---

### 14.13 在 Opaque 之后绘制 Skybox

一种常见策略是先画 Opaque，再只在最远深度处填天空。

Skybox Vertex Shader：

~~~glsl
#version 410 core

layout(location = 0) in vec3 aPosition;

uniform mat4 uSkyView;
uniform mat4 uProjection;

out vec3 vDirection;

void main() {
    vDirection = aPosition;

    vec4 clip = uProjection * uSkyView * vec4(aPosition, 1.0);

    // 令 z = w，透视除法后 NDC z = 1，即最远深度。
    gl_Position = clip.xyww;
}
~~~

Fragment Shader：

~~~glsl
#version 410 core

in vec3 vDirection;
uniform samplerCube uSkyCubemap;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 linearSky = texture(uSkyCubemap, normalize(vDirection)).rgb;
    outColor = vec4(linearSky, 1.0);
}
~~~

状态：

~~~cpp
glEnable(GL_DEPTH_TEST);
glDepthFunc(GL_LEQUAL);
glDepthMask(GL_FALSE);
glDisable(GL_BLEND);

drawSkybox();

glDepthMask(GL_TRUE);
glDepthFunc(GL_LESS);
~~~

解释：

- `xyww` 让 Sky Fragment 深度位于最远端。
- `GL_LEQUAL` 允许它通过被清为 1 的背景深度。
- 已有 Opaque 深度小于 1，Sky 无法覆盖它们。
- 关闭 Depth Write，Sky 不改变场景 Depth Buffer。

这套设置假设普通 OpenGL 深度：Clear 为 1，越近值越小。若以后使用 Reversed-Z，比较和最远值要相应改变。

---

### 14.14 先画天空还是后画天空

两种策略都存在。

#### 先画天空

关闭 Depth Write，然后画 Opaque 覆盖它。实现直观，但天空会为整屏运行 Fragment Shader，即使大部分被场景覆盖。

#### 后画天空

先有 Opaque Depth，再只填最远位置，常能少做一些无用 Fragment 工作，但 Depth State 要设置正确。

本书使用“Opaque 后、Transparent 前”绘制天空。Transparent 需要与天空颜色混合，因此天空必须先存在。

---

### 14.15 Skybox 常见错误

| 现象 | 常见原因 | 检查办法 |
| --- | --- | --- |
| Camera 移动时天空靠近 | View Matrix 没移除平移 | 固定旋转只移动 Camera |
| 天空覆盖所有物体 | Depth Function/Depth 值错误 | 只画 Opaque + Sky，显示 Depth |
| 天空不出现 | Cube 被剔除、Depth Func 不通过 | 临时关闭 Cull 和 Depth Test |
| 从 Cube 内部看不到面 | 背面剔除方向错误 | Cull Front 或关闭 Cull 对比 |
| 六个面方向错乱 | Cubemap 面顺序/旋转错误 | 给每个面写 `+X` 等标签 |
| 接缝明显 | 图片边缘不连续或采样设置错误 | 使用连续 Cubemap 资产并检查过滤 |
| 天空太暗或太亮 | sRGB/HDR 颜色链错误 | 直接显示采样值，检查编码次数 |

程序化天空可以根据方向、太阳角度和大气参数计算颜色，但它仍遵守同样原则：忽略 Camera 平移、位于最远深度、在透明之前提供背景。

---

进入下一部分前，先验证：带方向标签的 Cubemap 朝向正确；Camera 只旋转时天空转动，Camera 平移时天空位置不变。

#### 本部分检查点

**能做出来**：天空出现在所有不透明物体之后，相机移动时天空不跟着平移，转动时才变化。

**能回答**：

- 为什么要移除 View Matrix 的平移部分？
- Cubemap 用什么当采样坐标？
- 天空放在 Opaque 之前画和之后画各有什么代价？

**卡在这里就先不要往下读**：天空挡住了整个场景，通常是深度写入或深度函数没改。

## 第三部分：实例化绘制

### 14.16 Draw Call 为什么会成为问题

假设场景有 1000 棵完全相同的树：

~~~text
相同 Mesh
相同 Material
不同 Position/Rotation/Scale
~~~

普通提交可能是：

~~~cpp
for (const Tree& tree : trees) {
    setUniform("uModel", tree.modelMatrix);
    glDrawElements(...);
}
~~~

GPU 确实要处理 1000 棵树的顶点和 Fragment，但 CPU 也发出了 1000 次 Draw Call，并重复执行许多绑定和 Uniform 更新。

Instancing，实例化，让一次 Draw 重复同一个 Mesh 多次，每次读取不同的 per-instance 数据。

~~~text
普通：1000 个对象 -> 1000 Draw Calls
实例化：1000 个对象 -> 1 个 Instanced Draw
~~~

它主要减少 CPU 提交开销，不会让 GPU 只处理一棵树的顶点或 Pixel。

---

### 14.17 Per-Vertex 与 Per-Instance 数据

普通 Vertex Attribute 每个 Vertex 前进一步：

~~~text
position, normal, uv
~~~

Instance Attribute 每绘制一个实例才前进一步：

~~~text
modelMatrix, tintColor, objectId
~~~

OpenGL 用 Attribute Divisor 区分：

~~~text
divisor = 0 -> 每个 Vertex 更新，默认行为
divisor = 1 -> 每个 Instance 更新
~~~

Vertex Shader 也可以读取内建变量 `gl_InstanceID`，它从 0 到 `instanceCount - 1`。

---

### 14.18 一个 mat4 为什么占四个 Attribute Location

一个 `mat4` 有四列，每列是 `vec4`。一个 Vertex Attribute Location 一次最多描述一个向量。而 mat4 是四个向量，所以 Instance Model Matrix 要占四个连续的 Location。

假设 Mesh 已使用：

~~~text
location 0 -> position
location 1 -> normal
location 2 -> uv
~~~

Instance Matrix 可用 3、4、5、6：

~~~cpp
glBindVertexArray(meshVao);
glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);

constexpr GLuint firstLocation = 3;
constexpr GLsizei matrixStride = sizeof(Mat4);

for (GLuint column = 0; column < 4; ++column) {
    GLuint location = firstLocation + column;
    std::size_t offset = sizeof(Vec4) * column;

    glEnableVertexAttribArray(location);
    glVertexAttribPointer(
        location,
        4,
        GL_FLOAT,
        GL_FALSE,
        matrixStride,
        reinterpret_cast<void*>(offset)
    );
    glVertexAttribDivisor(location, 1);
}
~~~

这段状态属于 VAO，因此要为使用实例数据的 VAO 正确配置。

实际 `Mat4`/`Vec4` 内存布局必须与 Shader 列顺序一致。不要假定任意自定义矩阵类都能直接上传；用静态断言或明确扁平数组验证布局。

---

### 14.19 Instanced Vertex Shader

~~~glsl
#version 410 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in mat4 aInstanceModel; // 自动占用 3、4、5、6

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec2 vUv;

void main() {
    vec4 worldPosition = aInstanceModel * vec4(aPosition, 1.0);

    vWorldPosition = worldPosition.xyz;
    vWorldNormal = normalize(mat3(aInstanceModel) * aNormal);
    vUv = aUv;

    gl_Position = uProjection * uView * worldPosition;
}
~~~

这个简单版本假设实例只有旋转和统一缩放。若允许非均匀缩放，需要：

- 额外上传每实例 Normal Matrix；或
- 在 Shader 中求 inverse-transpose，成本更高；或
- 约束可实例化对象只能统一缩放。

学习项目建议先选择第三种，并把约束写进代码断言。

---

### 14.20 上传 Instance Buffer 并绘制

~~~cpp
std::vector<Mat4> visibleModels;
visibleModels.reserve(trees.size());

for (const Tree& tree : trees) {
    if (frustumVisible(tree.worldBounds, camera.frustum)) {
        visibleModels.push_back(tree.modelMatrix);
    }
}

glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
glBufferData(
    GL_ARRAY_BUFFER,
    visibleModels.size() * sizeof(Mat4),
    visibleModels.data(),
    GL_STREAM_DRAW
);

glBindVertexArray(treeMesh.vao);
glDrawElementsInstanced(
    GL_TRIANGLES,
    treeMesh.indexCount,
    GL_UNSIGNED_INT,
    nullptr,
    static_cast<GLsizei>(visibleModels.size())
);
~~~

数据流：

~~~text
每棵树单独做 Frustum Culling
-> 收集可见 Model Matrix
-> 上传 Instance Buffer
-> 绑定一次 Mesh 与 Material
-> 一次 Instanced Draw
-> GPU 用同一 Mesh 重复 instanceCount 次
~~~

`GL_STREAM_DRAW` 是使用提示，不是同步保证。后续高性能更新可能使用：Buffer Orphaning，申请一块新存储以避免等待旧数据；Persistent Mapping，让 CPU 长期保留 Buffer 映射指针；Ring Buffer，轮流使用多个 Buffer 区域。本章先保证数据正确。

---

### 14.21 哪些对象能放进同一个 Instance Batch

一个 Instanced Draw 通常要求共享：

- 同一个 Mesh/Primitive。
- 同一个 Material 与 Shader Variant。Shader Variant 是同一 Shader 因“是否使用 Normal Map、是否 Alpha Cutout”等功能开关编译出的不同版本。
- 相同的 Blend、Depth、Cull 状态。
- 相同 Texture 绑定，除非使用 Texture Array 等进阶方案。Texture Array 是尺寸和格式一致的多层 Texture，Shader 可用层编号为不同实例选择图片。

因此 Batch Key 可以是：

~~~text
(mesh, material, renderState)
~~~

1000 个不同 Mesh 不能简单合成一次普通 Instanced Draw。1000 棵树若每棵使用完全不同材质，也会拆成多个 Batch。

Per-instance Tint、Wind Phase、Object ID 等少量差异可以作为额外 Instance Attribute 上传。

---

### 14.22 Instancing 节省什么、不节省什么

#### 通常能减少

- CPU Draw Call 数量。
- 重复 Program、Material、VAO 绑定。
- 重复的每对象 Uniform 提交。

#### 不会自动减少

- 总实例数。
- GPU 顶点 Shader 执行次数。
- 屏幕覆盖 Pixel 数与 Overdraw。
- 每个实例的 Model Matrix 存储。
- 阴影 Pass 中需要绘制的可见实例。

数字例子：一个 Mesh 有 1000 个 Vertex，画 1000 个实例。

~~~text
普通 Draw：1000 Draw Calls，约 1,000,000 次 Vertex 处理
Instanced：1 Draw Call，仍约 1,000,000 次 Vertex 处理
~~~

所以 Instancing 主要解决 CPU Submission Bottleneck，也就是 CPU 花太多时间向 GPU 提交 Draw 的瓶颈。若瓶颈是 GPU Fragment Shader 或 Overdraw，Draw Call 变少也可能几乎不提高 FPS。

---

### 14.23 Instance Culling 的粒度

不能因为 Batch 的一棵树可见，就无条件绘制所有 1000 棵。

第一版在 CPU 上逐实例做 Frustum Culling，只把可见矩阵写进 Instance Buffer。这样仍保留一个 Draw，却不会处理视锥外实例。

若一个 Batch 分布覆盖整个世界，每帧重建可见列表本身也要花时间。这时才需要考虑更进一步的做法，见章末 14.27。在实际测量出问题之前，不要动它。

---

### 14.24 三部分的常见错误

| 现象 | 常见原因 | 首先检查 |
| --- | --- | --- |
| 透明层颜色顺序错误 | 未从远到近排序 | 显示 Draw 序号和距离 |
| 透明物体挡掉后层透明 | Depth Write 未关闭 | 检查 `glDepthMask` |
| 透明显示在墙前 | Depth Test 被关闭 | 查看 Opaque Depth Buffer |
| 透明边缘发黑 | Premultiplied 与 Blend State 不匹配 | 显示原始 RGB/A |
| 树叶排序很乱 | 应用 Blend 而非 Alpha Cutout | 改为 Cutoff 对比 |
| 天空随 Camera 平移 | View 没移除 translation | 只平移不旋转测试 |
| 天空覆盖场景 | 深度状态或 `xyww` 错 | 只跑 Opaque + Sky |
| 实例全在同一位置 | Divisor 未设为 1 | 显示 `gl_InstanceID` 颜色 |
| Instance Matrix 扭曲 | Attribute offset/列顺序错误 | 上传单位矩阵和纯平移矩阵测试 |
| 只有第一个实例正确 | Buffer 大小或 instanceCount 错 | 打印上传字节数与计数 |
| Draw Call 少了但 FPS 不变 | 瓶颈不在 CPU 提交 | 分析 GPU 时间和 Overdraw |

---

### 14.25 练习

#### 练习 A：手算两层透明

选择背景、远层和近层三个 RGB 与 Alpha。分别按正确和错误顺序计算最终颜色，说明结果为什么不同。

#### 练习 B：Depth State 实验

放置一堵不透明墙和两张透明面，分别测试：

1. Depth Test 关、Depth Write 关。
2. Depth Test 开、Depth Write 开。
3. Depth Test 开、Depth Write 关。

截图并解释每种现象。

#### 练习 C：Straight 与 Premultiplied

使用一张带柔和透明边缘的圆形 Texture。分别用两套正确配置和一种故意不匹配配置，观察边缘。

#### 练习 D：带标签 Cubemap

制作六张纯色并写有 `+X/-X/+Y/-Y/+Z/-Z` 的图片，确认 Cubemap 面方向。平移和旋转 Camera，写下 Skybox 分别应怎样响应。

#### 练习 E：1000 个实例

绘制 1000 个相同立方体：

- 逐对象 Draw 版本。
- Instanced Draw 版本。

记录 Draw Call、可见实例、Triangle 数和 CPU Render 时间。不要只记录 FPS。

#### 练习 F：Instance 调试属性

为每个实例上传 Tint Color 或用 `gl_InstanceID` 生成颜色，验证 GPU 确实能区分实例。

#### 练习 G：故意出错实验（破坏三条特殊路径）

三部分各挑几条，每次只改一处，先预测，再运行，然后恢复：

**透明**

1. 透明队列不排序，直接按提交顺序画。
2. 透明绘制时打开 Depth Write。
3. 用 Premultiplied 的贴图，但 Blend State 按 Straight Alpha 设置。

**天空**

4. 不移除 View Matrix 的平移部分，然后让相机走远。
5. 把 Skybox 的 Depth Function 改成默认的 Less。

**实例化**

6. 把 Attribute Divisor 从 1 改成 0。
7. 只为 mat4 分配一个 Attribute Location，而不是四个。
8. `instanceCount` 传对了，但 Instance Buffer 只上传了一半数据。

第 6 条的现象（所有实例挤在同一个位置）是实例化最经典的入门错误，值得亲手见一次。

---

### 14.26 本章验收标准

本章三部分互相独立，应分别验收。三部分的第 1～3 条都是**最低验收**，标注“加分完善”的属于**推荐验收**；章末 14.27 的内容属于**扩展**，第一遍不做。

透明部分完成时：

1. Transparent Queue 在 Opaque 之后、Post Process 和 UI 之前，从远到近绘制；若实现 Skybox，则还要在 Sky 之后。
2. Transparent 开启 Depth Test、关闭 Depth Write，并使用明确 Blend State。
3. 能解释 Straight 与 Premultiplied Alpha 的数据和公式差异。
4. 推荐验收：Alpha Cutout 物体正确写深度，并投出匹配轮廓的阴影。

天空部分完成时：

1. Skybox 随 Camera 旋转但不随平移变化。
2. Skybox 只填背景，不会覆盖 Opaque，并在 Transparent 之前提供背景色。
3. 六张带方向标签的测试图片朝向正确。

实例化部分完成时：

1. 1000 个相同 Mesh 可以通过一次 Instanced Draw 绘制。
2. 可见性裁剪仍按实例进行，不会无条件提交整个世界。
3. 统计能证明 Draw Call 下降，同时保留真实 Vertex/Triangle 数。

进入下一章前，请确认自己能回答：

- Source 与 Destination 分别来自哪里？
- 为什么透明需要远到近、Depth Test 开、Depth Write 关？
- Alpha Cutout 为什么能像 Opaque 一样写深度？
- Premultiplied Alpha 为什么使用 `ONE` 作为 Source Factor？
- Skybox 为什么要移除 View Matrix 的平移？
- Instancing 减少的是 Draw Call，为什么不会减少总 Vertex 工作？

下一章会把完整 3D 场景先画进离屏 Texture，再以全屏 Pass 完成灰度、Tone Mapping 和 Bloom。

---

### 14.27 扩展阅读：实例数量继续增长以后

这一节不属于本章要完成的内容，第一遍可以直接跳过。它只是告诉你，当 14.23 的 CPU 逐实例裁剪真的成为瓶颈时，业界往下走的方向是什么。

**按空间格子分 Batch**：把世界切成网格，每个格子一个 Batch。这样相机只需要重建附近几个格子的实例列表，而不是整个世界的。这是最容易做、也最该先做的一步。

**GPU Culling**：把"这个实例可见吗"的判断交给 GPU 自己做。因为实例可能有几万个，而这种判断彼此独立，正好适合并行。

**Indirect Draw**：绘制参数（画多少个实例、从哪里开始）不再由 CPU 写在函数调用里，而是存进一个 GPU Buffer。GPU 读取这个 Buffer 后自己发起绘制。

它和 GPU Culling 正好是一对：GPU 算出可见实例数，直接写进这个 Buffer。

两者都需要 Compute Shader。而 **macOS 的 OpenGL 4.1 没有 Compute Shader**，所以本书的 OpenGL 路线不实现它们。要等第 20 章迁移到 Vulkan 或 Metal 之后才有条件做。

记住这个顺序就够了：

~~~text
CPU 逐实例裁剪（本章做的）
    -> 按空间格子分 Batch（下一步，仍在 CPU）
    -> GPU Culling + Indirect Draw（需要 Compute Shader）
~~~

每一步都应该在 Profile，也就是实际性能测量，证明上一步不够用之后再做。
