#include "AssetPaths.h"

#include "../Core/Config.h"
#include "AssetDatabase.h"

#include "../Core/Log.h"

#include <unordered_set>

#define MODULE "AssetPaths"

namespace
{
constexpr const char* kEngineScheme = "engine://";
constexpr const char* kProjectScheme = "project://";

std::filesystem::path DetectEngineRoot()
{
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path();
    for (int depth = 0; depth < 10; ++depth)
    {
        std::error_code ec;
        // Content/Engine 或 Content/Project 存在 → 标准内容根
        if (fs::is_directory(dir / "Content" / "Engine", ec) || fs::is_directory(dir / "Content" / "Project", ec))
            return fs::absolute(dir);

        // CMakeLists + src/Content → 源码仓库根（不用 config.json，避免 build/ 下的 config 误判）
        if (fs::exists(dir / "CMakeLists.txt", ec) &&
            (fs::is_directory(dir / "Content", ec) || fs::is_directory(dir / "src", ec)))
            return fs::absolute(dir);

        if (!dir.has_parent_path())
            break;
        dir = dir.parent_path();
    }
    return fs::absolute(fs::current_path());
}

std::filesystem::path ResolveConfiguredRoot(const std::string& configured, const std::filesystem::path& fallbackRoot)
{
    if (configured.empty())
        return fallbackRoot;

    namespace fs = std::filesystem;
    fs::path path(configured);
    if (path.is_absolute())
        return fs::absolute(path);
    return fs::absolute(fallbackRoot / path);
}
} // namespace

AssetPaths& AssetPaths::Get()
{
    static AssetPaths instance;
    return instance;
}

bool AssetPaths::IsInitialized()
{
    return Get().m_initialized;
}

void AssetPaths::Init()
{
    AssetPaths& paths = Get();
    if (paths.m_initialized)
        return;

    Config& config = Config::Get();
    paths.m_engineRoot = DetectEngineRoot();
    if (config.projectRoot.empty())
        paths.m_projectRoot = paths.m_engineRoot;
    else
        paths.m_projectRoot = ResolveConfiguredRoot(config.projectRoot, paths.m_engineRoot);

    std::error_code ec;
    std::filesystem::current_path(paths.m_projectRoot, ec);
    if (ec)
    {
        LogA(LogLevel::WARNING, "AssetPaths: failed to set working directory to '{}'", paths.m_projectRoot.string());
    }
    else
    {
        // 工作目录已切到项目根，重新加载 config.json（避免 build/ 下的旧 config 覆盖内容根）
        config.LoadConfig();
    }

    paths.m_projectContentRoot =
        NormalizeSlashes(config.projectContentRoot.empty() ? "Content/Project" : config.projectContentRoot);
    paths.m_engineContentRoot =
        NormalizeSlashes(config.engineContentRoot.empty() ? "Content/Engine" : config.engineContentRoot);

    paths.m_initialized = true;
    LogA(LogLevel::INFO, "AssetPaths: engineRoot='{}' projectRoot='{}' engineContent='{}' projectContent='{}'",
         paths.m_engineRoot.string(), paths.m_projectRoot.string(), paths.m_engineContentRoot,
         paths.m_projectContentRoot);

    std::error_code includeEc;
    const std::filesystem::path includeDir = paths.m_engineRoot / paths.GetEngineShaderIncludeRoot();
    if (!std::filesystem::is_directory(includeDir, includeEc))
    {
        LogA(LogLevel::ERROR, "AssetPaths: engine shader include directory missing: '{}'", includeDir.string());
    }
}

std::string AssetPaths::NormalizeSlashes(std::string path)
{
    for (char& c : path)
    {
        if (c == '\\')
            c = '/';
    }
    while (!path.empty() && path.front() == '/')
        path.erase(path.begin());
    while (!path.empty() && path.back() == '/')
        path.pop_back();
    return path;
}

std::string AssetPaths::ResolveVirtualRef(const std::string& virtualRef, const std::string& contentRoot) const
{
    std::string relative = virtualRef;
    if (!contentRoot.empty())
        relative = contentRoot + "/" + relative;
    return NormalizeSlashes(std::move(relative));
}

