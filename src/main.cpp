#include "Engine/Asset/AssetManager.h"
#include "Engine/Asset/AssetPaths.h"
#include "Engine/Asset/Types/Material.h"
#include "Engine/Asset/Types/Model.h"
#include "Engine/Asset/Types/Texture/IBLImage.h"
#include "Engine/Core/Config.h"
#include "Engine/Core/Log.h"
#include "Engine/Core/RenderDocSupport.h"
#include "Engine/Core/Window.h"
#include "Engine/Entity/Components/light/Light.h"
#include "Engine/Entity/Components/light/SkyBox.h"
#include "Engine/Entity/Components/MeshRender.h"
#include "Engine/Entity/Components/Transform.h"
#include "Engine/Entity/EntityManager.h"
#include "Engine/Renderer/RenderContext.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Scene/SceneSerializer.h"
#include "Engine/Scene/SceneSession.h"

#include <iostream>
#include <string>

#define MODULE "Main"

namespace
{
struct LaunchOptions
{
    std::string projectPath;
    bool showHelp = false;
};

LaunchOptions ParseCommandLine(int argc, char** argv)
{
    LaunchOptions opts;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            opts.showHelp = true;
            continue;
        }
        if (arg == "--project" || arg == "-p")
        {
            if (i + 1 < argc)
                opts.projectPath = argv[++i];
            continue;
        }
        if (opts.projectPath.empty() && !arg.empty() && arg[0] != '-')
            opts.projectPath = arg;
    }
    return opts;
}

void PrintUsage(const char* executable)
{
    const char* name = executable && executable[0] != '\0' ? executable : "GLEngine";
    std::cout << "用法: " << name << " [选项] [Project路径]\n"
              << "  -p, --project <路径>  指定项目根目录（含 Content/Project）；默认使用引擎根目录\n"
              << "  -h, --help            显示帮助\n";
}
} // namespace

int main(int argc, char** argv)
{
    const LaunchOptions opts = ParseCommandLine(argc, argv);
    if (opts.showHelp)
    {
        PrintUsage(argc > 0 ? argv[0] : nullptr);
        return 0;
    }

    Config::Get().LoadConfig();
    if (!opts.projectPath.empty())
        Config::Get().projectRoot = opts.projectPath;

    AssetPaths::Init();
#if GLE_ENABLE_RENDERDOC
    if (Config::Get().enableRenderDoc)
        RenderDocSupport::Init();
#endif
    LogA(LogLevel::INFO, "GLEngine starting");
    int INITIALWIDTH = Config::Get().initialWidth;
    int INITIALHEIGHT = Config::Get().initialHeight;
    std::unique_ptr<Window> win = std::make_unique<Window>();
    if (!win->Create(INITIALWIDTH, INITIALHEIGHT, "GLEngine"))
    {
        LogA(LogLevel::ERROR, "Failed to create window!");
        return 0;
    }
    AssetManager::Get().LoadEngineAssets();
    RenderContext context;
    Renderer renderer;
    renderer.Init(INITIALWIDTH, INITIALHEIGHT);
    win->renderCallback = [&renderer](RenderContext& ctx) { renderer.Render(ctx); };

    const std::string& startupScene = Config::Get().startupScene;
    const std::string defaultEngineScene = "Content/Engine/scenes/Default.scene";
    const std::string sceneToLoad = startupScene.empty() ? defaultEngineScene : startupScene;

    std::string msg;
    if (SceneSerializer::LoadScene(sceneToLoad, context, msg))
    {
        LogA(LogLevel::INFO, "{}", msg);
    }
    else
    {
        LogA(LogLevel::WARNING, "Scene load failed ({}), falling back to CreateDefaultScene", msg);
        SceneSession::Get().Reset();
    }
    // 加载资产
    AssetManager::Get().LoadComputeShader("project://ComputeShader/ComputeShader.glsl");
    EntityManager::Get().Init();
    win->Run(context);
    Config::Get().SaveConfig();
    LogA(LogLevel::INFO, "GLEngine shutdown");
    return 0;
}

#undef MODULE
