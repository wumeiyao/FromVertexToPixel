# 第 11 章：颜色空间、材质与基础光照

<!-- nav -->
> [上一章：场景、相机与可见性](10-scene-camera-culling.md) ｜ [下一章：方向光阴影贴图](12-shadow-mapping.md)
> [教材目录](README.md) ｜ [术语表](../references/glossary.md) ｜ [排错索引](../references/troubleshooting.md) ｜ [零基础前置](../references/beginner-prerequisites.md) ｜ [可运行实现](../exercises/05_lighting_shadows/11-color-material-lighting.md) ｜ [环境搭建](../plan/02-vscode-on-macos.md) ｜ [学习日志](../plan/progress.md)

前两章解决了“画什么”和“哪些对象需要画”。本章解决“一个可见表面最终应该是什么颜色”。这会同时涉及 Texture、Material、Normal、Light、Camera 和颜色空间。

本章使用经典的 Lambert + Blinn-Phong 模型。它不是最真实的 PBR。PBR 是 Physically Based Rendering，基于物理的渲染，用更接近真实能量规律的参数和公式描述材质；Blinn-Phong 公式较少，更适合先看清光照计算中每份数据怎样参与。

## 本章导读

### 本章学习目标

完成本章后，应能：

1. 区分 RGB 数值、线性光强和 sRGB 编码值。
2. 解释 Material 与 Mesh、Shader、Texture 的关系。
3. 明确 `N`、`L`、`V`、`H` 四个方向分别指向哪里。
4. 推导并实现环境光、Lambert 漫反射和 Blinn-Phong 镜面高光。
5. 正确变换 Normal，理解为什么非均匀缩放需要 Normal Matrix。
6. 用分层调试法定位“全黑、过暗、高光乱跑”等问题。

本章分三部分：先建立正确颜色链并准备 Material/Light 数据，再推导基础光照，最后写入 Shader 并分层调试。

第一次实现只做一种 Directional Light，并依次显示 `albedo`、`normal`、`N dot L`、最终颜色。Point Light、Spot Light 和 Normal Map 先读懂概念，不加入第一版 Shader。

## 第一部分：颜色、材质与光源数据

### 11.1 颜色首先是一组数字

渲染器中的颜色通常写成：

~~~text
color = (R, G, B, A)
~~~

- `R`：红色通道。
- `G`：绿色通道。
- `B`：蓝色通道。
- `A`：Alpha，通常用于透明和覆盖率。

使用浮点数时，普通显示颜色常落在 0 到 1：

| 数值 | 颜色 |
| --- | --- |
| `(0, 0, 0)` | 黑 |
| `(1, 1, 1)` | 白 |
| `(1, 0, 0)` | 红 |
| `(0, 1, 0)` | 绿 |
| `(0, 0, 1)` | 蓝 |
| `(1, 1, 0)` | 黄 |

RGB 是三个通道，不是三个灯光开关。中间值表示不同强度，例如 `(0.5, 0, 0)` 表示较暗的红。

#### 11.1.1 Alpha 不是亮度

Alpha 不表示“颜色有多亮”。它通常表示新颜色在混合中占多少，或几何表面对一个 Pixel 的覆盖程度。

一个颜色可以是明亮但半透明的：

~~~text
(1, 1, 1, 0.2)
~~~

透明混合会在第 14 章详细学习。本章先把不透明材质的 Alpha 设为 1。

---

### 11.2 为什么颜色数值不能总是直接加减乘除

显示器接收的颜色值通常不是与物理光强成正比的。图片文件和显示设备常使用 sRGB 编码，让有限数字更符合人眼对暗部变化的敏感程度。

这产生两种需要区分的值：

| 类型 | 含义 | 适合做什么 |
| --- | --- | --- |
| 线性值 | 数字与光强近似成比例 | 光照、插值、加法、混合 |
| sRGB 编码值 | 为存储和显示重新分配精度 | 颜色图片、最终显示 |

最重要的原则：

~~~text
读取颜色图片时：sRGB -> Linear
进行光照计算时：始终使用 Linear
送到显示器前：Linear -> sRGB
~~~