std::string AssetPaths::CanonicalizeSourcePath(const std::string& path) const
{
    std::string normalized = NormalizeSlashes(path);
    if (normalized.empty())
        return {};

    namespace fs = std::filesystem;
    const fs::path input(normalized);
    if (input.is_absolute())
    {
        std::error_code ec;
        const fs::path relativeProject = fs::relative(input, m_projectRoot, ec);
        if (!ec && !relativeProject.empty())
            return NormalizeSlashes(relativeProject.generic_string());

        std::error_code engineEc;
        const fs::path relativeEngine = fs::relative(input, m_engineRoot, engineEc);
        if (!engineEc && !relativeEngine.empty())
            return NormalizeSlashes(relativeEngine.generic_string());
        return normalized;
    }

    if (!m_projectContentRoot.empty())
    {
        if (normalized == m_projectContentRoot || normalized.rfind(m_projectContentRoot + "/", 0) == 0)
            return normalized;
    }

    if (!m_engineContentRoot.empty())
    {
        if (normalized == m_engineContentRoot || normalized.rfind(m_engineContentRoot + "/", 0) == 0)
            return normalized;
    }

    if (!m_projectContentRoot.empty())
        return NormalizeSlashes(m_projectContentRoot + "/" + normalized);
    return normalized;
}

std::string AssetPaths::ResolveToSourcePath(const std::string& guidOrPath) const
{
    if (guidOrPath.empty())
        return {};

    if (guidOrPath.rfind(kEngineScheme, 0) == 0)
        return ResolveVirtualRef(guidOrPath.substr(std::char_traits<char>::length(kEngineScheme)), m_engineContentRoot);

    if (guidOrPath.rfind(kProjectScheme, 0) == 0)
        return ResolveVirtualRef(guidOrPath.substr(std::char_traits<char>::length(kProjectScheme)), m_projectContentRoot);

    return CanonicalizeSourcePath(guidOrPath);
}

std::filesystem::path AssetPaths::ResolveContentBaseRoot(const std::string& normalizedSourcePath) const
{
    if (!m_engineContentRoot.empty() &&
        (normalizedSourcePath == m_engineContentRoot ||
         normalizedSourcePath.rfind(m_engineContentRoot + "/", 0) == 0))
        return m_engineRoot;
    return m_projectRoot;
}

std::string AssetPaths::ToAbsolutePath(const std::string& sourcePath) const
{
    if (sourcePath.empty())
        return {};

    namespace fs = std::filesystem;
    const std::string normalized = NormalizeSlashes(sourcePath);
    const fs::path input(normalized);
    if (input.is_absolute())
        return NormalizeSlashes(fs::absolute(input).generic_string());

    const fs::path base = ResolveContentBaseRoot(normalized);
    return NormalizeSlashes(fs::absolute(base / input).generic_string());
}

std::string AssetPaths::ToRelativeSourcePath(const std::filesystem::path& absolutePath) const
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path relativeProject = fs::relative(absolutePath, m_projectRoot, ec);
    if (!ec && !relativeProject.empty())
        return NormalizeSlashes(relativeProject.generic_string());

    std::error_code engineEc;
    const fs::path relativeEngine = fs::relative(absolutePath, m_engineRoot, engineEc);
    if (!engineEc && !relativeEngine.empty())
        return NormalizeSlashes(relativeEngine.generic_string());

    return NormalizeSlashes(fs::absolute(absolutePath).generic_string());
}

std::vector<std::string> AssetPaths::GetContentScanRoots() const
{
    std::vector<std::string> roots;
    std::unordered_set<std::string> seen;
    auto addRoot = [&](const std::filesystem::path& base, const std::string& root)
    {
        const std::string normalized = NormalizeSlashes(root);
        if (normalized.empty() || !seen.insert(normalized).second)
            return;
        std::error_code ec;
        if (std::filesystem::is_directory(base / normalized, ec))
            roots.push_back(normalized);
    };

    addRoot(m_engineRoot, m_engineContentRoot);
    addRoot(m_projectRoot, m_projectContentRoot);
    if (roots.empty())
    {
        addRoot(m_engineRoot, "Content/Engine");
        addRoot(m_projectRoot, "Content/Project");
    }
    return roots;
}

std::string AssetPaths::GetEngineShaderIncludeRoot() const
{
    return NormalizeSlashes(m_engineContentRoot + "/shaders/include");
}

std::string AssetPaths::ResolveProjectShaderIncludeRoot(const std::string& includingSourcePath) const
{
    const std::string source = NormalizeSlashes(includingSourcePath);
    const std::string projectRoot = NormalizeSlashes(m_projectContentRoot);
    if (source.empty() || projectRoot.empty())
        return {};

    const std::string prefix = projectRoot + "/";
    if (source.rfind(prefix, 0) != 0)
        return {};

    const std::string rest = source.substr(prefix.size());
    const size_t slash = rest.find('/');
    const std::string projectName = slash == std::string::npos ? rest : rest.substr(0, slash);
    if (projectName.empty())
        return {};

    return NormalizeSlashes(projectRoot + "/" + projectName + "/Shader/include");
}

#undef MODULE
