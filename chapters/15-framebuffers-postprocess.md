# 第 15 章：离屏 Framebuffer 与后处理

<!-- nav -->
> [上一章：透明、天空与实例化](14-transparency-sky-instancing.md) ｜ [下一章：UI 与图形调试工具](16-ui-debug-tools.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/07_postprocess_ui/15-framebuffers-postprocess.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

前面的场景可以直接画到窗口，但这种方式很难修改“已经完成的整张画面”。灰度、色调映射、Bloom、屏幕扭曲、景深等效果都需要先得到一张场景 Texture，再让后续 Shader 逐 Pixel 读取和处理它。

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 解释为什么“修改整张已完成画面”必须先画进离屏 Texture。
2. 创建带 Color Texture 和 Depth Texture 附件的 FBO，并检查它是否完整。
3. 用一个全屏三角形或四边形把离屏 Texture 原样显示出来。
4. 说明 HDR 为什么需要浮点格式，Tone Mapping 又在解决什么问题。
5. 拆解 Bloom 的四步：Bright Extract、降采样、Blur、合成。
6. 在窗口 Resize 时正确重建全部离屏资源，不泄漏也不用旧尺寸采样。
7. 按“离屏是否画对 -> 是否采样对 -> 效果是否算对”的顺序排查后处理问题。

本章建立完整离屏流程。HDR 是 High Dynamic Range，高动态范围，表示中间画面能保留大于普通显示白色的亮度：

~~~text
3D Scene -> HDR Scene Texture -> Post Process -> Window Backbuffer -> Present
~~~

本章分三部分：先把场景画进 Texture 并原样显示，再加入 HDR、Tone Mapping 和 Bloom，最后处理 Resize、调试和资源释放。

请把本章看成三个连续但可独立停下的项目：

1. **离屏复制**：场景先写入 `sceneColor`，再原样复制到窗口；画面应与改造前一致。
2. **HDR 输出**：`sceneColor` 能保存大于 1 的值，并经过 Exposure、一个 Tone Mapper 和一次 sRGB 编码正确显示。
3. **Bloom 扩展**：在前两步稳定后，再加入 Bright Extract、降采样、Blur 和合成。

第一项是本章最低完成要求，第二项是推荐完成要求，第三项是进阶效果。MSAA 也是可选扩展，不要在离屏复制尚未正确时加入。

## 第一部分：离屏 Target 与全屏处理

### 15.1 默认 Framebuffer 是什么

创建 GLFW 窗口和 OpenGL Context 后，系统提供一个默认 Framebuffer。绑定编号 0 就是在使用它：

~~~cpp
glBindFramebuffer(GL_FRAMEBUFFER, 0);
~~~

它通常包含：

- 用于最终显示的颜色缓冲。
- 用于深度测试的深度缓冲。
- 可能还有模板缓冲和多重采样。

窗口系统常采用 Double Buffering，双缓冲：

~~~text
Front Buffer：显示器当前正在扫描显示
Back Buffer：程序正在绘制下一帧
Swap Buffers：两者交换职责
~~~

这避免用户看见“画到一半”的帧。

默认 Framebuffer 适合最终输出，但它的颜色附件通常不是一个可以随意绑定到 `sampler2D` 的普通 Texture。若后处理 Shader 要采样完整场景，应先画到自己创建的 Texture。

---

### 15.2 离屏渲染是什么意思

Offscreen Rendering，离屏渲染，是把结果写入不直接显示的 GPU 资源。

~~~text
直接渲染：
Scene Draw -> Window Backbuffer -> Present

离屏渲染：
Scene Draw -> sceneColor Texture
sceneColor -> Post Process Shader -> Window Backbuffer -> Present
~~~

“离屏”不表示离开 GPU，也不表示写到磁盘。`sceneColor` 仍位于 GPU 内存，后一个 Pass 直接采样它。

这像先在一张中间画布上完成 3D 场景，再把这张画布经过滤镜复制到最终画布。

---

### 15.3 FBO、Framebuffer、Attachment、Texture、Renderbuffer

这些词必须区分。

#### 15.3.1 Framebuffer

Framebuffer 是“一组当前渲染输出位置”的概念。一次 Draw 的颜色和深度要写到它的附件。

#### 15.3.2 FBO

FBO 是 Framebuffer Object，OpenGL 中用于描述自建 Framebuffer 附件组合的对象。

FBO 自身不保存整张图片，它保存“哪些资源绑定为 Color、Depth 等 Attachment”的关系。

#### 15.3.3 Attachment

Attachment 是挂到 FBO 上的实际存储资源：

~~~text
GL_COLOR_ATTACHMENT0
GL_DEPTH_ATTACHMENT
GL_STENCIL_ATTACHMENT
~~~

#### 15.3.4 Texture

Texture 可以被 Framebuffer 写入，也能在后续 Shader 中通过 Sampler 读取。因此 `sceneColor` 必须使用 Texture。

#### 15.3.5 Renderbuffer

Renderbuffer 也能作为 Attachment，通常只用于渲染写入和固定功能测试，不能像普通 Texture 那样直接采样。

选择规则：

~~~text
后续 Shader 需要采样 -> 使用 Texture
只需要本 Pass 的深度测试，不再读取 -> 可用 Renderbuffer
~~~

为了后续可视化 Scene Depth，本章让 Color 和 Depth 都使用 Texture。

---

### 15.4 Attachment Format：里面到底保存什么

#### 15.4.1 Scene Color

普通 `GL_RGBA8` 每通道约 8 位，通常只能保存 0 到 1 的有限颜色。光照值超过 1 时会被夹住，亮部信息丢失。

本章使用：

~~~text
GL_RGBA16F
~~~

它为每个 RGBA 通道使用 16 位浮点存储，可以保存大于 1 的线性 HDR 值。

#### 15.4.2 Scene Depth

可使用：

~~~text
GL_DEPTH_COMPONENT24
~~~

它保存深度测试值。后续若要做 Depth Debug、景深或深度重建，可以采样它。

#### 15.4.3 Format 不是随便的字符串

Texture 创建常涉及三类参数：

- Internal Format：GPU 实际怎样存，例如 `GL_RGBA16F`。
- External Format：上传数据有哪些通道，例如 `GL_RGBA`。
- Data Type：上传数据每通道是什么类型，例如 `GL_FLOAT`。

本章创建空 Attachment，数据指针传 `nullptr`，但参数仍必须构成合法组合。

---

### 15.5 创建 Scene FBO

~~~cpp
struct SceneTarget {
    GLuint fbo = 0;
    GLuint color = 0;
    GLuint depth = 0;
    int width = 0;
    int height = 0;
};
~~~

创建函数示意：

~~~cpp
SceneTarget createSceneTarget(int width, int height) {
    SceneTarget target{};
    target.width = width;
    target.height = height;

    glGenFramebuffers(1, &target.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);

    glGenTextures(1, &target.color);
    glBindTexture(GL_TEXTURE_2D, target.color);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA16F,
        width,
        height,
        0,
        GL_RGBA,
        GL_FLOAT,
        nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        target.color,
        0
    );

    glGenTextures(1, &target.depth);
    glBindTexture(GL_TEXTURE_2D, target.depth);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_DEPTH_COMPONENT24,
        width,
        height,
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D,
        target.depth,
        0
    );

    GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, drawBuffers);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("Scene framebuffer is incomplete");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return target;
}
~~~