---

### 11.3 用数字理解 sRGB

假设一张图片某通道存储值是 `0.5`。它解码到线性空间后大约只有 `0.214`，并不是一半光强。

因此，把黑色 `0` 和白色 `1` 的 sRGB 数值直接平均：

~~~text
(0 + 1) / 2 = 0.5
~~~

得到的视觉中点会太暗。正确做法是：

~~~text
1. 把两端解码到 Linear：0 和 1
2. 在线性空间平均：0.5
3. 编码回 sRGB：大约 0.735
~~~

这解释了为什么错误颜色空间的光照经常显得过暗、发灰或层次不自然。

#### 11.3.1 sRGB 解码公式

对单个 sRGB 通道 `c_srgb`：

~~~text
当 c_srgb <= 0.04045：
    c_linear = c_srgb / 12.92

否则：
    c_linear = ((c_srgb + 0.055) / 1.055) ^ 2.4
~~~

#### 11.3.2 sRGB 编码公式

对单个线性通道 `c_linear`：

~~~text
当 c_linear <= 0.0031308：
    c_srgb = 12.92 * c_linear

否则：
    c_srgb = 1.055 * c_linear ^ (1 / 2.4) - 0.055
~~~

不要求背公式。OpenGL 可以借助 sRGB Texture 格式与 sRGB Framebuffer 自动做转换。你需要记住转换应发生在哪里。

很多资料会把最终编码近似写成 `pow(linear, 1/2.2)`，并称为 Gamma Correction，伽马校正。它有助于建立直觉，但 sRGB 的精确曲线是上面的分段公式，二者不要在同一条输出链中重复执行。

---

### 11.4 OpenGL 中怎样建立正确颜色链

#### 11.4.1 Base Color Texture

给人看的 Base Color 图片通常按 sRGB 上传：

~~~cpp
glTexImage2D(
    GL_TEXTURE_2D,
    0,
    GL_SRGB8_ALPHA8,  // GPU 存 sRGB，采样时自动解码为 Linear
    width,
    height,
    0,
    GL_RGBA,
    GL_UNSIGNED_BYTE,
    pixels
);
~~~

片段 Shader 的 `texture()` 返回值已经是线性 RGB。Alpha 不做 sRGB 转换。

#### 11.4.2 数据 Texture

Normal、Roughness、Metallic 等保存数值的 Texture 不能使用 sRGB 内部格式：

~~~cpp
// 示例：RGBA 数据纹理
internalFormat = GL_RGBA8;
~~~

否则 GPU 会把数值误当颜色解码。

#### 11.4.3 最终输出

若当前目标是支持 sRGB 的默认 Framebuffer，可以启用：

~~~cpp
glEnable(GL_FRAMEBUFFER_SRGB);
~~~

GPU 会在写入 sRGB 颜色附件时把 Shader 输出的线性 RGB 编码为 sRGB。窗口创建时还要确保默认 Framebuffer 支持 sRGB；具体设置取决于 GLFW 和平台。

后续第 15 章使用浮点 HDR Texture 时，中间颜色保持线性，Tone Mapping 后再做最终编码。HDR 是 High Dynamic Range，高动态范围，允许中间颜色保存大于 1 的高亮强度。不要既手写 `pow(color, 1/2.2)`，又开启自动 sRGB 写入，否则会发生两次编码，画面会过亮。

---

### 11.5 Material 到底是什么

Mesh 只描述表面在哪里，Material 描述表面怎样参与着色。

同一个球体 Mesh 可以配不同 Material：

~~~text
白色粗糙塑料
红色亮面塑料
木纹表面
金属表面
~~~

一个最小 Material 可以包含：

~~~cpp
struct Material {
    ProgramHandle shader;
    TextureHandle baseColorTexture;
    Vec3 baseColorFactor{1.0f, 1.0f, 1.0f};
    float specularStrength = 0.3f;
    float shininess = 32.0f;
};
~~~

逐项说明：

