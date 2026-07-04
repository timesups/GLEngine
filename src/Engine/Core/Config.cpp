#include "Config.h"

#include "../Renderer/RenderPipeline/RenderPipelineRegistry.h"
#include "LightingConvention.h"
#include "Log.h"

#include <cctype>
#include <fstream>

#include <nlohmann/json.hpp>

#define MODULE "config"

namespace
{
constexpr const char* kConfigPath = "config.json";

std::string NormalizeExtension(std::string ext)
{
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (!ext.empty() && ext.front() != '.')
        ext.insert(ext.begin(), '.');
    return ext;
}

std::string ParseRenderPipelineFromJson(const nlohmann::json& value)
{
    if (value.is_string())
        return value.get<std::string>();

    if (value.is_number_integer())
    {
        switch (value.get<int>())
        {
        case 0:
            return "Forward";
        case 1:
            return "Deferred";
        case 2:
            return "DeferredPlus";
        default:
            break;
        }
    }

    return LightingConvention::DirectionalCalibrationBaseline::kDefaultRenderPipeline;
}
} // namespace

Config& Config::Get()
{
    static Config config;
    return config;
}

Config::Config()
{
    for (std::string& ext : assetBrowserHiddenExtensions)
        ext = NormalizeExtension(std::move(ext));
}

bool Config::IsAssetBrowserHiddenFile(const std::filesystem::path& path) const
{
    if (assetBrowserHiddenExtensions.empty())
        return false;
    const std::string ext = NormalizeExtension(path.extension().string());
    if (ext.empty())
        return false;
    for (const std::string& hidden : assetBrowserHiddenExtensions)
    {
        if (ext == NormalizeExtension(hidden))
            return true;
    }
    return false;
}

Config::~Config() = default;

std::string Config::GetGlslVersionString() const
{
    const int glslVersion = openGLMajor * 100 + openGLMinor * 10;
    if (openGLMajor > 3 || (openGLMajor == 3 && openGLMinor >= 3))
        return "#version " + std::to_string(glslVersion) + " core";
    if (openGLMajor == 3)
        return "#version " + std::to_string(120 + openGLMinor * 10);
    return "#version " + std::to_string(100 + (openGLMajor - 2) * 10 + openGLMinor * 10);
}

std::string Config::GetShaderVersionDefines() const
{
    const int glslVersion = openGLMajor * 100 + openGLMinor * 10;
    std::string defines;
    defines += "#define GLE_OPENGL_MAJOR " + std::to_string(openGLMajor) + "\n";
    defines += "#define GLE_OPENGL_MINOR " + std::to_string(openGLMinor) + "\n";
    defines += "#define GLE_GLSL_VERSION " + std::to_string(glslVersion) + "\n";
    defines += "#define GLE_OPENGL_" + std::to_string(openGLMajor) + "_" + std::to_string(openGLMinor) + "\n";
    return defines;
}

void Config::SaveConfig()
{
    nlohmann::json j;
    j["renderPipeline"] = renderPipeline;
    j["openGLMajor"] = openGLMajor;
    j["openGLMinor"] = openGLMinor;
    j["vsync"] = vsync;
    j["enableRenderDoc"] = enableRenderDoc;
    j["showRenderDocOverlay"] = showRenderDocOverlay;
    j["renderDocPath"] = renderDocPath;
    j["projectRoot"] = projectRoot;
    j["projectContentRoot"] = projectContentRoot;
    j["engineContentRoot"] = engineContentRoot;
    j["assetBrowserHiddenExtensions"] = assetBrowserHiddenExtensions;
    j["initialWidth"] = initialWidth;
    j["initialHeight"] = initialHeight;
    j["startupScene"] = startupScene;

    std::ofstream ofs(kConfigPath);
    if (!ofs)
        return;
    ofs << j.dump(4);
}

void Config::LoadConfig()
{
    std::ifstream ifs(kConfigPath);
    if (!ifs)
        return;

    nlohmann::json j;
    ifs >> j;

    if (j.contains("renderPipeline"))
        renderPipeline = RenderPipelineRegistry::ResolveName(ParseRenderPipelineFromJson(j["renderPipeline"]));
    if (j.contains("openGLMajor"))
        openGLMajor = j["openGLMajor"].get<int>();
    if (j.contains("openGLMinor"))
        openGLMinor = j["openGLMinor"].get<int>();
    if (j.contains("vsync"))
        vsync = j["vsync"].get<bool>();
    if (j.contains("enableRenderDoc"))
        enableRenderDoc = j["enableRenderDoc"].get<bool>();
    if (j.contains("showRenderDocOverlay"))
        showRenderDocOverlay = j["showRenderDocOverlay"].get<bool>();
    if (j.contains("renderDocPath") && j["renderDocPath"].is_string())
        renderDocPath = j["renderDocPath"].get<std::string>();
    if (j.contains("projectRoot") && j["projectRoot"].is_string())
        projectRoot = j["projectRoot"].get<std::string>();
    if (j.contains("initialWidth"))
        initialWidth = j["initialWidth"].get<int>();
    if (j.contains("initialHeight"))
        initialHeight = j["initialHeight"].get<int>();
    if (j.contains("projectContentRoot") && j["projectContentRoot"].is_string())
        projectContentRoot = j["projectContentRoot"].get<std::string>();
    if (j.contains("engineContentRoot") && j["engineContentRoot"].is_string())
        engineContentRoot = j["engineContentRoot"].get<std::string>();
    if (j.contains("assetBrowserHiddenExtensions") && j["assetBrowserHiddenExtensions"].is_array())
    {
        assetBrowserHiddenExtensions.clear();
        for (const nlohmann::json& ext : j["assetBrowserHiddenExtensions"])
        {
            if (!ext.is_string())
                continue;
            assetBrowserHiddenExtensions.push_back(NormalizeExtension(ext.get<std::string>()));
        }
        if (assetBrowserHiddenExtensions.empty())
            assetBrowserHiddenExtensions.push_back(".meta");
    }
    if (j.contains("startupScene") && j["startupScene"].is_string())
        startupScene = j["startupScene"].get<std::string>();

    // 修正非法配置
    if (initialWidth <= 0)
        initialWidth = 1280;
    if (initialHeight <= 0)
        initialHeight = 720;
}

#undef MODULE