逐段意图：

- 创建 FBO，后续 Attachment 都挂到它上面。
- 创建浮点 Color Texture，供后处理采样。
- `CLAMP_TO_EDGE` 避免全屏滤镜在边缘采到重复内容。
- 创建 Depth Texture，供深度测试和调试采样。
- 明确 Color Shader Output 写往 `GL_COLOR_ATTACHMENT0`。
- 在 FBO 仍绑定时检查完整性。
- 最后恢复默认 Framebuffer，避免后续初始化代码误写 Scene Target。

---

### 15.6 FBO 为什么会不完整

`glCheckFramebufferStatus` 失败表示附件组合不能用于渲染。常见原因：

- 没有合法 Attachment。
- Texture 尺寸为 0。
- Color 与 Depth 尺寸不匹配。
- 多重采样样本数不匹配。
- Internal Format 不支持作为对应 Attachment。
- Draw Buffer 指向不存在的 Color Attachment。

不要忽略完整性错误继续绘制。FBO 不完整时，后续黑屏只是结果，真正原因已经在创建阶段出现。

建议把状态十六进制值和 Attachment 尺寸、格式一起打印，而不是只写“FBO error”。

---

### 15.7 把 3D Scene 画进离屏 Target

Scene Pass 开始：

~~~cpp
glBindFramebuffer(GL_FRAMEBUFFER, sceneTarget.fbo);
glViewport(0, 0, sceneTarget.width, sceneTarget.height);