- `shader`：执行哪套 GPU 着色程序。
- `baseColorTexture`：表面基础颜色图片。
- `baseColorFactor`：对图片颜色再进行整体调色。
- `specularStrength`：镜面高光强度。
- `shininess`：高光集中程度。

最终 Albedo 可以是：

~~~text
albedo = sampledBaseColor * baseColorFactor
~~~

Albedo 可以先理解为“表面本身反射的基础颜色”，它还没有乘灯光。

Material 不只是几项 uniform。它通常也决定是否透明、是否双面、使用什么深度和混合状态。第 13、14 章会把这些状态放入不同渲染队列。

---

### 11.6 Light：光源提供什么数据

光源至少提供颜色、强度和方向或位置。

#### 11.6.1 Directional Light：方向光

方向光模拟非常远的太阳。所有位置收到的光线方向近似相同。

~~~cpp
struct DirectionalLight {
    Vec3 directionToLight;
    Vec3 color;
    float intensity;
};
~~~

本书明确约定：`directionToLight` 是“从表面指向光源”的方向。只要整个项目保持这一约定，就不需要在 Shader 中反复猜正负号。

#### 11.6.2 Point Light：点光源

点光源像灯泡，有世界位置，光向四周传播。

对世界位置 `P` 的 Fragment：

~~~text
toLight = lightPosition - P
distance = length(toLight)
L = toLight / distance
~~~

点光源通常随距离衰减。一个常见可调公式：

~~~text
attenuation = 1 / (constant + linear*d + quadratic*d*d)
~~~

`constant`、`linear`、`quadratic` 是人为设置的三个衰减系数；`d` 是光源到 Fragment 的距离。名字中的 linear/quadratic 表示这一项分别随距离一次方和平方增长。

本章实现方向光即可。理解点光源的意义是：`L` 不一定对所有 Fragment 相同。

#### 11.6.3 Spot Light：聚光灯

聚光灯在点光源基础上还限制一个锥形方向，例如手电筒。它需要额外比较 Fragment 方向与聚光灯朝向。本章暂不实现。

---

#### 本部分检查点

**能做出来**：说明你的颜色链每一环用的是什么空间：贴图怎么存、Shader 里怎么算、写出去时怎么编码。

**能回答**：

- sRGB 值 0.5 对应的线性值大约是多少？
- 为什么光照必须在线性空间算？
- Base Color 和 Normal Map 为什么不能用同一种内部格式？

**卡在这里就先不要往下读**：颜色链一旦双重编码，后面所有光照都会偏亮，而且看起来“好像也没错”。这一节必须先弄对。

## 第二部分：基础光照的数学

### 11.7 光照中的四个核心方向

对每个 Fragment，Blinn-Phong 常使用四个单位向量：

~~~text
N：Normal，表面朝向
L：从表面指向光源
V：从表面指向相机
H：L 与 V 的半角方向
~~~

图示：

~~~text
                 L（去往光源）
                /
               /      H（L 与 V 中间）
              /      /
             /      /
------------P---------------- 表面
             \      |
              \     | N（表面法线）
               \    |
                V（去往相机）
~~~

方向图只表示关系，不表示真实长度。

所有方向必须满足两个条件：

1. 位于同一个坐标空间，例如全部在 World Space。
2. 在点积前归一化，也就是长度为 1。

Local Normal 与 World Light Direction 做点积没有物理意义。它可能暂时出现亮暗，但模型旋转后光照会粘在模型上或朝错误方向移动。

---

### 11.8 Normal 怎样变换到 World Space

Position 用 Model Matrix 变换：

~~~text
P_world = M * P_local
~~~

方向不受平移影响，因此不能把 Normal 当成普通位置。若 Model Matrix 只包含旋转和统一缩放，使用 `mat3(M)` 后归一化通常能工作：

~~~glsl
N_world = normalize(mat3(uModel) * aNormal);
~~~

#### 11.8.1 非均匀缩放为什么会破坏法线

非均匀缩放是各轴比例不同，例如：

~~~text
scale = (2, 1, 0.5)
~~~

表面的切线方向和法线原本互相垂直。若对两者直接使用同一个非均匀缩放矩阵，变换后可能不再垂直，光照方向就错了。

