#pragma once

#include <filesystem>
#include <string>
#include <vector>

/// 资产虚拟路径与磁盘路径解析（engine:// / project://）。
class AssetPaths
{
  public:
    static AssetPaths& Get();

    /// 在 Config::LoadConfig 之后调用；解析 engineRoot / projectRoot 并将工作目录切到项目根。
    static void Init();

    static bool IsInitialized();

    /// guid / engine:// / project:// / 相对路径 → 规范化源路径（相对内容根，'/' 分隔）
    std::string ResolveToSourcePath(const std::string& guidOrPath) const;

    /// 规范化源路径 → 绝对磁盘路径（供 Assimp / stb / ifstream 使用）
    std::string ToAbsolutePath(const std::string& sourcePath) const;

    /// 绝对磁盘路径 → 相对源路径（优先相对 projectRoot，其次 engineRoot）
    std::string ToRelativeSourcePath(const std::filesystem::path& absolutePath) const;

    const std::filesystem::path& GetEngineRoot() const
    {
        return m_engineRoot;
    }

    const std::filesystem::path& GetProjectRoot() const
    {
        return m_projectRoot;
    }

    const std::string& GetProjectContentRoot() const
    {
        return m_projectContentRoot;
    }

    const std::string& GetEngineContentRoot() const
    {
        return m_engineContentRoot;
    }

    /// 供 ScanAllMeta 使用的内容根目录（相对源路径，去重）
    std::vector<std::string> GetContentScanRoots() const;

    /// 引擎 shader 公共头目录（相对 engineRoot），默认 {engineContentRoot}/shaders/include
    std::string GetEngineShaderIncludeRoot() const;

    /// 若 including 位于 {projectContentRoot}/<Name>/...，返回 {projectContentRoot}/<Name>/Shader/include；否则空
    std::string ResolveProjectShaderIncludeRoot(const std::string& includingSourcePath) const;

  private:
    AssetPaths() = default;

    std::filesystem::path ResolveContentBaseRoot(const std::string& normalizedSourcePath) const;
    std::string ResolveVirtualRef(const std::string& virtualRef, const std::string& contentRoot) const;
    std::string CanonicalizeSourcePath(const std::string& path) const;
    static std::string NormalizeSlashes(std::string path);

    std::filesystem::path m_engineRoot;
    std::filesystem::path m_projectRoot;
    std::string m_projectContentRoot = "Content/Project";
    std::string m_engineContentRoot = "Content/Engine";
    bool m_initialized = false;
};
