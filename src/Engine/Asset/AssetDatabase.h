#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

enum class AssetType
{
    Model,
    Texture2D,
    TextureCube,
    Material,
    Shader,
    Scene,
};

struct AssetMeta
{
    std::string guid;
    AssetType type;
    std::string sourcePath;
    nlohmann::json importSettings;
    std::vector<std::string> dependencies;
};

class AssetDataBase
{
  public:
    static AssetDataBase& Get();
    ~AssetDataBase();
    AssetDataBase(const AssetDataBase&) = delete;
    AssetDataBase& operator=(const AssetDataBase&) = delete;

    /// 统一路径格式（`/` 分隔、lexically_normal）
    static std::string NormalizeSourcePath(const std::string& path);
    /// 解析 guid/虚拟路径/裸文件名 → 与 .meta 查找一致的规范源路径
    static std::string CanonicalizeMetaSourcePath(const std::string& sourcePath);

    AssetMeta LoadOrCreateMeta(const std::string& sourcePath);
    /// 从磁盘重新读取 .meta 并更新内存缓存（meta 外部修改后导入前调用）
    bool RefreshMetaFromDisk(const std::string& sourcePath);
    bool TryGetMeta(const std::string& sourcePath, AssetMeta& outMeta) const;
    std::string ResolveGuid(const std::string& guid);
    /// guid 或源路径 → 规范化源路径；无法解析时返回空
    std::string ResolveAssetRef(const std::string& guidOrPath);
    void SaveMeta(const AssetMeta& meta);
    size_t ScanAllMeta(const std::string& rootPath = "Content");

    static bool GetImportBool(const AssetMeta& meta, const char* key, bool defaultValue);
    static void SetImportBool(AssetMeta& meta, const char* key, bool value);
    static int GetImportInt(const AssetMeta& meta, const char* key, int defaultValue);
    static void SetImportInt(AssetMeta& meta, const char* key, int value);
    static std::string GetImportString(const AssetMeta& meta, const char* key);
    static void SetImportString(AssetMeta& meta, const char* key, const std::string& value);
    void UpdateImportBool(const std::string& sourcePath, const char* key, bool value);
    void UpdateDependencies(const std::string& sourcePath, const std::vector<std::string>& dependencies);

    static bool IsFloatTextureExtension(const std::string& sourcePath);
    static bool DefaultTextureGenerateMips(AssetType type, const std::string& sourcePath);
    /// 补全贴图 importSettings（showInUi / srgb / generateMips，仅写入缺失项）
    static bool EnsureTextureImportSettings(AssetMeta& meta, const std::string& sourcePath);
    /// 补全 cubemap 的 type / srgb / showInUi / generateMips（仅写入缺失项）
    static bool EnsureCubemapImportSettings(AssetMeta& meta, const std::string& sourcePath);

    void EnsureCubemapMeta(const std::string& sourcePath, bool srgb);
    void RemoveMeta(const std::string& sourcePath);

  private:
    AssetDataBase();
};
