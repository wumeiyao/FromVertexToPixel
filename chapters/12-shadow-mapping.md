# 第 12 章：方向光阴影贴图

<!-- nav -->
> [上一章：颜色空间、材质与基础光照](11-color-material-lighting.md) ｜ [下一章：Render Pass 与渲染队列](13-render-passes-and-queues.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/05_lighting_shadows/12-shadow-mapping.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

上一章的 Lambert 光照只回答：“表面是否朝向光源？”但一个表面即使正对太阳，也可能被墙挡住。阴影要回答第二个问题：“从光源到这个表面的路径上，是否先遇到了别的物体？”

本章实现最经典的 Shadow Mapping，阴影贴图。它的核心不是把一张黑色图片贴到地面，而是先从光源视角记录最近深度，再从玩家相机渲染时查询遮挡关系。

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 解释 Shadow Caster、Shadow Receiver 和 Shadow Map。
2. 说清楚为什么阴影至少需要两个 Render Pass。
3. 构造方向光的 View、Orthographic Projection 和 Depth Texture。
4. 把 World Position 转成 Shadow UV 与 Light Depth。
5. 理解 Shadow Acne、Bias、Peter-Panning 和 PCF。
6. 通过显示深度图、光源视锥和单项数值定位阴影错误。

本章分三部分：先把阴影视为“光源可见性”，再完成写深度和查深度两个 Pass，最后处理误差、质量与调试。

第一遍只实现固定方向光、固定正交范围和硬阴影：Shadow Map 能单独显示，Main Pass 能完成一次深度比较。Bias、PCF、动态跟随和 Texel Snapping 都应在硬阴影正确之后逐项加入。

## 第一部分：阴影问题与所需数据

### 12.1 用“光源能否看见”理解阴影

想象把一台只记录远近的相机放在光源处。

~~~text
光源相机看见：

光源 ----> 箱子 ----> 地面上的点 P
             ^
             先被看见
~~~

沿着某个方向，光源首先看见箱子。地面点 `P` 在同一方向上却更远，因此光源看不见 `P`，光线也就到不了 `P`。这时 `P` 位于阴影中。

Shadow Mapping 把这个过程分成两次绘制：

~~~text
Pass 1：从光源看场景，保存每个方向上最近表面的深度
Pass 2：从主相机看场景，把当前表面深度与光源记录的最近深度比较
~~~

---

### 12.2 先定义本章术语

#### 12.2.1 Shadow Caster：投射阴影的物体

Caster 是会挡住光的物体，例如箱子、角色、墙。它需要在 Shadow Pass 中被绘制到深度图。

#### 12.2.2 Shadow Receiver：接收阴影的表面

Receiver 是会在主光照中查询 Shadow Map 的表面，例如地面或另一个模型。

同一个物体可以既是 Caster 又是 Receiver。角色可以把阴影投到地面，也可以让自己的手臂在身体上投下阴影。

#### 12.2.3 Depth：深度

Depth 是经过投影后，用于比较远近的数值。Shadow Map 保存的是从光源视角得到的 Depth，不是从主相机得到的 Depth。

#### 12.2.4 Shadow Map：阴影贴图

Shadow Map 是一张 Depth Texture。每个 Texel 保存光源沿对应方向看见的最近深度。

它没有 RGB 颜色。调试时把它显示成灰度，只是为了让人眼能观察深度数值。

#### 12.2.5 Light Space

Light Space 指以光源“相机”为观察者的空间与投影过程。一个 World Position 可以同时有：

~~~text
Camera Clip Position = cameraProjection * cameraView * worldPosition
Light Clip Position  = lightProjection  * lightView  * worldPosition
~~~

前者决定它画到屏幕哪里；后者决定它查 Shadow Map 哪里。

---

### 12.3 为什么方向光使用正交投影

方向光模拟非常远的太阳。场景不同位置收到的光线方向近似平行：

~~~text
\  \  \  \  \   平行光线
 \  \  \  \  \
~~~

透视投影会产生近大远小，适合有限位置的 Camera 或 Spot Light。方向光没有一个需要表现透视缩小的近距离位置，所以通常使用 Orthographic Projection，正交投影。

正交投影由一个盒状区域定义：

~~~text
left, right
bottom, top
near, far
~~~

只有这个盒子里的 Caster 会写入 Shadow Map，也只有落在该范围内的 Receiver 能得到有效阴影。

#### 12.3.1 方向光仍然需要一个虚拟位置

数学上的方向光只有方向，没有位置。但 `lookAt` View Matrix 需要 eye 和 target。可以围绕需要覆盖的场景区域创建一个虚拟光源相机：

~~~text
center = 阴影覆盖区域中心
L = 从表面指向光源的单位方向
lightEye = center + L * distance
lightTarget = center
~~~

`distance` 只要让场景落在 Light View 的 near/far 范围内即可。真正决定方向的是 `lightEye -> lightTarget`。

---

### 12.4 两个 Pass 的完整数据依赖

~~~text
场景中的 Shadow Caster
        |
        v
  Shadow Pass                （使用 Light ViewProjection）
        |
        v
Shadow Depth Texture ────────┐
                             |
场景中的可见表面              |  作为输入 Texture
        |                    |
        v                    v
    Main Pass  <─────────────┘（使用 Camera ViewProjection）
        |
        v
  比较 Light Depth
        |
        v
  直接光乘可见度
        |
        v
    场景颜色
~~~

Shadow Pass 的输出是 Main Pass 的输入。因此 Shadow Pass 必须先完成。

Shadow Map 通常每帧更新，因为 Caster 或光源可能移动。若场景和灯都静止，可以缓存，但第一版先每帧更新，避免生命周期与失效判断干扰理解。

---

#### 本部分检查点

**能做出来**：画出两个 Pass 的数据依赖图：谁先写什么、谁后读什么、中间那张 Texture 叫什么。

**能回答**：

- 为什么阴影至少需要两个 Pass？
- 方向光为什么用正交投影而不是透视？
- Shadow Caster 和 Shadow Receiver 有什么区别？

**卡在这里就先不要往下读**：依赖关系画不出来，代码写出来也会是“改一处坏三处”。

## 第二部分：写入并查询 Shadow Map

### 12.5 创建 Shadow Depth Texture 与 FBO

本章需要一个只保存深度的离屏 Framebuffer。

FBO 是 Framebuffer Object，OpenGL 中用于把 Texture 或 Renderbuffer 组织成一个渲染目标的对象。本章把一张 Depth Texture 挂到 FBO 的 Depth Attachment 上。

~~~cpp
constexpr int shadowSize = 2048;

GLuint shadowDepth = 0;
glGenTextures(1, &shadowDepth);
glBindTexture(GL_TEXTURE_2D, shadowDepth);

glTexImage2D(
    GL_TEXTURE_2D,
    0,
    GL_DEPTH_COMPONENT24,
    shadowSize,
    shadowSize,
    0,
    GL_DEPTH_COMPONENT,
    GL_FLOAT,
    nullptr
);

glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

const float borderDepth[] = {1.0f, 1.0f, 1.0f, 1.0f};
glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderDepth);