glEnable(GL_DEPTH_TEST);
glDepthMask(GL_TRUE);
glDisable(GL_BLEND);

glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

renderOpaquePass();
// 只有第 14 章对应部分已经实现时，才调用：
// renderSkyPass();
// renderTransparentPass();
~~~

完成后：

~~~text
sceneTarget.color -> 完整场景的线性 HDR 颜色
sceneTarget.depth -> 主相机深度
~~~

此时还没有把结果呈现到窗口。它只是 GPU 中两张可以继续读取的 Texture。

---

### 15.8 Fullscreen Triangle：让 Shader 对整张图运行

后处理不再需要 3D Mesh。只要产生覆盖整个输出 Viewport 的 Fragment，然后根据 UV 采样 `sceneColor`。

可以使用一个超出 NDC 边界的大 Triangle：

~~~text
NDC 可见范围：[-1,1] x [-1,1]

Triangle 顶点：
(-1,-1), (3,-1), (-1,3)
~~~

它完整覆盖屏幕，没有两个 Triangle 中间的共享对角线。

#### 15.8.1 不使用 Vertex Buffer 的 Shader

~~~glsl
#version 410 core

out vec2 vUv;

void main() {
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 position = positions[gl_VertexID];
    vUv = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
~~~

`gl_VertexID` 在 `glDrawArrays` 的三个 Vertex 中分别为 0、1、2，因此可以直接索引常量数组。

OpenGL Core Profile 即使 Shader 不读取 Attribute，绘制时通常仍需绑定一个合法 VAO。可以创建一个空 VAO 专门用于 Fullscreen Draw。

~~~cpp
glBindVertexArray(fullscreenVao);
glDrawArrays(GL_TRIANGLES, 0, 3);
~~~

---

### 15.9 最小 Post Process：原样显示 Scene Color

先不要做灰度或 Bloom。第一步只证明离屏 Texture 能正确显示。

~~~glsl
#version 410 core

in vec2 vUv;
uniform sampler2D uSceneColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = texture(uSceneColor, vUv).rgb;
    outColor = vec4(color, 1.0);
}
~~~

Post Pass：

~~~cpp
glBindFramebuffer(GL_FRAMEBUFFER, 0);
glViewport(0, 0, windowWidth, windowHeight);

glDisable(GL_DEPTH_TEST);
glDepthMask(GL_FALSE);
glDisable(GL_BLEND);
glDisable(GL_CULL_FACE);

useProgram(copyProgram);
bindTexture(0, sceneTarget.color);
setUniform(copyProgram, "uSceneColor", 0);

glBindVertexArray(fullscreenVao);
glDrawArrays(GL_TRIANGLES, 0, 3);

glDepthMask(GL_TRUE);
~~~

若这一步不能原样显示场景，不要继续加效果。检查 FBO、Viewport、Texture Binding、Sampler Unit、UV 和 Fullscreen VAO。

由于 `sceneColor` 是 HDR，直接写到普通窗口会截断大于 1 的值。为了验证几何链可以暂时接受；下一节加入 Tone Mapping 才得到正确最终画面。

---

### 15.10 不能一边写 Texture，一边采样同一 Texture

假设 `sceneColor` 仍绑定为当前 FBO 的 Color Attachment，同时 Fragment Shader 又把 `sceneColor` 绑定为输入 Sampler。这形成 Feedback Loop：同一次绘制既读又写同一图像区域，结果未定义。

错误思路：

~~~text
sceneColor -> grayscale -> 写回 sceneColor
~~~

正确做法：

~~~text
sceneColor -> grayscale -> postColor
~~~

若有多个连续效果，可使用 Ping-Pong Textures：

~~~text
Pass A：Texture 0 读 -> Texture 1 写
Pass B：Texture 1 读 -> Texture 0 写
Pass C：Texture 0 读 -> Window 写
~~~

“读取资源”和“当前 Attachment”必须是不同存储，除非使用明确支持的高级机制并处理同步；本书不使用这些例外。

---

### 15.11 灰度后处理

简单平均 RGB：

