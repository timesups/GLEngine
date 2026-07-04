# GLEngineNew

基于 OpenGL 的学习用小型渲染引擎，采用 **Entity–Component** 组织场景，自定义 **包裹式 GLSL** 管线描述，支持着色器 **热重载**，并集成 **Dear ImGui（docking 分支）** 作为编辑器 UI。

---

## 项目简介

本仓库实现了一个轻量级图形程序框架：使用 **GLFW** 创建窗口与 **OpenGL 4.6 Core** 上下文（版本可在 `config.json` 配置），**Glad** 加载 OpenGL 函数，通过 **Assimp** 加载模型、**stb_image** 加载纹理，**GLM** 处理数学。

- **渲染入口**：`Renderer::Render`，支持 **Forward** / **Deferred** 两条管线（`config.json` → `renderPipeline`）
- **资源**：`AssetManager`、`LoaderManager`、`AssetDatabase`（`.meta` GUID、依赖追踪）
- **场景**：`EntityManager` + `.scene` JSON 序列化；`SceneSession` 跟踪当前路径与脏标记
- **绘制**：`MeshRender` + `Material` + `ShaderPass`，按 **Render Queue** 排序

帧循环中 **ImGui** 使用全视口 **DockSpace**（中央节点穿透），3D 场景仅绘制在 Dock **中央区域**对应的帧缓冲矩形内；`RenderContext` 中的 `width`/`height` 与 `sceneViewport*` 与该区域一致，用于相机宽高比与 `glViewport`。

补充说明（渲染状态）：

- 引擎采用 OpenGL 状态机；每个 `ShaderPass` 会根据该 pass 的 `PassOption` **显式设置所需状态**。
- 为减少重复 `glEnable/glDisable/gl*Func` 调用，引擎内置了一个**轻量级 GL 状态缓存**（见 `ShaderPass.cpp`），仅在状态发生变化时才发出真正的 OpenGL 调用。
- 每帧开始会调用 `ShaderPass::InvalidateStateCache()` 做一次同步，避免外部代码直接改 GL 状态导致缓存与真实状态不一致。

---

## 技术栈

| 类别 | 依赖 |
|------|------|
| 语言与标准 | C++ **20**（x64） |
| 编译器 | **Clang**（LLVM，`x86_64-pc-windows-msvc` ABI） |
| 构建 | **CMake** + **Ninja** |
| 图形 API | OpenGL **4.6 Core**（GLFW 窗口提示与 GLSL `#version` 由 `config.json` 决定） |
| 窗口 / 输入 | GLFW |
| OpenGL 加载 | Glad（`external/glad` + `CPP_Library/include`） |
| 模型 | Assimp |
| 纹理 | stb_image（`external/stb`） |
| 数学 | GLM |
| JSON | nlohmann/json（场景序列化、`.meta`） |
| 即时 UI | **Dear ImGui**（**docking** 分支，`external/imgui`；GLFW + OpenGL3 后端） |
| 日志 | `Log()` / `LogManager`（`std::format`），ImGui **Log** 窗口显示 |
| 调试捕获 | **RenderDoc** in-app API（仅 **Debug** 构建启用，`GLE_ENABLE_RENDERDOC`） |
| IDE | **Cursor** / VS Code + **clangd** + **CodeLLDB** |

第三方库默认从 `C:\CPP_Library` 读取（可通过 `-DCPP_LIBRARY_ROOT=...` 覆盖），链接 `glfw3D.lib` / `glfw3.lib`、`assimp-vc143-mtd.lib` 等 **MSVC ABI** 预编译库。Clang 作为编译器，但仍需 **MSVC C++ Build Tools** 提供标准库头文件与链接库（**不需要 Visual Studio IDE**）。

**说明**：更新 ImGui 源码后，建议执行 `.\scripts\clean.ps1` 再重新编译，避免旧的 `.obj` 与头文件不一致导致链接错误。

---

## 功能概况

### 窗口与输入

- 创建 OpenGL 上下文、帧循环、帧缓冲尺寸回调；VSync 可在 Preferences 中切换并即时生效。
- 默认光标为系统光标；**按住鼠标右键**且 ImGui 不捕获鼠标时进入「视角模式」（光标禁用、累计鼠标位移给相机）；松开后可正常操作 ImGui。
- **WASD** 移动、**E/Q** 垂直、**Shift** 冲刺；未按住右键时，行走键仅在 ImGui 不占用键盘时生效。
- **G** 键边沿切换 UI 显示/隐藏（输入框聚焦时不响应）。