GLuint shadowFbo = 0;
glGenFramebuffers(1, &shadowFbo);
glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
glFramebufferTexture2D(
    GL_FRAMEBUFFER,
    GL_DEPTH_ATTACHMENT,
    GL_TEXTURE_2D,
    shadowDepth,
    0
);

glDrawBuffer(GL_NONE);
glReadBuffer(GL_NONE);

assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)
       == GL_FRAMEBUFFER_COMPLETE);

glBindFramebuffer(GL_FRAMEBUFFER, 0);
~~~

逐项解释：

- `GL_DEPTH_COMPONENT24` 请求常见的 24 位深度存储。
- 像素数据传 `nullptr`，因为 Texture 内容会由 Shadow Pass 写入。
- 第一版用 `GL_NEAREST`，让每次采样读取明确的一个深度值。
- Border Depth 设为 1，表示超出边界处视为最远，不轻易产生黑边。
- FBO 没有 Color Attachment，所以 Draw/Read Buffer 设为 `GL_NONE`。
- 完整性检查必须通过，否则这个 Pass 没有合法输出目标。

这里使用“手动比较”方式：普通 `sampler2D` 读出深度，再在 Shader 中比较。OpenGL 也支持 `sampler2DShadow` 自动比较，但初学阶段手动比较更容易观察每个值。

---

### 12.6 构造 Light ViewProjection

伪代码：

~~~cpp
Vec3 regionCenter{0.0f, 0.0f, 0.0f};
Vec3 directionToLight = normalize(Vec3{0.5f, 1.0f, 0.3f});

Vec3 lightEye = regionCenter + directionToLight * 30.0f;
Mat4 lightView = lookAt(lightEye, regionCenter, Vec3{0, 1, 0});