~~~text
gray = (R + G + B) / 3
~~~

会忽略人眼对绿色更敏感、对蓝色较不敏感的事实。常使用 Rec.709 亮度权重：

~~~text
luminance = 0.2126 * R
          + 0.7152 * G
          + 0.0722 * B
~~~

Shader：

~~~glsl
vec3 linearColor = texture(uSceneColor, vUv).rgb;
float luminance = dot(
    linearColor,
    vec3(0.2126, 0.7152, 0.0722)
);
vec3 gray = vec3(luminance);
outColor = vec4(gray, 1.0);
~~~

权重用于线性颜色值。之后仍需 Tone Mapping 与显示编码。若在已经 sRGB 编码的数值上计算，得到的是视觉近似而不是线性亮度。

---

#### 本部分检查点

**能做出来**：场景先画进离屏 Texture，再原样显示到窗口，画面和直接画到窗口时**完全一致**。

**能回答**：

- FBO 完整性检查失败最常见的原因是什么？
- 为什么要用全屏三角形/四边形而不是直接“拷贝”？
- Depth 附件用 Texture 和用 Renderbuffer 有什么区别？

**卡在这里就先不要往下读**：这一步必须做到“看不出区别”。带着色差进入 HDR 部分，后面分不清是色调映射的问题还是这里的问题。

## 第二部分：HDR、Tone Mapping 与 Bloom

### 15.12 HDR：为什么中间颜色需要大于 1

现实中太阳反光可以比阴影亮几百或几千倍。若所有通道只能保存到 1：

~~~text
亮墙 = 1
灯泡 = 5 -> 被截成 1
太阳反光 = 50 -> 也被截成 1
~~~

三者最终都是白，强度差异丢失。Bloom 也无法区分“普通白墙”和“真正高亮光源”。

`GL_RGBA16F` 让 Scene Pass 保存：

~~~text
(0.2, 0.3, 0.4)
(1.0, 0.8, 0.5)
(8.0, 4.0, 1.0)
~~~

这些都是线性 HDR 值。显示器最终仍需要有限范围，因此要 Tone Mapping。

---

### 15.13 Exposure：控制整个场景的曝光

Exposure 类似相机曝光，控制 HDR 光量进入 Tone Mapper 前的整体尺度。

最简单：

~~~text
exposedColor = hdrColor * exposure
~~~

- Exposure 小：画面更暗，保留更多亮部。
- Exposure 大：画面更亮，暗部更可见，但亮部更易压缩。

Exposure 应是可调参数，不要用它掩盖灯光单位完全失控的问题。先让材质与灯光处于合理数量级，再用 Exposure 调整观察范围。

---

### 15.14 Tone Mapping：压缩，不是截断

Tone Mapping 把 `[0, +infinity)` 的 HDR 颜色压缩到适合显示的 `[0,1]`。

#### 15.14.1 Reinhard

最简单公式：

~~~text
mapped = exposed / (exposed + 1)
~~~

逐通道例子：

| HDR 值 | Reinhard 结果 |
| --- | --- |
| 0 | 0 |
| 0.5 | 0.333 |
| 1 | 0.5 |
| 4 | 0.8 |
| 100 | 0.990 |

无论 HDR 多大，结果都逐渐接近 1，而不是突然截平。

~~~glsl
vec3 hdr = texture(uSceneColor, vUv).rgb;
vec3 exposed = hdr * uExposure;
vec3 mapped = exposed / (exposed + vec3(1.0));
outColor = vec4(mapped, 1.0);
~~~

#### 15.14.2 Exponential

另一种简单曲线：

~~~text
mapped = 1 - exp(-hdr * exposure)
~~~

本章任选一种实现。Reinhard 容易手算，适合学习。电影级 Tone Mapper 会更复杂，但解决的是曲线外观，不改变“先 HDR、后压缩、最后显示编码”的主流程。

---

### 15.15 最终颜色空间顺序

把第 11 章和本章连接起来：

~~~text
sRGB Base Color Texture
    |  采样时自动解码
    v
Linear Albedo
    |
    v
Linear Lighting
    |
    v
Linear HDR sceneColor
    |
    v
Bloom 等 Linear Effects
    |
    v
Tone Mapping  得到 0..1 Linear
    |  一次 sRGB 编码
    v
Window Backbuffer
    |
    v
Display
~~~