Normal Matrix 定义为：

~~~text
N_matrix = transpose(inverse(mat3(M)))
~~~

变换：

~~~glsl
mat3 normalMatrix = transpose(inverse(mat3(uModel)));
vec3 worldNormal = normalize(normalMatrix * aNormal);
~~~

为什么是逆转置？直观上，Normal 的任务不是跟随表面上的点移动，而是继续与变换后的表面切线保持垂直。逆矩阵抵消缩放对方向关系的破坏，转置则把这一约束转换到法线所需形式。

每个 Vertex 都求逆矩阵比较浪费。CPU 可在每个对象 Model Matrix 改变时计算一次 `normalMatrix`，作为 uniform 上传。

若 Model Matrix 不可逆，例如某轴缩放为 0，Normal Matrix 无法定义。实际项目应禁止可渲染对象使用零缩放。

---

### 11.9 Lambert 漫反射

拿一张白纸照向手电筒：

- 正对光源时最亮。
- 逐渐侧向时变暗。
- 背对光源时没有直射光。

这由点积 `N dot L` 描述。

对两个单位向量：

~~~text
dot(N, L) = cos(theta)
~~~

`theta` 是两者夹角：

| 夹角 | dot | 含义 |
| --- | --- | --- |
| 0 度 | 1 | 完全正对光 |
| 60 度 | 0.5 | 只有一半朝向程度 |
| 90 度 | 0 | 侧对光 |
| 大于 90 度 | 负数 | 背对光 |

背面不应产生负光，所以截到 0：

~~~text
NdotL = max(dot(N, L), 0)
~~~

Lambert 漫反射项：

~~~text
diffuse = albedo * lightColor * lightIntensity * NdotL
~~~

符号含义：

- `albedo`：表面线性基础颜色。
- `lightColor`：光源线性颜色。
- `lightIntensity`：光强系数。
- `NdotL`：表面对光源的朝向程度。

RGB 乘法按通道进行。例如蓝色物体 `(0,0,1)` 被红光 `(1,0,0)` 照射，结果接近黑，因为物体主要反射蓝色，而光里几乎没有蓝色成分。

---

### 11.10 Ambient：人为补充的最低亮度

只有一盏方向光时，背光面会完全黑。现实世界中，光会在墙、地面和天空之间多次反弹；这叫间接光照或全局光照。

本章暂时不实现真实间接光，而使用一个简单 Ambient 项：

~~~text
ambient = albedo * ambientColor * ambientStrength
~~~

它让所有表面得到少量基础亮度。

Ambient 是教学近似，不是真正物理环境光。若设得过大，模型会失去明暗层次，看起来像自己发光。

---

### 11.11 Blinn-Phong 镜面高光

塑料、漆面和金属上常出现随相机移动的亮点，这叫 Specular Highlight，镜面高光。

Blinn-Phong 先计算半角向量：

~~~text
H = normalize(L + V)
~~~

它位于去往光源的 `L` 与去往相机的 `V` 之间。

当法线 `N` 接近 `H` 时，说明表面朝向恰好能把光反射向相机：

~~~text
NdotH = max(dot(N, H), 0)
specularFactor = NdotH ^ shininess
~~~

`shininess` 控制高光形状：

- 较小值，例如 8：高光宽、边缘柔。
- 较大值，例如 128：高光小而集中。

完整镜面项：

~~~text
specular = lightColor
         * lightIntensity
         * specularStrength
         * specularFactor
~~~

为了避免背光面出现不合理高光，通常只在 `NdotL > 0` 时计算 Specular。

经典 Blinn-Phong 并不严格守恒，也不能正确区分金属和非金属。本章用它理解向量和数据流，后续再迁移到 PBR。

---

### 11.12 基础光照完整公式

先定义：

~~~text
A = albedo，表面基础颜色
Ca = ambientColor，环境颜色
Ia = ambientStrength，环境强度
Cl = lightColor，灯光颜色
Il = lightIntensity，灯光强度
Ks = specularStrength，镜面强度
s = shininess，高光指数
~~~

方向项：