### 渲染

- **Forward**：经典前向路径。
- **Deferred**（默认）：GBuffer → SSAO → 延迟光照 → 天空盒 / 半透明 → Bloom → Tonemap 等后处理。
- 相机 UBO（`UniformBuffer` + `GLInput.glsl` 对齐）、阴影（定向光 + 局部光）、IBL 天空盒。
- **物理相机曝光**：光圈 / 快门 / ISO / EV 补偿 → Tonemap `exposure`（见 `LightingConvention::ComputeExposureLinear`）。

### 实体与组件

| 组件 | 说明 |
|------|------|
| `Transform` | 位置、旋转、缩放 |
| `MeshRender` | 模型 + 材质槽 |
| `Camera` | 透视、自由飞行、后处理参数、物理成像 |
| `Light` | 定向光（lux）、点光/聚光（lumen） |
| `SkyBox` | HDR IBL、背景绘制、强度调节 |

### 资源与热重载

- 着色器 / 纹理 / 模型 / 材质加载；`AssetManager` 缓存引擎默认资源。
- 根 `.glsl` 及 `#include` 依赖在磁盘变更后**自动重新编译**（`LoaderManager::UpdateAssetFromDisk`）。
- 资产虚拟路径：`engine://…`、`project://…`，或 `Content/Project/…` 相对路径。

### 编辑器 UI（ImGui）

| 面板 | 功能 |
|------|------|
| **Log** | 过滤、自动滚动、清空 |
| **Detail** | 选中实体或资产的属性编辑；材质保存到 `.mat` |
| **Outline** | 场景实体列表与选择 |
| **Asset Browser** | 浏览 `Content/`；Create **Material / Folder / Scene**；双击 `.scene` 加载 |
| **RenderDoc** | 捕获控制（Debug + `enableRenderDoc` 时显示） |

**菜单栏**

- **File**：New Scene、Save Scene（无路径时走另存为）、Save Scene As、Exit；菜单栏显示 `Scene: 路径*`（`*` 表示未保存）
- **Edit → Preferences…**：编辑 `config.json` 各字段（Apply / Save / Cancel）

脏场景在 New / Load / Exit 前会弹出确认对话框。

---

## 配置（`config.json`）

启动时从**项目根目录**读取，退出时写回。也可在 **Edit → Preferences** 中编辑。

| 字段 | 说明 | 即时生效 |
|------|------|----------|
| `renderPipeline` | `Forward` / `Deferred` | 需重启 |
| `openGLMajor` / `openGLMinor` | OpenGL 上下文与 GLSL 版本 | 需重启 |
| `vsync` | 垂直同步 | ✓ |
| `initialWidth` / `initialHeight` | 窗口初始尺寸 | ✓ |
| `projectRoot` | 外部项目根（空 = 引擎根） | ✓（刷新 AssetPaths） |
| `projectContentRoot` | 项目内容根，默认 `Content/Project` | ✓ |
| `engineContentRoot` | 引擎内容根，默认 `Content/Engine` | ✓ |
| `startupScene` | 启动场景（相对 `projectRoot`）；空则加载 `Content/Engine/scenes/Default.scene` | 下次启动 |
| `assetBrowserHiddenExtensions` | 资产浏览器隐藏扩展名，如 `.meta` | ✓ |
| `enableRenderDoc` | 启用 RenderDoc API 集成（仅 Debug 构建有效） | Apply 后 Reload |
| `renderDocPath` | `renderdoc.dll` 路径或 RenderDoc 安装目录 | Apply 后 Reload |
| `showRenderDocOverlay` | RenderDoc 左上角 FPS/帧号 overlay | ✓ |

示例：

```json
{
    "renderPipeline": "Deferred",
    "openGLMajor": 4,
    "openGLMinor": 6,
    "vsync": true,
    "initialWidth": 1280,
    "initialHeight": 720,
    "projectRoot": "",
    "projectContentRoot": "Content/Project",
    "engineContentRoot": "Content/Engine",
    "startupScene": "",
    "assetBrowserHiddenExtensions": [".meta"],
    "enableRenderDoc": true,
    "renderDocPath": "C:\\Program Files\\RenderDoc",
    "showRenderDocOverlay": false
}
```