若默认 Framebuffer 支持 sRGB，可在最终写入时启用：

~~~cpp
glEnable(GL_FRAMEBUFFER_SRGB);
~~~

Tone Mapping Shader 输出线性 `mapped`，OpenGL 编码到 sRGB。

不要同时再写：

~~~glsl
mapped = pow(mapped, vec3(1.0 / 2.2));
~~~

否则很可能编码两次。若平台默认 Framebuffer 不支持 sRGB，才需要由 Shader 明确编码；项目必须选择一种方案并记录。

UI 通常在 Tone Mapping 后绘制。UI 颜色 Texture 采样时从 sRGB 解码，在线性目标上混合，写入 sRGB Backbuffer 时再编码。

---

### 15.16 Bloom 不是“让整张图更亮”

Bloom 模拟非常亮区域在镜头和视觉系统中向周围扩散的光晕。

基本链：

~~~text
HDR sceneColor
    |
    v
Bright Extract    只保留高亮
    |
    v
Downsample        降低分辨率
    |
    v
Blur              扩散亮部
    |
    v
Upsample          放大并组合
    |
    v
与原 HDR Scene 相加
    |
    v
Tone Mapping
~~~

Bloom 应在 Tone Mapping 前处理 HDR 值，因为大于 1 的强度正是区分高亮区域的重要信息。

---

### 15.17 第一步：Bright Extract

计算线性亮度：

~~~glsl
vec3 hdr = texture(uSceneColor, vUv).rgb;
float brightness = dot(hdr, vec3(0.2126, 0.7152, 0.0722));

vec3 bright = brightness > uThreshold
    ? hdr
    : vec3(0.0);

outColor = vec4(bright, 1.0);
~~~

`uThreshold` 例如设为 1，表示只有 Tone Mapping 前亮度大于普通显示白的区域产生 Bloom。

硬阈值可能形成突变。后续可使用 Soft Knee，让阈值附近逐渐进入 Bloom；第一版先使用硬阈值，方便检查。

若 Bright Texture 几乎全黑：

- 场景可能没有大于阈值的 HDR 值。
- 灯光强度太低。
- Scene Color 实际使用了 RGBA8，已被夹到 1。

若几乎全白，阈值可能太低或曝光/灯光值失控。

---

### 15.18 第二步：Downsample

把 Bright Texture 缩小，例如：

~~~text
1920 x 1080
-> 960 x 540
-> 480 x 270
~~~

好处：

- 后续 Blur 处理的 Pixel 更少。
- 相同采样半径对应到原屏幕时扩散范围更大。
- 多层尺寸可以形成不同尺度的光晕。

最简单 Downsample 可让目标 Viewport 变小，用 Linear Filter 采样上一级 Texture。更高质量方案会对覆盖区域做多点过滤，避免闪烁；本章先用双线性采样理解资源链。

每一级必须有独立 Texture 和 FBO Attachment，尺寸明确记录。

---

### 15.19 第三步：Separable Gaussian Blur

二维模糊若直接采样 `N x N` 邻域，5 x 5 需要 25 次采样。Gaussian Kernel 可以分离成：

~~~text
先水平 1 x 5
再垂直 5 x 1
~~~

总共约 10 次采样，结果近似相同。

一个五点一维权重例子：

~~~text
offset 0: 0.227027
offset 1: 0.316216
offset 2: 0.070270
~~~

中心一次，两侧各一次，所有权重和约等于 1。

~~~glsl
vec3 blur1D(sampler2D image, vec2 uv, vec2 direction) {
    vec2 texel = 1.0 / vec2(textureSize(image, 0));

    vec3 result = texture(image, uv).rgb * 0.227027;
    result += texture(image, uv + direction * texel * 1.0).rgb * 0.316216;
    result += texture(image, uv - direction * texel * 1.0).rgb * 0.316216;
    result += texture(image, uv + direction * texel * 2.0).rgb * 0.070270;
    result += texture(image, uv - direction * texel * 2.0).rgb * 0.070270;
    return result;
}
~~~

调用：

~~~text
水平 Pass：direction = (1, 0)
垂直 Pass：direction = (0, 1)
~~~

水平 Pass 读 Texture A、写 Texture B；垂直 Pass 读 B、写 A。这就是 Ping-Pong。

---

### 15.20 第四步：Upsample 与合成