~~~text
d = max(dot(N, L), 0)
h = max(dot(N, H), 0) ^ s
~~~

三部分：

~~~text
ambient  = A * Ca * Ia
diffuse  = A * Cl * Il * d
specular = Cl * Il * Ks * h，且只在 d > 0 时存在
~~~

最终线性颜色：

~~~text
litColor = ambient + diffuse + specular
~~~

这个结果可以大于 1。若使用普通 8 位 Framebuffer，它会被限制；第 15 章会使用 HDR Framebuffer 保存高亮值并进行 Tone Mapping。

---

### 11.13 一次手算光照

假设：

~~~text
albedo = (0.8, 0.2, 0.1)
ambientColor = (1, 1, 1)
ambientStrength = 0.1
lightColor = (1, 1, 1)
lightIntensity = 1
NdotL = 0.5
specularStrength = 0.2
specularFactor = 0.25
~~~

环境项：

~~~text
ambient = (0.8, 0.2, 0.1) * 0.1
        = (0.08, 0.02, 0.01)
~~~

漫反射：

~~~text
diffuse = (0.8, 0.2, 0.1) * 0.5
        = (0.4, 0.1, 0.05)
~~~

镜面项：

~~~text
specular = (1, 1, 1) * 0.2 * 0.25
         = (0.05, 0.05, 0.05)
~~~

相加：

~~~text
litColor = (0.53, 0.17, 0.11)
~~~

注意镜面项没有乘红色 Albedo，所以白光下的高光接近白色。这是传统非金属 Blinn-Phong 的常见做法。

---

#### 本部分检查点

**能做出来**：手算一次完整光照：给定 N、L、V，算出 Ambient + Diffuse + Specular 的最终颜色。

**能回答**：

- N、L、V、H 各从哪里指向哪里？
- 为什么 Diffuse 项要对 0 取 max？
- 非均匀缩放为什么会破坏 Normal？

**卡在这里就先不要往下读**：公式里任何一个符号说不出方向，写进 Shader 也只是在试参数。回到 11.13 重算。

## 第三部分：Shader 实现与调试

### 11.14 Vertex Shader：准备 World Position 与 World Normal

~~~glsl
#version 410 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec2 vUv;

void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);

    vWorldPosition = worldPosition.xyz;
    vWorldNormal = uNormalMatrix * aNormal;
    vUv = aUv;

    gl_Position = uProjection * uView * worldPosition;
}
~~~

逐行意图：

- `aPosition/aNormal/aUv` 来自 Vertex Buffer。
- `worldPosition` 把局部顶点变到 World Space。
- `vWorldPosition` 交给片段 Shader，用来计算去往相机的方向。
- `uNormalMatrix` 把 Local Normal 正确变到 World Space。
- `vUv` 用来采样 Base Color Texture。
- `gl_Position` 是最终 Clip Space 位置。

没有在 Vertex Shader 中强制归一化 `vWorldNormal`，因为光栅化插值会再次改变长度；片段 Shader 中仍必须归一化。

---

### 11.15 Fragment Shader：逐步计算颜色

~~~glsl
#version 410 core

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec2 vUv;

uniform sampler2D uBaseColorTexture;
uniform vec3 uBaseColorFactor;

uniform vec3 uCameraPosition;
uniform vec3 uLightDirectionToLight;
uniform vec3 uLightColor;
uniform float uLightIntensity;

uniform vec3 uAmbientColor;
uniform float uAmbientStrength;
uniform float uSpecularStrength;
uniform float uShininess;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 baseSample = texture(uBaseColorTexture, vUv);
    vec3 albedo = baseSample.rgb * uBaseColorFactor;

    vec3 N = normalize(vWorldNormal);
    vec3 L = normalize(uLightDirectionToLight);
    vec3 V = normalize(uCameraPosition - vWorldPosition);

    float ndotl = max(dot(N, L), 0.0);

    vec3 ambient = albedo * uAmbientColor * uAmbientStrength;
    vec3 diffuse = albedo * uLightColor * uLightIntensity * ndotl;

    float specularFactor = 0.0;
    if (ndotl > 0.0) {
        vec3 H = normalize(L + V);
        specularFactor = pow(max(dot(N, H), 0.0), uShininess);
    }

    vec3 specular = uLightColor
                  * uLightIntensity
                  * uSpecularStrength
                  * specularFactor;

    vec3 linearColor = ambient + diffuse + specular;
    outColor = vec4(linearColor, baseSample.a);
}
~~~