---

## RenderDoc（Debug）

- 仅在 **Debug** 构建中编译 `RenderDocSupport.cpp` 并定义 `GLE_ENABLE_RENDERDOC`。
- 需在 `config.json` 中设置 `enableRenderDoc: true`。
- **不再**在构建后自动拷贝 `renderdoc.dll` 到 exe 目录；请在 Preferences 中配置 `renderDocPath`，或留空以尝试默认安装路径（`Program Files\RenderDoc` 等）。
- 通过 qrenderdoc 启动时，会使用已注入的 `renderdoc.dll`。
- Preferences 中可关闭 `showRenderDocOverlay`，隐藏左上角 FPS 等信息。

---

## 光照与场景尺度

约定详见 `src/Engine/Core/LightingConvention.h`：

| 项目 | 约定 |
|------|------|
| 长度 | **1 世界单位 = 1 米** |
| 定向光 | `Light::m_intensity` 单位为 **lux** |
| 点光 / 聚光 | 单位为 **lumen** |
| 相机曝光 | f/2.8、1/60s、ISO 100 → Tonemap exposure = 1.0 |
| Tonemap | `1 - exp(-hdr × exposure)`（`Content/Engine/shaders/Tonemap.glsl`） |

---

## 场景文件（`.scene`）

JSON 格式，当前版本 **v1**。由 `SceneSerializer` 读写。

```json
{
    "version": 1,
    "activeCamera": "MainCamera",
    "entities": [
        {
            "name": "MainCamera",
            "components": {
                "Transform": { "position": [0, 0, 5], "rotation": [0, 0, 0], "scale": [1, 1, 1] },
                "Camera": {
                    "fov": 60, "near": 0.1, "far": 1000,
                    "imaging": { "aperture": 2.8, "shutterSpeed": 0.016667, "iso": 100, "exposureCompensationEv": 0 },
                    "postSetting": { "tonemap": [...], "bloom": [...], "ssao": [...] }
                }
            }
        }
    ]
}
```

**已序列化组件**：`Transform`、`MeshRender`（model + materials）、`Camera`、`Light`、`SkyBox`。

- 内置默认场景：`Content/Engine/scenes/Default.scene`
- 加载失败时回退到代码内 `CreateDefaultScene()`
- 资产引用支持 `engine://`、`project://` 及相对源路径

---

## 命令行

```text
GLEngineNew.exe [选项] [Project路径]

  -p, --project <路径>  指定项目根目录（含 Content/Project）；默认使用引擎根目录
  -h, --help            显示帮助
```

---

## 仓库结构（概要）

```text
GLEngineNew/
├── Content/
│   ├── Engine/               # 引擎内置：shaders/、scenes/、hdr/、model/、materials/…
│   └── Project/              # 项目内容：按子项目组织（如 Endfield/Perlica/…）
├── build/                    # CMake/Ninja 构建目录（gitignore）
│   └── Debug/                # GLEngineNew.exe、PDB、运行时 DLL
├── cmake/toolchain/
│   └── clang-windows.cmake
├── config.json               # 引擎配置（见上文）
├── external/                 # glad、stb、imgui、renderdoc 头等
├── resources/                # Windows 图标等资源
├── scripts/
│   ├── build.ps1
│   ├── clean.ps1
│   └── run.ps1
├── src/
│   ├── main.cpp
│   └── Engine/
│       ├── Asset/            # AssetManager、AssetPaths、LoaderManager、AssetDatabase
│       ├── Core/             # Window、Log、Gui、Config、RenderDocSupport、LightingConvention
│       ├── Entity/           # Entity、EntityManager、组件
│       ├── Scene/            # SceneSerializer、SceneSession
│       └── Renderer/         # Renderer、RenderPipeline（Forward/Deferred）、CubemapBaker…
├── .vscode/
├── CMakeLists.txt
├── CMakePresets.json
├── .clang-format
└── .editorconfig
```

**资产路径约定：**

- 引擎内置：`engine://shaders/DefaultLit.glsl`、`engine://materials/DefaultMaterial`
- 项目资源：`project://…` 或 `Content/Project/…`
- `AssetPaths::Init()` 在 `Config::LoadConfig` 之后解析根目录并将工作目录切到项目根

---

## 构建与运行

### 依赖