Mat4 lightProjection = orthographic(
    -20.0f, 20.0f,   // left, right
    -20.0f, 20.0f,   // bottom, top
     0.1f, 60.0f     // near, far
);

Mat4 lightViewProjection = lightProjection * lightView;
~~~

数值不是通用答案。它们定义 Shadow Map 覆盖的世界区域：

- 区域太小：边缘对象没有阴影。
- 区域太大：每个 Shadow Texel 覆盖更大世界面积，阴影变粗糙。
- near/far 太宽：深度精度利用不充分。

第一版使用固定、容易观察的区域。之后再根据 Camera 动态拟合。

若 `directionToLight` 几乎与 World Up 平行，`lookAt` 的 up 向量可能退化。此时选择另一根不平行的 up，例如 `(0,0,1)`。

---

### 12.7 Shadow Pass：只写光源深度

Shadow Pass 开始时必须设置自己的目标、Viewport 和状态：

~~~cpp
glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
glViewport(0, 0, shadowSize, shadowSize);
glClear(GL_DEPTH_BUFFER_BIT);

glEnable(GL_DEPTH_TEST);
glDepthMask(GL_TRUE);
glDisable(GL_BLEND);
glEnable(GL_CULL_FACE);
glCullFace(GL_BACK);

useProgram(shadowProgram);
setUniform(shadowProgram, "uLightViewProjection", lightViewProjection);

for (const RenderItem& item : shadowCasters) {
    setUniform(shadowProgram, "uModel", item.modelMatrix);
    bindMesh(item.mesh);
    drawMesh(item.mesh);
}
~~~

#### 12.7.1 Shadow Vertex Shader

~~~glsl
#version 410 core

layout(location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uLightViewProjection;

void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    gl_Position = uLightViewProjection * worldPosition;
}
~~~

这里只需要 position，因为输出目标只关心深度。

#### 12.7.2 Shadow Fragment Shader

~~~glsl
#version 410 core

void main() {
    // 不输出颜色；固定功能深度阶段会写入 Depth Attachment。
}
~~~

普通不透明 Mesh 不需要颜色、灯光或 Base Color Texture。Alpha Cutout 材质例外：叶片透明区域也要在 Shadow Fragment Shader 中采样 Alpha 并 `discard`，否则整张四边形都会投影。本章先使用完全不透明模型。

---

### 12.8 Main Pass：同时准备 Camera 与 Light 坐标

主 Vertex Shader 既要输出屏幕位置，也要把同一个 World Position 投影到 Light Clip Space：

~~~glsl
#version 410 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightViewProjection;
uniform mat3 uNormalMatrix;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec2 vUv;
out vec4 vLightClipPosition;

void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);

    vWorldPosition = worldPosition.xyz;
    vWorldNormal = uNormalMatrix * aNormal;
    vUv = aUv;
    vLightClipPosition = uLightViewProjection * worldPosition;

    gl_Position = uProjection * uView * worldPosition;
}
~~~

注意 `vLightClipPosition` 仍是齐次 Clip 坐标，还没有除以 `w`，也还不是 Texture UV。

---

### 12.9 从 Light Clip Position 得到 Shadow UV

Fragment Shader 中依次做三步。

#### 第一步：透视除法

~~~glsl
vec3 lightNdc = vLightClipPosition.xyz / vLightClipPosition.w;
~~~

对正交投影，`w` 通常不会产生透视变化，但仍保持统一流程。

#### 第二步：把 NDC 从 -1..1 映射到 0..1

NDC 是 Normalized Device Coordinates，标准化设备坐标，是透视除法后、Viewport 映射前的统一坐标。OpenGL NDC 范围是：

~~~text
x, y, z 都位于 [-1, 1]
~~~

Texture UV 和 Depth Texture 数值使用 `[0,1]`，因此：

~~~glsl
vec3 shadowCoord = lightNdc * 0.5 + 0.5;
~~~

此时：

~~~text
shadowCoord.xy -> Shadow Map UV
shadowCoord.z  -> 当前 Fragment 从光源看见的深度
~~~

#### 第三步：检查有效范围

~~~glsl
bool outside = shadowCoord.x < 0.0 || shadowCoord.x > 1.0
            || shadowCoord.y < 0.0 || shadowCoord.y > 1.0
            || shadowCoord.z < 0.0 || shadowCoord.z > 1.0;
~~~

超出光源投影范围时，本章把它视为“不受这张 Shadow Map 阴影影响”。不要用越界 UV 得到偶然结果。

---