这段 Shader 假设：

- Base Color Texture 使用 sRGB 内部格式，采样结果已是线性 RGB。
- 所有位置和方向位于 World Space。
- `uLightDirectionToLight` 的约定是从表面指向光源。
- 输出目标会在正确位置进行 sRGB 编码。

若这四条有一条不成立，画面即使“能亮”也可能是错误的。

---

### 11.16 Flat、Smooth 与顶点法线

同一个几何形状可以有不同法线表现。

#### Flat Shading

一个三角形内使用同一个面法线，折角清晰。低多边形模型常使用它。

#### Smooth Shading

相邻面共享或平均顶点法线，GPU 在三角形内插值后，表面看起来平滑。

Smooth Shading 不会增加几何细节，它只改变光照过渡。球体轮廓仍由实际 Triangle 决定。

硬边位置通常需要复制 Vertex，使相邻面能保存不同 Normal。第 9 章已经解释过：Vertex 是全部 Attribute 的组合，不只是 position。

---

### 11.17 Normal Map 现在处在什么位置

Normal Map 用 Texture 保存微小法线变化，让平面看起来有凹凸细节。它不会真的移动顶点，也不会改变物体轮廓。

Normal Map 中常见 RGB 值需要从 0 到 1 映射回 -1 到 1：

~~~glsl
vec3 tangentNormal = texture(uNormalMap, vUv).xyz * 2.0 - 1.0;
~~~

这根法线通常位于 Tangent Space，需要 TBN 矩阵转换到 World Space。TBN 由 Tangent、Bitangent、Normal 三个方向组成。

本章不要求实现 Normal Map。先确保普通顶点 Normal、颜色空间和基础光照正确，否则加入 TBN 会让错误来源更多。

---

### 11.18 分层调试方法

光照不正确时，不要同时修改灯光颜色、法线、矩阵和 Gamma。按以下顺序输出中间量。

#### 第 1 层：固定颜色

~~~glsl
outColor = vec4(1, 0, 1, 1);
~~~

若仍看不见，问题不在 Texture 和光照。

#### 第 2 层：Albedo

~~~glsl
outColor = vec4(albedo, 1);
~~~

验证 UV、Texture 绑定和 Base Color Factor。

#### 第 3 层：Normal 伪颜色

~~~glsl
outColor = vec4(N * 0.5 + 0.5, 1);
~~~

验证 Normal 数据、Normal Matrix 和插值。

#### 第 4 层：只看 N dot L

~~~glsl
outColor = vec4(vec3(ndotl), 1);
~~~

表面正对光时应白，侧面应黑，模型旋转后明暗应留在世界光方向上。

#### 第 5 层：只看 Specular

只输出 `specularFactor`，移动 Camera。高光应随视角变化，而不是粘在屏幕或模型固定一侧。

#### 第 6 层：恢复组合并检查 sRGB

最后才组合 Ambient、Diffuse、Specular，并对比开启/关闭 sRGB 输出。明确记录哪里发生解码与编码。

---

### 11.19 常见错误

| 现象 | 常见原因 | 首先检查 |
| --- | --- | --- |
| 模型全黑 | `N dot L` 全为负、灯强为 0、Texture 绑定错 | 输出 albedo 和 `ndotl` |
| 模型从内部亮 | `L` 方向正负号约定反了 | 画出灯方向，打印点积 |
| 模型旋转但光照跟着转 | Local Normal 与 World Light 混用 | 输出 World Normal |
| 非均匀缩放后亮暗扭曲 | 没用 inverse-transpose Normal Matrix | 禁用缩放做对比 |
| 高光粘在固定位置 | `V` 计算错误或空间不一致 | 只显示 Specular 并移动相机 |
| 画面整体过暗 | 在线性值上未正确编码，或直接用 sRGB 做光照 | 检查 Texture 内部格式和输出设置 |
| 画面过亮、发白 | 做了两次 Gamma/sRGB 编码 | 暂时关闭手写 `pow` |
| Normal Map 凹凸反向 | 绿色通道约定或 TBN 手性不同 | 显示切线空间 normal |
| 缩放到 0 后出现 NaN | Normal Matrix 不可逆 | 禁止零缩放 |