| 工具 | 安装方式 | 说明 |
|------|----------|------|
| **LLVM/Clang** | `winget install LLVM.LLVM` | 编译器与 clangd |
| **CMake** | `winget install Kitware.CMake` | 构建系统 |
| **Ninja** | `winget install Ninja-build.Ninja` | 后端生成器 |
| **Windows SDK** | 随 Windows 或 Build Tools 安装 | 系统头文件与库 |
| **MSVC C++ Build Tools** | `winget install Microsoft.VisualStudio.2022.BuildTools` | 勾选「使用 C++ 的桌面开发」 |

第三方库目录默认为 `C:\CPP_Library`：

```text
C:\CPP_Library\
├── include/    # glad、GLFW、glm、assimp 等头文件
├── libs/       # glfw3D.lib、glfw3.lib、assimp-vc143-mtd.lib、unit.lib
└── bin/        # 运行时 DLL（assimp 等；RenderDoc 请自行配置路径）
```

路径不同时：

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/clang-windows.cmake `
  -DCPP_LIBRARY_ROOT="D:/your/libs"
```

### 编译

```powershell
cd GLEngineNew
.\scripts\build.ps1              # Debug
.\scripts\build.ps1 -Config Release
.\scripts\build.ps1 -Clean
```

或使用 CMake Presets：

```powershell
cmake --preset clang-ninja-debug
cmake --build --preset debug
```