模糊后的低分辨率 Bloom 要放大到场景尺寸。使用 Linear Filter 采样能得到平滑结果。

最简单合成：

~~~glsl
vec3 scene = texture(uSceneColor, vUv).rgb;
vec3 bloom = texture(uBloomTexture, vUv).rgb;

vec3 hdrCombined = scene + bloom * uBloomStrength;
vec3 exposed = hdrCombined * uExposure;
vec3 mapped = exposed / (exposed + vec3(1.0));

outColor = vec4(mapped, 1.0);
~~~

正确顺序：

~~~text
HDR Scene + HDR Bloom
-> Exposure
-> Tone Mapping
-> sRGB Encode
~~~

不要先 Tone Map 再提取 Bloom，否则高亮差异已被压缩。

多级 Bloom 会从多个 Mip 尺寸逐级上采样相加。这里的 Mip 是同一效果链中逐级缩小的分辨率层，概念类似 Texture Mipmap；多个尺寸能得到大、小不同范围的光晕。两级实现足以作为本章练习。

---

#### 本部分检查点

**能做出来**：高亮区域出现柔和光晕，普通白墙不发光；关掉 Bloom 后画面回到原样。

**能回答**：

- HDR 为什么需要浮点格式？
- Tone Mapping 在解决什么问题？
- Bloom 的四步各输入什么、输出什么？

**卡在这里就先不要往下读**：整张图都在发光，说明 Bright Extract 的阈值或颜色空间不对，而不是 Blur 半径不对。

## 第三部分：资源组织、Resize 与验证

### 15.21 后处理链怎样组织资源

一个清晰的最小资源图：

~~~text
Scene FBO
  writes sceneColor RGBA16F
  writes sceneDepth DEPTH24

Bright Pass
  reads sceneColor
  writes bloomHalfA

Horizontal Blur
  reads bloomHalfA
  writes bloomHalfB

Vertical Blur
  reads bloomHalfB
  writes bloomHalfA

Final Composite
  reads sceneColor
  reads bloomHalfA
  writes window backbuffer
~~~

对每个 Pass 都能回答“读谁、写谁”，便不会意外产生 Feedback Loop。

建议给 Texture 和 Pass 使用可读名字，并在 GPU Debugger 中打标记，而不是只看到 `texture 17`、`FBO 5`。

---

### 15.22 Resize 是 GPU 资源重建事件

窗口从 1280 x 720 变成 1920 x 1080 后：

- Window Backbuffer 尺寸变了。
- Scene Color/Depth 仍是旧尺寸。
- Bloom 各级尺寸也仍是旧尺寸。

只调用 `glViewport` 不会自动改变 Texture Storage。必须重建所有依赖分辨率的 Attachment。

~~~cpp
void resizeRenderer(int width, int height) {
    if (width <= 0 || height <= 0) {
        return; // 窗口最小化时可能得到 0 尺寸
    }

    destroySceneTarget(sceneTarget);
    sceneTarget = createSceneTarget(width, height);

    // 只有实现 Bloom 后才重建它的分辨率相关资源：
    // destroyBloomTargets(bloomTargets);
    // bloomTargets = createBloomTargets(width, height);

    camera.setAspect(
        static_cast<float>(width) / static_cast<float>(height)
    );
}
~~~

单线程 OpenGL 学习程序中，按顺序删除并重建即可；驱动会管理已经提交命令对旧对象的使用。多帧并行或显式 API 中才需要更明确的 GPU 生命周期同步。

Resize 后应同时更新：

~~~text
Scene Target 尺寸
Bloom 各级尺寸
Camera Aspect / Projection
各 Pass Viewport
任何使用 1/width、1/height 的参数
~~~

---

### 15.23 可选：MSAA Target 与 Resolve

MSAA 是 Multisample Anti-Aliasing，用一个 Pixel 内多个覆盖采样减少几何边缘锯齿。

若 Scene FBO 使用 Multisample Texture/Renderbuffer，普通 `sampler2D` 不能直接当作单采样图读取。通常先 Resolve：

~~~text
Multisampled Scene FBO
    -> glBlitFramebuffer Resolve
Single-Sample sceneColor Texture
    -> Post Process
~~~

Resolve 把多个 Sample 合成为一个 Pixel 值。

第一版可以不启用 MSAA，先确保离屏和后处理正确。加入 MSAA 后，Color 与 Depth Attachment 的样本数必须一致，否则 FBO 不完整。

