#pragma once

#include <filesystem>
#include <string>
#include <vector>

class Config
{
  public:
    static Config& Get();
    ~Config();
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    void SaveConfig();
    void LoadConfig();

    /// 局部光（点光/聚光）数量上限；UBO、阴影 FBO、shader 数组均依赖此值
    static constexpr int MaxLocalLight = 4;

    std::string renderPipeline = "Deferred";
    /// 请求的 OpenGL 上下文版本（GLFW 窗口提示与着色器 #version 均依赖此值）
    int openGLMajor = 4;
    int openGLMinor = 6;
    bool vsync = true;
    bool enableRenderDoc = false;
    /// RenderDoc 附加时左上角 FPS / 帧号等 in-app overlay
    bool showRenderDocOverlay = true;
    /// renderdoc.dll 路径或 RenderDoc 安装目录；空则尝试默认安装位置
    std::string renderDocPath;

    /// 外部项目根目录（空则与引擎根目录相同）
    std::string projectRoot;
    /// 场景尺度：世界单位与米的换算见 LightingConvention.h（默认 1 unit = 1 m）
    /// 项目内容根（相对 projectRoot），默认 Content/Project
    std::string projectContentRoot = "Content/Project";
    /// 引擎内置内容根（相对 engineRoot），默认 Content/Engine
    std::string engineContentRoot = "Content/Engine";

    /// GLSL #version 行（不含换行），如 "#version 460 core"
    std::string GetGlslVersionString() const;
    /// 注入着色器前缀的版本相关宏（含换行）
    std::string GetShaderVersionDefines() const;
    /// 资产浏览器中隐藏的文件扩展名（含点，如 ".meta"）
    std::vector<std::string> assetBrowserHiddenExtensions{".meta"};
    /// 启动时加载的场景（相对 projectRoot）；空则使用内置默认场景
    std::string startupScene;

    bool IsAssetBrowserHiddenFile(const std::filesystem::path& path) const;

    //初始分辨率
    int initialWidth = 1280;
    int initialHeight = 720;

  private:
    Config();
};
