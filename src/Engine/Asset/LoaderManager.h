#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Types/Shader.h"

class Texture2D;
class Material;
class Model;
struct aiNode;
struct aiScene;
struct aiMesh;
class TextureCube;
class ComputeShader;
struct IBLImage;

struct MaterialFileDesc
{
    std::string materialName;
    std::string shaderPath;
    int queueOverride = -1;
    bool hasQueueOverride = false;
    bool showInUi = true;
    std::unordered_map<std::string, MaterialProperty> properties;
};

int64_t get_mtime(const std::string& filePath);
void stringToLower(std::string& str);
std::string getBlockContent(const std::string& content, int startPoint);

template <typename T> struct FileState
{
    FileState();
    FileState(const std::string& path);
    void AddRelativeAsset(std::shared_ptr<T> asset);
    void UpdateModifyTime();
    bool IsNeedReload();

    std::string path;
    std::vector<std::shared_ptr<T>> relativeAsset;
    int64_t mTime = -1;
    int64_t mMetaTime = -1;
};

class LoaderManager
{
  public:
    static LoaderManager& Get();
    ~LoaderManager();

    LoaderManager(const LoaderManager&) = delete;
    LoaderManager& operator=(const LoaderManager&) = delete;

    void UpdateAssetFromDisk();
    bool LoadTextureFromFile(const std::string& path, std::shared_ptr<Texture2D>& tex, bool forceReload = false);
    bool LoadShader(const std::string& path, std::shared_ptr<Shader>& shader, bool reload = false);
    bool LoadComputeShader(const std::string& path, std::shared_ptr<ComputeShader>& shader, bool reload = false);

    bool LoadModel(const std::string& path, std::shared_ptr<Model>& model);
    bool LoadIBLMapFromFile(const std::string& path, std::shared_ptr<IBLImage>& ibl, bool forceReload = false);
    bool ParseMaterialFile(const std::string& path, MaterialFileDesc& outDesc);
    bool WriteMaterialFile(const std::string& path, const MaterialFileDesc& desc);
    bool BuildMaterialFileDescFromMaterial(const Material& mat, MaterialFileDesc& outDesc);

  private:
    void ProcessNode(aiNode* node, const aiScene* scene);
    void ProcessMesh(aiMesh* mesh, const aiScene* scene);
    std::string ReadAndPerprocessShaderFile(const std::string& path);
    bool LoadShaderCodeFromFile(const std::string& path, std::vector<PassCode>& codes, std::vector<PassOption>& options,
                                std::unordered_map<std::string, MaterialProperty>& propertiesMap,
                                std::vector<std::string>& propertyOrder);

  public:
    std::map<std::string, FileState<Model>> m_models;

  private:
    LoaderManager();
    std::map<std::string, FileState<Shader>> m_shaderFiles;
    std::map<std::string, FileState<ComputeShader>> m_computeShaderFiles;
    std::map<std::string, FileState<Texture2D>> m_TextureFiles;
    std::map<std::string, FileState<IBLImage>> m_iblMaps;
    std::shared_ptr<Shader> currentShader;
    std::shared_ptr<Model> currentModel;
};