### 12.10 深度比较为什么能判断遮挡

从 Shadow Map 取出光源当时记录的最近深度：

~~~glsl
float closestDepth = texture(uShadowMap, shadowCoord.xy).r;
float currentDepth = shadowCoord.z;
~~~

比较：

~~~text
currentDepth <= closestDepth
    当前表面就是光源看到的最近表面，光可到达

currentDepth > closestDepth
    同一方向已有更近表面，当前表面被挡住
~~~

#### 12.10.1 数字例子

Shadow Map 某 Texel 保存：

~~~text
closestDepth = 0.35
~~~

主相机当前 Fragment 投影到同一 UV，得到：

~~~text
currentDepth = 0.62
~~~

`0.62 > 0.35`，说明光源沿该方向先在深度 0.35 遇到一个物体，深度 0.62 的当前 Fragment 位于其后，因此处于阴影。

理想比较可以写成：

~~~glsl
float shadow = currentDepth > closestDepth ? 1.0 : 0.0;
~~~

其中约定：

~~~text
shadow = 0 -> 被光照到
shadow = 1 -> 在阴影中
~~~

但直接比较会产生精度问题，需要 Bias。

---

#### 本部分检查点

**能做出来**：把 Shadow Map 当灰度图显示出来，能看到场景从光源视角的深度轮廓。

**能回答**：

- World Position 怎样变成 Shadow UV？
- 深度比较为什么能判断遮挡？
- 采样到 Shadow Map 之外时应该算有阴影还是没阴影？

**卡在这里就先不要往下读**：先确认深度图本身是对的，再去调 Bias。顺序反了会把两类问题混在一起。

## 第三部分：误差、质量与调试

### 12.11 Shadow Acne：表面为什么会遮挡自己

Shadow Map 是离散 Texel，且深度只有有限精度。Shadow Pass 写入某三角形时，一个 Texel 代表附近一小片区域；Main Pass 再投影回来时，插值和浮点误差会让 `currentDepth` 略大于原值。

结果是表面被错误判定为位于自己后面，出现密集条纹或斑点，称为 Shadow Acne。

~~~text
理想：currentDepth = 0.500000
记录：closestDepth = 0.500000

实际误差：currentDepth = 0.500012
比较结果：0.500012 > 0.500000 -> 错误判为阴影
~~~

---

### 12.12 Bias：给比较留一点误差空间

加入 Bias 后：

~~~text
currentDepth - bias > closestDepth -> 阴影
~~~

最简单是常量：

~~~glsl
float bias = 0.001;
~~~

但表面与光线夹角越倾斜，误差通常越明显，因此常使用与斜率相关的 Bias：

~~~glsl
float ndotl = max(dot(normalize(N), normalize(L)), 0.0);
float bias = max(0.005 * (1.0 - ndotl), 0.0005);
~~~

解释：

- 表面正对光时 `ndotl` 接近 1，使用较小 Bias。
- 表面斜对光时 `ndotl` 变小，Bias 增大。
- `0.0005` 是最小 Bias。

这些数值依赖 Light Projection 范围、深度格式和场景尺度，不是通用魔法常数。

#### 12.12.1 Peter-Panning

Bias 太大时，物体接触处的阴影会离开物体，像物体悬浮在地面上。这称为 Peter-Panning。

~~~text
Bias 太小 -> Shadow Acne 条纹
Bias 太大 -> 阴影与物体分离
~~~

调 Bias 前必须先确认 Light Matrix、Shadow UV 和 Depth Texture 正确。Bias 只能处理小精度误差，不能修复坐标变换错误。

---

### 12.13 PCF：用邻近多次比较柔化边缘

Shadow Map 分辨率有限。一次 Texel 比较会形成明显锯齿边缘。

PCF 是 Percentage-Closer Filtering。它不直接模糊深度，而是在附近多个 Texel 分别做深度比较，再平均“处于阴影”的比例。

3 x 3 PCF：

~~~glsl
float sampleShadow(vec3 shadowCoord, vec3 N, vec3 L) {
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0 || shadowCoord.z > 1.0) {
        return 0.0;
    }

    float ndotl = max(dot(normalize(N), normalize(L)), 0.0);
    float bias = max(0.005 * (1.0 - ndotl), 0.0005);

    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    float shadow = 0.0;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(x, y) * texelSize;
            float closest = texture(uShadowMap, shadowCoord.xy + offset).r;
            shadow += shadowCoord.z - bias > closest ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
}
~~~

