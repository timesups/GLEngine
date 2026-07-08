#pragma once
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "AssetDatabase.h"

#include <glm/glm.hpp>

class Texture2D;
class Material;
class Model;
class Shader;
class ComputeShader;
class Texture;
class Mesh;
struct IBLImage;

class AssetManager
{
  public:
    ~AssetManager();
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;
    void LoadEngineAssets();
    /// 按模型 meta 的 importSettings 绑定材质（加载或 meta 变更后调用）
    void ApplyModelMaterials(const std::shared_ptr<Model>& model);
    static AssetManager& Get();
    static bool IsInitializing();

    /// guid 或源路径 → 规范化源路径
    static std::string ResolveAssetPath(const std::string& guidOrPath);
    AssetMeta GetAssetMeta(const std::string& sourcePath);
    bool TryGetAssetMeta(const std::string& sourcePath, AssetMeta& outMeta) const;

    std::shared_ptr<Shader> LoadShader(const std::string& path);
    std::shared_ptr<ComputeShader> LoadComputeShader(const std::string& path, bool forceReload = false);
    std::shared_ptr<Texture2D> LoadTexture(const std::string& path, bool forceReload = false);
    /// 1×1 线性 RGB 贴图（用于标定材质等），path 仅作标识与缓存键
    std::shared_ptr<Texture2D> CreateSolidColorTexture2D(const std::string& path, const glm::vec3& linearRgb);
    std::shared_ptr<Model> LoadModel(const std::string& path);
    std::shared_ptr<Material> CreateMaterial(const std::string& sourcePath, std::shared_ptr<Shader> shader);
    std::shared_ptr<Material> LoadMaterial(const std::string& path);
    /// 从 .mat 构建材质实例，不注册到 m_materials（供编辑器预览）
    std::shared_ptr<Material> BuildMaterialFromFile(const std::string& path,
                                                    std::vector<std::string>* outDependencies = nullptr);
    std::shared_ptr<Material> FindLoadedMaterialBySourcePath(const std::string& sourcePath) const;
    std::shared_ptr<IBLImage> LoadIBL(const std::string& path, bool forceReload = false);
    bool IsAssetLoadedAtPath(const std::string& sourcePath) const;
    Mesh* GetScreenMesh();

    template <typename T>
    static void CollectUiVisibleKeys(const std::map<std::string, std::shared_ptr<T>>& assets,
                                     std::vector<std::string>& outKeys, bool sorted = false)
    {
        outKeys.clear();
        for (const auto& [name, asset] : assets)
        {
            if (asset && asset->m_showInUi)
                outKeys.push_back(name);
        }
        if (sorted)
            std::sort(outKeys.begin(), outKeys.end());
    }

    template <typename T> std::shared_ptr<T> GetAsset(const std::string& path)
    {
        const std::string key = NormalizeAssetKey(path);
        if (key.empty())
            return {};

        if constexpr (std::is_same_v<T, Shader>)
        {
            auto it = m_shaders.find(key);
            return it == m_shaders.end() ? std::shared_ptr<Shader>{} : it->second;
        }
        else if constexpr (std::is_same_v<T, ComputeShader>)
        {
            auto it = m_computeShaders.find(key);
            return it == m_computeShaders.end() ? std::shared_ptr<ComputeShader>{} : it->second;
        }
        else if constexpr (std::is_same_v<T, Texture> || std::is_same_v<T, Texture2D>)
        {
            auto it = m_textures.find(key);
            return it == m_textures.end() ? std::shared_ptr<Texture2D>{} : it->second;
        }
        else if constexpr (std::is_same_v<T, Model>)
        {
            auto it = m_models.find(key);
            return it == m_models.end() ? std::shared_ptr<Model>{} : it->second;
        }
        else if constexpr (std::is_same_v<T, Material>)
        {
            auto it = m_materials.find(key);
            return it == m_materials.end() ? std::shared_ptr<Material>{} : it->second;
        }
        else if constexpr (std::is_same_v<T, IBLImage>)
        {
            auto it = m_iblImages.find(key);
            return it == m_iblImages.end() ? std::shared_ptr<IBLImage>{} : it->second;
        }
        else
        {
            static_assert(!sizeof(T), "AssetManager::GetAsset<T>: unsupported asset type T");
            return {};
        }
    }

  private:
    AssetManager();
    void InitScreenMesh();
    static std::string NormalizeAssetKey(const std::string& guidOrPath);

  public:
    std::map<std::string, std::shared_ptr<Shader>> m_shaders;
    std::map<std::string, std::shared_ptr<ComputeShader>> m_computeShaders;
    std::map<std::string, std::shared_ptr<Texture2D>> m_textures;
    std::map<std::string, std::shared_ptr<Model>> m_models;
    std::map<std::string, std::shared_ptr<Material>> m_materials;
    std::map<std::string, std::shared_ptr<IBLImage>> m_iblImages;

  private:
    std::unique_ptr<Mesh> m_screenMesh;
};