输出：`build\Debug\GLEngineNew.exe`（Release 为 `build\Release\`）。构建后会将 Assimp 等必要 DLL 复制到 exe 同目录。

### 运行

**工作目录必须是项目根目录**（使 `Content/`、`config.json` 等相对路径正确，着色器热重载才能生效）。

```powershell
.\scripts\run.ps1
# 或
.\build\Debug\GLEngineNew.exe
```

调试（F5）时 `.vscode/launch.json` 已将 `cwd` 设为项目根。

### 清理

```powershell
.\scripts\clean.ps1
```

---

## 开发环境与调试

推荐使用 **Cursor** 或 **VS Code** 打开项目根目录。

| 扩展 | 用途 |
|------|------|
| **CodeLLDB** | 调试 |
| **CMake Tools** | Presets 与构建 |
| **clangd** | 补全、跳转、格式化 |

1. 首次打开后执行 `.\scripts\build.ps1`
2. 选择 **Debug GLEngineNew**，按 **F5**
3. 修改 `Content/` 下 shader 可在下一帧触发热重载

- **可以卸载 VS IDE**（不再使用 `.sln`）
- **不要卸载 MSVC Build Tools 与 Windows SDK**

---

## 代码风格

| 文件 | 作用 |
|------|------|
| `.editorconfig` | UTF-8、CRLF、4 空格缩进 |
| `.clang-format` | Allman 大括号、行宽 120 |
| `.clang-format-ignore` | 跳过 `external/`、`Content/`、`build/` |

---

## 日志

- 代码中：`Log(module, LogLevel::..., "…{}", …)`（`Log.h`）
- `LogManager` 队列由 `Gui::ShowLog` 每帧消费并显示

---

## 自定义 GLSL 着色器文件格式说明

本文描述引擎中 `LoaderManager` 对 **根着色器文件**（如 `Content/Engine/shaders/*.glsl` 或 `Content/Project/shaders/*.glsl`）的解析规则。该格式在标准 GLSL 外包了一层「Unity 风格」的块结构，并由加载器切出 `GLSLPROGRAM`～`ENDGLSL` 之间的代码，分别打上 `VERTEX` / `FRAGMENT` 宏后编译为 OpenGL Core 着色器（版本由 `config.json` 的 `openGLMajor` / `openGLMinor` 决定）。

### 1. 整体结构

根文件必须包含 **`SubShader`** 块（大小写不敏感，加载器会对全文做小写后再查找 `subshader`）。

推荐骨架：

```text
GLSLShader
{
    Properties
    {
        /* 材质属性默认值，语法见下文「材质与渲染队列」 */
    }
    SubShader
    {
        Pass
        {
            /* 渲染状态（可选） */
            GLSLPROGRAM
            /* 共用 GLSL 源码：通过 #ifdef VERTEX / FRAGMENT 分片 */
            ENDGLSL
        }
        /* 可有多个 Pass */
    }
}
```

- **`GLSLShader` / `Properties`**：外层约定；`Properties` 内声明 shader 默认材质属性（见「材质与渲染队列」）。
- **`Queue`**（可选）：在 `GLSLShader` 或 `SubShader` 块内、`GLSLPROGRAM` 之外声明默认渲染队列（见「材质与渲染队列」）。
- **`SubShader`**：必填；其 **第一个 `{` 与配对的 `}`** 之间的内容参与后续 `Pass` 扫描。
- **`Pass`**：至少一个；见下文 **Pass 关键字与换行** 限制。

### 2. Pass 块与换行要求

加载器在 `SubShader` 内部用子串查找 **`pass` 后紧跟换行** 的模式（实现上为 `pass\n`，且对当前子串做过小写处理）。

因此请保证：

- 关键字写作 `Pass` 或 `pass` 均可（子串会先转小写）。
- **`Pass` 与下一行之间必须是换行**，例如：

```text
    Pass
    {
```

若写成 `Pass {` 同一行，**可能无法匹配** `pass\n`，导致该 Pass 被忽略或无法解析。

每个 `Pass` 对应引擎里的一个 **ShaderPass**（一对顶点/片段程序；若检测到几何着色器宏则走特殊分支，见第 6 节）。

### 3. GLSL 源码区间：`GLSLPROGRAM` … `ENDGLSL`

- 仅在 **`GLSLPROGRAM` 与 `ENDGLSL` 之间** 的行会进入实际 GLSL 源码（行内包含这两个标记的整行会被去掉，不进入源码）。
- 该区间的代码会 **原样拼接**（前面加换行累积），再分别包上版本与宏（见第 6 节）。

#### 顶点 / 片元分工（推荐写法）

使用宏区分阶段，与加载器注入的 `#define VERTEX` / `#define FRAGMENT` 配合：

```glsl
#ifdef VERTEX
void main() { ... }
#endif

#ifdef FRAGMENT
out vec4 FragColor;
void main() { ... }
#endif
```

### 4. `#include` 规则

- 写在 **最终会进入 `GLSLPROGRAM` 的文本里**（或与之一同被预处理的根文件/被包含文件中）。
- 语法：`#include "相对路径字符串"`（须 **双引号**）。
- **解析方式**：取**当前正在读取的文件**路径中，最后一个 `/` **之前**的目录为 `rootFolder`，再拼接为：  
  `rootFolder + "/" + 引号内路径`  
  例如根文件为 `Content/Engine/shaders/Foo.glsl`，`#include "/include/Core.glsl"` 会解析为 `Content/Engine/shaders/include/Core.glsl`。

被包含文件中的 `#include` 同样按**该文件自身路径**计算 `rootFolder`，支持多级嵌套。

引擎公共头目录：`Content/Engine/shaders/include/`（`Core.glsl`、`Transform.glsl`、`Lighting.glsl`、`Light.glsl`、`Shadow.glsl` 等）。

### 5. Pass 内的渲染状态（`GLSLPROGRAM` 之外）

在 `Pass {` 与 `GLSLPROGRAM` 之间可写若干**整行**配置。加载器会对该行做 **Trim + Tokenize + Lowercase**，然后按“第一个 token（关键字）”精确解析（避免 `BlendEq` 被误当作 `Blend` 解析的历史问题）。

常见项如下（关键字大小写不敏感；下文均以小写展示）。

#### 5.1 Pass 名称

- 行内含 `name` 且带双引号时，引号内内容作为 **Pass 名称**（`passName`）。

#### 5.2 深度

- **`zwrite on|off`**：`off` 关闭深度写入，否则开启。
- **`ztest off`**：关闭深度测试（对应 `PassOption.enableDepthTest=false`）。
- **`ztest <比较函数>`**：开启深度测试并设置深度比较函数。

比较函数字符串需与下表一致（小写）：  
  `always`、`never`、`less`、`equal`、`lequal`、`greater`、`notequal`、`gequal`。

#### 5.3 裁剪

- **`cull front|back|off`**

#### 5.4 混合

- **`blend off`**：关闭混合。
- **`blend <源因子> <目标因子>`**：开启混合并设置因子。因子名为小写、无空格枚举名，例如：`srcalpha`、`oneminussrcalpha`。  
  支持的符号与 `LoaderManager.cpp` 中 `DeterminBlendFunc` 一致，例如：`zero`、`one`、`srccolor`、`oneminussrccolor`、`dstcolor`、`oneminusdstcolor`、`srcalpha`、`oneminussrcalpha`、`dstalpha`、`oneminusdstalpha`。
- **`blendeq <方程>`**：`add`、`subtract`、`reversesubtract`、`min`、`max`。

#### 5.5 模板（Stencil）

支持两种写法：

- **`stencil off`**：关闭模板测试（对应 `PassOption.enableStencilTest=false`）。
- **`Stencil { ... }`**：进入模板子块；出现该块时会自动启用模板测试（`enableStencilTest=true`），直到匹配到 `}` 退出子块（简化状态机，与括号嵌套无关）。

子块内可包含（关键字匹配为小写子串）：

- **`BitMask <值>`**：模板写入掩码（按 C++ `stoi(..., 0)` 解析，支持 `0xff` 等进制形式）。
- **`AndMask <值>`**：模板比较掩码（同样支持 `0xff`）。
- **`Func <比较函数>`**：模板比较函数（字符串集合与深度比较函数一致）。
- **`Ref <值>`**：模板参考值（整数，支持 `0x..`）。
- **`Fail` / `DpFail` / `DpPass`**：模板操作（`keep`、`zero`、`replace`、`incr`、`incrwrap`、`decr`、`decrwrap`、`invert` 等，见 `DeterminOpByString`）。

### 6. 编译时注入与几何着色器

对每个 Pass 提取出的 `realPassCode`，加载器会生成：

- **顶点着色器**：`#version … core` + `#define VERTEX` + `realPassCode`
- **片段着色器**：`#version … core` + `#define FRAGMENT` + `realPassCode`

若 **整个 Pass 原始文本**（`passCode`）中出现子串 **`#ifdef GEOMETRY`**（大小写敏感，按源码查找），则会把 **片段着色器侧** 的拼接改为带 `#define GEOMETRY` 的同一份 `realPassCode`（与引擎内几何着色器路径一致；具体以 `ShaderPass` 实现为准）。

### 7. 与引擎内置头文件的关系

`#include` 的公共头（如 `GLInput.glsl`、`Transform.glsl`）中通常声明：

- 顶点属性：`aPosition`、`aNormal`、`aTexcoord0` 等（与网格顶点布局一致）。
- UBO：`camera_buffer`（`std140`，binding 与 C++ 侧 `UniformBuffer` 一致）。
- 宏/函数：如 `ObjectToClipPos` 等（见各 `include` 文件）。

编写 Pass 内 GLSL 时应与这些声明保持一致。

### 8. 热重载

根文件及被 `#include` 的文件若参与依赖登记，在磁盘修改时间变化时会触发对应 `Shader` 重新走上述流程编译。路径与依赖关系由 `LoaderManager` 的 `m_shaderFiles` / `FileState` 维护。

### 9. 常见错误

| 现象 | 可能原因 |
|------|----------|
| 报错无 SubShader | 根文件缺少 `SubShader` 块或拼写无法被小写后匹配为 `subshader`。 |
| 报错无 Pass | 无有效 `Pass` 块，或 `Pass` 未单独起行（未形成 `pass\n`）。 |
| 包含文件找不到 | `#include` 引号路径与 `rootFolder` 拼接后不是实际文件路径；或用了非双引号。 |
| 编译失败 | `GLSLPROGRAM` 内语法与目标 GLSL 版本不符；或缺少 `#ifdef VERTEX`/`FRAGMENT` 对应入口。 |
| Stencil/描边无效果 | pass 未启用模板测试（用 `Stencil {}` 块或 `stencil off/on`）；或描边外扩在空间上混用了 object/world（见下方示例）。 |

### 10. 参考示例

仓库内可参考：

- `Content/Engine/shaders/DefaultLit.glsl` — 引擎默认光照（Forward / Deferred 宏分支）。
- `Content/Engine/shaders/DeferredLight.glsl` — 延迟光照全屏 Pass。
- `Content/Project/shaders/EndfieldGbuffer.glsl` — 项目自定义 GBuffer。
- `Content/Project/Endfield/Perlica/Shader/Endfield.glsl` — 角色着色示例。

更细节的解析实现见：`src/Engine/Asset/LoaderManager.cpp`（`LoadShader`、`ReadAndPerprocessShaderFile`、`getBlockContent`）。

---

## 材质与渲染队列

引擎使用 **整型渲染队列**（类似 Unity `RenderQueue`）：数值越小越早绘制。`Material::GetRenderQueue()` 返回最终队列；材质可覆盖 shader 默认值。

### 内置队列常量（`RenderQueue.h`）

| 名称 | 值 | 用途 |
|------|-----|------|
| `Background` | 1000 | 背景 |
| `Geometry` / `Opaque` | 2000 | 默认不透明几何（GBuffer pass） |
| `AlphaTest` | 2450 | 裁剪/测试类不透明 |
| `Skybox` | 2500 | 天空盒（`DrawRenderQueue` 范围覆盖此队列时绘制天空盒实体） |
| `Transparent` | 3000 | 半透明（延迟光照之后，按深度从远到近排序） |
| `Overlay` | 4000 | 叠加层 |

**划分规则：**

- 每帧 `GatherSceneRenderUnit` 收集全部 `MeshRender` 子网格到 `renderUnits`，按 **队列升序** 排序；`queue >= Transparent` 时同队列内再按视线深度 **从远到近** 排序。
- `EntityManager::DrawRenderQueue(min, max[, materialOverride])` 绘制 `renderQueue ∈ [min, max)` 的单元；范围含 `Skybox` 时会先绘制天空盒实体。
- 未写 `Queue` 时：pass 开启 `blend` → 默认 `Transparent(3000)`，否则 `Geometry(2000)`。

**Renderer 中的典型调用：**

```cpp
// Shadow / GBuffer：不透明
DrawRenderQueue(0, RenderQueue::OpaqueUpperBound[, shadowMat]);

// 延迟光照之后
DrawRenderQueue(RenderQueue::Skybox, RenderQueue::Transparent);  // 天空盒
DrawRenderQueue(RenderQueue::Transparent, INT_MAX);              // 半透明 + Overlay
```

### 在 Shader 文件中指定 Queue

在根 `.glsl` 的 `GLSLPROGRAM` **之外**写一行（大小写不敏感）：

```text
Queue Geometry
```

也支持别名（`opaque`、`transparent`、`alphatest`…）或任意整数，例如 `Queue 2450`。

示例见 `Content/Engine/shaders/DefaultLit.glsl`（`Queue Geometry`）。

### Shader `Properties` 默认值

支持 `float` / `int` / `bool` / `vec2`~`vec4` 与 `sampler2D` / `samplerCube` 的 `=` 默认值。新建材质或未在 UI 指定贴图时，会自动加载默认纹理。

`sampler2D` 写法示例：

```text
sampler2D _baseColor = "white"                              // -> Content/Project/textures/white.png
sampler2D _normal = "normal"                                // -> Content/Project/textures/normal.png
sampler2D _ao = Content/Project/textures/custom_ao.png      // 完整或相对路径亦可
```

短名会在 `Content/Project/textures/` 下依次尝试 `.png`、`.jpg`、`.jpeg`。内置占位图：`black`、`grey`、`normal`、`white`。

### 材质文件（`.mat` / `.material`）

`AssetManager::LoadMaterial` 解析如下结构（可通过资源浏览器加载）：

```text
Material
{
    Name FloorMat
    Shader Content/Project/shaders/EndfieldGbuffer.glsl
    Queue Geometry
    Properties
    {
        float _brightness = 1.0
        sampler2D _baseColor = Content/Project/textures/bricks2.jpg
    }
}
```

- **`Name`**：可选；省略则使用文件名。
- **`Shader`**：必填；指向根着色器路径。
- **`Queue`**：可选；覆盖 shader 默认队列（语法同 shader 的 `Queue`）。
- **`Properties`**：属性块，行语法与 shader `Properties` 一致；`sampler2D` / `samplerCube` 可在 `=` 后写纹理路径并自动加载。

代码中也可调用 `Material::SetRenderQueue(int)` / `ClearRenderQueueOverride()` 覆盖或恢复继承 shader 队列。

---

## 描边（Stencil Outline）注意事项

`Content/Project/shaders/` 中带 stencil 的 Pass 可用于描边。描边外扩建议在 **object space** 完成（例如 `aPosition + normalize(aNormal) * width`），然后再用 `ObjectToClipPos` 投影，避免把 world space 法线加到 object space 位置上造成外扩失真/不可见（旋转、缩放时尤其明显）。

同时，公共头 `Content/Engine/shaders/include/Transform.glsl` 中 `ObjectToWorldN(vec3 normal)` 已修复为使用传入参数，而不是错误地始终读取 `aNormal`。