`texelSize` 是一个 Texel 在 UV 中的尺寸。例如 2048 宽的 Texture，每个 Texel 横向约占 `1/2048`。

返回值可能是 0、1/9、2/9……1。边缘因此呈现过渡。

PCF 是多次采样，质量越高成本越大。第一版 3 x 3 足够理解原理。

---

### 12.14 把阴影应用到光照

阴影应减少“直接来自这盏灯”的部分，而不一定抹掉 Ambient：

~~~text
final = ambient + (1 - shadow) * (diffuse + specular)
~~~

对应 GLSL：

~~~glsl
vec3 litColor = ambient
              + (1.0 - shadow) * (diffuse + specular);
~~~

若在阴影中 `shadow = 1`：

~~~text
final = ambient
~~~

若完全受光 `shadow = 0`：

~~~text
final = ambient + diffuse + specular
~~~

这比把最终颜色直接乘 0 更符合“只有直接光被挡住”的直觉。

---

### 12.15 分辨率、覆盖范围与精度的交换

假设 Shadow Map 是 2048 x 2048。

- 覆盖 20 x 20 世界单位时，一个 Texel 约覆盖 `20/2048` 单位。
- 覆盖 200 x 200 世界单位时，一个 Texel 约覆盖 `200/2048` 单位。

Texture 分辨率没变，覆盖范围放大十倍，每个 Texel 在世界中也粗十倍，阴影边缘自然更糊。

改善方向包括：

- 缩紧 Orthographic Bounds。
- 提高 Shadow Map 分辨率。
- 只覆盖 Camera 真正需要的区域。
- 使用 Cascaded Shadow Maps，级联阴影贴图：把 Camera 可见距离切成近、中、远几段，每段使用独立 Shadow Map，让近处获得更密集的 Texel。

提高分辨率会增加 GPU 内存、Shadow Pass 填充和采样成本。不要把 16384 当成修复所有问题的按钮。

---

### 12.16 阴影抖动与 Texel Snapping

若 Light Projection 每帧紧跟 Camera 轻微移动，世界点会不断跨越 Shadow Texel 边界，阴影可能在静止物体上轻微游动，称为 shimmering。

一种稳定方法是让 Light Projection 的中心对齐到 Shadow Texel 对应的世界网格，称为 Texel Snapping。

基本思想：

~~~text
worldUnitsPerTexel = projectionWidth / shadowResolution
lightSpaceCenter = round(lightSpaceCenter / worldUnitsPerTexel)
                 * worldUnitsPerTexel
~~~

第一版可以先固定 Light Projection。只有固定版本正确后，再让它跟随 Camera 并加入稳定处理。

---

### 12.17 调试阴影的正确顺序

阴影同时涉及两台 Camera、两个 Pass 和一张中间 Texture。必须逐层观察。

#### 第 1 步：直接显示 Shadow Map

把深度采样显示为灰度：

~~~glsl
float depth = texture(uShadowMap, uv).r;
outColor = vec4(vec3(depth), 1.0);
~~~

透视深度常集中在接近 1 的区域，显示时可用 `pow` 或范围重映射增强对比；方向光正交深度通常更容易观察。

你应能在灰度图中认出 Caster 轮廓。若整张为白，Shadow Pass 很可能没有写入。

#### 第 2 步：绘制 Light Frustum

从调试 Camera 观察光源正交盒子，确认场景位于其中。

#### 第 3 步：显示 Shadow UV

~~~glsl
outColor = vec4(shadowCoord.xy, 0.0, 1.0);
~~~

观察 UV 是否平滑，并检查是否大面积越界。

#### 第 4 步：显示 currentDepth 与 closestDepth

分别输出两个值，或输出它们的差：

~~~glsl
float difference = shadowCoord.z - closestDepth;
~~~

先证明比较方向正确，再加入 Bias。

#### 第 5 步：只显示二值 Shadow

暂时不用光照颜色，输出黑白 Shadow Mask。确认形状和方向正确。

#### 第 6 步：加入 Bias、PCF 和光照

最后才调精度与柔化。

“冻结 Light Matrix”是非常有效的调试开关：阴影坐标不再随 Camera 或光变化，问题更容易复现。

---

### 12.18 常见错误