---

### 15.24 中间 Texture 的调试显示

后处理最有效的工具是让任意 Texture 全屏显示或显示在小窗口中。

至少支持：

- `sceneColor`。
- `sceneDepth`。
- `brightExtract`。
- `blurHorizontal`。
- `blurVertical`。
- `finalComposite`。

#### 15.24.1 HDR 显示

直接显示 HDR Texture 会把大于 1 的部分截白。调试时可选择：

- 使用与最终相同 Tone Mapper。
- 乘以一个很小的可调缩放。
- 显示某个通道或 Log Luminance，也就是先对亮度取对数，把跨度很大的 HDR 范围压到较容易观察的范围。

#### 15.24.2 Depth 显示

Perspective Depth 通常是非线性的，大部分值接近 1。直接显示可能几乎全白。可以先做范围拉伸或线性化；至少不要因此误判 Depth Texture 没内容。

#### 15.24.3 UV 显示

~~~glsl
outColor = vec4(vUv, 0.0, 1.0);
~~~

屏幕左下应接近黑，右侧增加红，上侧增加绿。若翻转或扭曲，先修复 Fullscreen Triangle 与 Texture 原点约定。

---

### 15.25 资源拥有与销毁

每个 OpenGL Handle 都要有明确拥有者。

~~~cpp
void destroySceneTarget(SceneTarget& target) {
    if (target.depth != 0) {
        glDeleteTextures(1, &target.depth);
    }
    if (target.color != 0) {
        glDeleteTextures(1, &target.color);
    }
    if (target.fbo != 0) {
        glDeleteFramebuffers(1, &target.fbo);
    }
    target = {};
}
~~~

反复 Resize 时若只创建新 Texture 而不删除旧资源，GPU 内存会持续增长。若先删除 Texture、后续 Pass 却仍保存旧 Handle，又会采样无效资源。

可以把 Target 封装成不可复制、可移动的 RAII 类型。RAII 是 C++ 的“资源获取即初始化”习惯：对象构造时取得资源，析构时自动释放。初学阶段至少保证创建与销毁函数成对，并将已删除 Handle 归零。

---

### 15.26 常见错误

| 现象 | 常见原因 | 首先检查 |
| --- | --- | --- |
| 离屏后完全黑屏 | FBO 不完整、Fullscreen Texture 未绑定 | 记录 status，直接 Copy Pass |
| 场景只显示一部分 | Viewport 仍是 Bloom/Shadow 尺寸 | 每 Pass 显式设置 Viewport |
| 画面上下颠倒 | Texture 与屏幕 UV 原点约定不同 | 显示 UV 渐变 |
| 画面拉伸 | Scene Texture 未随 Resize 重建 | 打印 Window/Target 尺寸 |
| 后处理随机闪烁 | 同一 Texture 同时读写 | 画出 Pass Resource 图 |
| HDR 亮处全部一样白 | Scene Color 实际是 RGBA8 | 查询 Texture Internal Format |
| 整体过亮 | Tone Mapping 后又做两次 sRGB 编码 | 关闭手写 Gamma 对比 |
| 整体过暗 | 忘记最终 sRGB 编码 | 检查默认 Framebuffer 能力 |
| Bloom 全屏泛光 | Threshold 太低或线性/HDR 链错误 | 单独显示 Bright Texture |
| Bloom 没有效果 | 没有大于 Threshold 的 HDR 值 | 显示最大 Luminance |
| Blur 方向怪异 | `texelSize` 或水平/垂直方向错 | 单独显示每次 Blur |
| FBO Resize 后不完整 | 某附件仍旧尺寸或 0 尺寸 | 打印所有 Attachment 尺寸 |
| UI 颜色被压暗 | UI 在 Tone Mapping 前绘制 | 检查 Pass 顺序 |

---

### 15.27 推荐实现顺序

每一步成功后再进入下一步：

**里程碑一：离屏后画面保持不变**

1. 创建 `sceneColor` 与 `sceneDepth`，检查 FBO Complete。
2. 把 3D Scene 画进 Scene FBO。
3. 用 Copy Shader 原样显示 `sceneColor`。
4. 加 UV Debug 和中间 Texture 预览。
5. 实现 Gray Shader。