---

### 11.20 练习

#### 练习 A：颜色空间数字实验

在 Shader 中制作左右两块区域：

- 左边直接显示 sRGB 数值 0.5。
- 右边显示“线性 0.5 编码后的 sRGB”，大约 0.735。

观察视觉差异，并解释为什么线性一半光强不等于显示数值 0.5。

#### 练习 B：四种材质模式

为同一个模型实现：

1. 固定纯色。
2. Vertex Color。
3. Base Color Texture。
4. Directional Light + Blinn-Phong。

用键盘切换，确保每一层都能单独验证。

#### 练习 C：Normal Matrix

给模型设置 `(2, 1, 0.25)` 的非均匀缩放，分别使用：

- `mat3(model) * normal`。
- `transpose(inverse(mat3(model))) * normal`。

截图并解释两者差异。

#### 练习 D：手算再对照 Shader

使用第 11.13 节的数字，在 CPU 上打印 Ambient、Diffuse、Specular 和 Final，再让 Shader 使用同样输入。用浮点容差比较结果。

#### 练习 E：灯光方向约定

把灯光方向可视化成一根世界空间箭头。故意反转它，记录模型亮面怎样变化。写下项目最终采用的方向约定。

#### 练习 F：故意出错实验（破坏光照与颜色链）

每次只改一处，先预测，再运行，然后恢复：

1. 把 `max(0, N·L)` 改成 `N·L`（允许负值）。
2. 不归一化插值后的 Normal。
3. 用 `mat3(uModel)` 代替 inverse-transpose Normal Matrix，然后把模型按 `(2, 1, 0.25)` 缩放。
4. 把 Base Color Texture 的内部格式从 sRGB 改成普通 RGB。
5. 既开启 sRGB 写入，又在 Shader 里手写一次 `pow(color, 1.0/2.2)`。
6. 把 `L` 的约定反过来（从光源指向表面）。

第 3、4、5 条都属于“画面还能看，只是不对”的类型，最容易被忽略。做完后对照本章 11.19 常见错误，确认你能从现象反推出原因，而不是反过来。

---

### 11.21 本章验收标准

**最低验收**（第一次必须完成）。完成方向光时应满足：

1. Base Color 图片按 sRGB 读取，Normal 等数据 Texture 保持线性。
2. 光照计算全部在线性空间中完成，最终只编码一次。
3. 方向光的 `L` 约定在 C++ 和 GLSL 中完全一致。
4. 模型移动和旋转后，光照仍正确；第一版可以禁止非均匀缩放。
5. Camera 移动时镜面高光按预期变化。
6. 可以分别显示 Albedo、World Normal、`N dot L`、Specular 和 Final。
7. 你能逐项解释完整光照公式中的每个符号。

**推荐验收**：使用 inverse-transpose Normal Matrix 后，模型按 `(2, 1, 0.25)` 非均匀缩放，亮暗分布仍符合表面真实朝向。

**扩展验收**（可留到第二遍）：Point Light、Spot Light 和 Normal Map。它们都不属于本章第一次完成条件。

进入下一章前，请确认自己能回答：

- 为什么颜色 Texture 和数据 Texture 的内部格式不同？
- 为什么光照必须在线性空间计算？
- `N`、`L`、`V`、`H` 分别从哪里指向哪里？
- 为什么 Normal 不能总是直接乘 Model Matrix？
- Ambient、Diffuse 和 Specular 各表现什么现象？
- 为什么一个表面朝向光源，仍然可能处于阴影中？

最后一个问题正是下一章的主题：阴影要判断从光源到表面的路径是否被别的物体挡住。