| 现象 | 常见原因 | 首先检查 |
| --- | --- | --- |
| 完全没有阴影 | FBO 不完整、Caster 未绘制、Sampler 绑定错 | 直接显示 Shadow Map |
| 整个场景都黑 | 比较方向反了、Depth Texture 全 0 | 显示 closest/current |
| 阴影方向相反 | Light Direction 约定混乱 | 绘制光源 Camera 和方向箭头 |
| 阴影贴在错误位置 | 忘记 Model Matrix 或 Light VP 顺序错 | 显示 shadow UV |
| 表面密集条纹 | Bias 太小或法线错误 | 先输出 `N dot L` 再调 Bias |
| 阴影悬浮 | Bias 太大 | 逐步减小最小 Bias |
| 阴影边缘被切断 | Light Orthographic Bounds 太小 | 绘制 Light Frustum |
| 边缘出现黑框 | 越界采样或 Border Depth 错 | 显式检查 0..1 |
| 相机移动时阴影抖动 | Light Projection 每帧细微移动 | 固定矩阵，再做 Texel Snapping |
| 透明叶片投下方块阴影 | Shadow Pass 没做 Alpha Cutout | 在 Shadow Fragment 中采 Alpha |
| Resize 后主画面正确但阴影错 | Shadow Viewport 未恢复或状态泄漏 | 每个 Pass 显式设 Viewport |

---

### 12.19 练习

#### 练习 A：最小场景

创建一个地面和一个悬在地面上的立方体。使用固定方向光与固定 Orthographic Projection，先得到硬边阴影。

#### 练习 B：深度比较手算

给出三组 `closestDepth`、`currentDepth` 和 `bias`，手算是否在阴影：

~~~text
A: closest=0.30, current=0.25, bias=0.001
B: closest=0.30, current=0.60, bias=0.001
C: closest=0.50, current=0.5004, bias=0.001
~~~

说明 C 为什么不会被判为阴影。

#### 练习 C：Bias 对照

制作运行时滑块控制 min/max Bias。分别截取 Acne 明显、正确、Peter-Panning 明显的三张图，并记录数值和场景尺度。

#### 练习 D：PCF 对照

在同一位置显示 1 次采样、3 x 3 PCF、5 x 5 PCF 的阴影边缘，同时记录每个 Fragment 的采样次数。

#### 练习 E：覆盖范围

固定 1024 分辨率，分别使用 10、50、200 世界单位宽的正交范围。解释边缘清晰度为什么变化。

#### 练习 F：故意出错实验（破坏阴影管线）

每次只改一处，先预测，再运行，然后恢复：

1. Shadow Pass 结束后不恢复 Viewport，直接画主场景。
2. 把深度比较方向反过来。
3. 把 Bias 设为 0。
4. 把 Bias 设成一个很大的值（例如 0.05）。
5. 计算 Shadow UV 时忘记乘 Model Matrix。
6. 把 Light 正交范围缩小到只覆盖场景的四分之一。
7. 采样 Shadow Map 时不检查 UV 是否在 `0..1` 内。

第 3 条和第 4 条产生的是**一对相反的典型缺陷**：Shadow Acne 和 Peter-Panning。它们都是 Bias 引起的，但方向相反。亲手做一遍，比记住两个名词有用得多。

---

### 12.20 本章验收标准
最低验收先得到固定、可解释的硬阴影：

1. Shadow FBO 完整，Depth Texture 能单独显示。
2. 地面和立方体在 Light Frustum 中，Shadow Map 中可认出轮廓。
3. Main Pass 能正确计算 Shadow UV、currentDepth 和 closestDepth。
4. 阴影方向正确，Caster 移动时阴影同步移动。
5. Camera 移动不会让阴影粘在屏幕上。
6. 可以显示 Shadow Map 和 Shadow Mask，并用固定数值手算一次深度比较。

推荐完善：

1. Bias 能控制 Acne 与 Peter-Panning，且你知道它不是通用常数。
2. 可以独立显示 Shadow UV 和 Depth Difference。
3. 3 x 3 PCF 能柔化锯齿边缘。

动态跟随、Texel Snapping、更复杂过滤和多盏灯阴影属于后续扩展。

进入下一章前，请确认自己能回答：

- 为什么 Main Camera 看得见，不代表 Light 看得见？
- Shadow Map 每个 Texel 保存的是什么？
- 为什么方向光通常使用正交投影？
- Light Clip Position 怎样变成 Texture UV？

完成推荐完善后再回答：

- Bias 为什么既不能太小，也不能太大？
- PCF 平均的到底是深度，还是多次比较结果？

下一章会把 Shadow Pass、Main Pass、Transparent Pass、Post Process 等工作组织成一份可解释、可调试的完整帧流程。