到这里先停下来验证：关闭 Gray 后，离屏版本应与原先直接画到窗口的版本相同。Resize 也必须正确，才能进入下一阶段。

**里程碑二：建立正确 HDR 显示链**

6. 将 Scene Color 改为 `RGBA16F`，确认高亮值可大于 1。
7. 实现 Exposure + Reinhard Tone Mapping。

此时已经有完整可用的 HDR 后处理路径。下面的 Bloom 是独立增强项。

**里程碑三：可选 Bloom**

8. 实现 Bright Extract，并单独预览。
9. 实现半分辨率水平/垂直 Blur。
10. 合成 Bloom，再 Tone Map。
11. 实现 Resize 时完整重建资源。

这个顺序保证每一步只增加一个主要变量。不要第一次就复制一大段 Bloom 代码，然后面对六张全黑 Texture。

---

### 15.28 练习

#### 练习 A：FBO 结构图

画出 `sceneFbo`、`sceneColor`、`sceneDepth` 的关系。说明删除 FBO 后 Texture 是否一定同时被删除，以及为什么资源拥有者必须自己处理。

#### 练习 B：Copy 与 Gray

实现运行时切换：

~~~text
0 -> Copy
1 -> Gray
2 -> 显示 UV
3 -> 显示 Depth
~~~

证明 Fullscreen Pass 的输入与坐标正确。

#### 练习 C：Tone Mapping 数字表

手算 HDR 值 `0.1、0.5、1、2、10、100` 经过 Reinhard 后的结果，再与 Shader 截图中的亮度趋势对照。

#### 练习 D：故意出错实验（制造 Feedback Loop）

先在纸上说明“读 sceneColor、同时写 sceneColor”为什么有依赖冲突。不要在正式路径保留未定义行为；改用第二张 Texture 完成同一效果。

#### 练习 E：两级 Bloom

实现半分辨率与四分之一分辨率两级 Bloom。允许分别显示 Bright、每级 Blur 和最终合成。

#### 练习 F：Resize 压力测试

连续拖动窗口大小、最大化、最小化、恢复。记录：

- FBO 是否始终 Complete。
- Texture 是否更新到正确尺寸。
- GPU 内存是否持续增长。
- 画面是否拉伸或使用旧内容。

---

### 15.29 本章验收标准
最低验收，也就是先获得一条可靠的离屏路径：

1. Scene FBO 包含完整、同尺寸的 Color 与 Depth Attachment。
2. 3D Scene 能先画到 `sceneColor`，再用 Fullscreen Triangle 原样显示。
3. 后处理 Pass 不发生同一 Texture 同时读写的 Feedback Loop。
4. Resize 后所有分辨率相关 Texture、Viewport 和 Camera Aspect 都更新。
5. FBO 与 Texture 有成对销毁逻辑，不会因 Resize 持续泄漏。
6. 你能对 Copy Pass 明确写出 Input 与 Output。

灰度是第一个可选滤镜练习：实现时使用明确亮度权重，并在线性颜色上计算；它不阻塞离屏复制的最低验收。

推荐验收，也就是加入 HDR 显示链：

1. Scene Color 使用浮点格式，并能保存大于 1 的 HDR 数值。
2. Exposure、Tone Mapping 和 sRGB 编码顺序正确，编码只发生一次。
3. 能单独显示 Tone Mapping 前后的结果并解释差异。

Bloom 扩展验收：

1. Bright Extract、Blur 和 Composite 可以分别显示。
2. 每一步读取和写入不同 Texture，没有 Feedback Loop。
3. Bloom Target 会随窗口正确重建，且不持续泄漏资源。

进入下一章前，请确认自己能回答：

- FBO 本身是不是保存像素的地方？
- Texture 与 Renderbuffer 的选择依据是什么？
- 为什么后处理通常使用 Fullscreen Triangle？
- 为什么同一 Texture 不能在一次 Pass 中既作为输入又作为输出？
- 为什么只改 Viewport 不能完成 Resize？

完成 HDR/Bloom 扩展后再回答：

- HDR 为什么必须在 Tone Mapping 前保留大于 1 的值？
- Bloom 的阈值、降采样、模糊和合成分别解决什么问题？

下一章会为这些中间资源和 Pass 加入可交互 UI、图形调试标记、平台可用的 GPU 截帧方法和错误检查，使渲染器从“能工作”变成“能够被观察和解释”。
