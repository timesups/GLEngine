#include "AssetManager.h"

#include "../Core/Log.h"
#include "../Core/util.h"
#include "AssetDatabase.h"
#include "LoaderManager.h"

#include "Types/ComputeShader.h"
#include "Types/Material.h"
#include "Types/Mesh.h"
#include "Types/Model.h"
#include "Types/Shader.h"
#include "Types/Texture/IBLImage.h"
#include "Types/Texture/Texture2D.h"
#include "Types/Texture/TextureCube.h"

#include "../Renderer/CubemapBaker.h"

#include <unordered_map>

#define MODULE "AssetManager"

namespace
{
bool s_assetManagerInitializing = false;

std::shared_ptr<Material> ResolveMaterialRef(const std::string& path, const std::shared_ptr<Material>& fallback)
{
    if (path.empty())
        return fallback;

    AssetManager& assets = AssetManager::Get();
    const std::string resolved = AssetManager::ResolveAssetPath(path);
    if (resolved.empty())
    {
        LogA(LogLevel::WARNING, "ResolveMaterialRef: invalid material ref '{}'", path);
        return fallback;
    }

    if (std::shared_ptr<Material> cached = assets.GetAsset<Material>(resolved))
        return cached;

    if (std::shared_ptr<Material> loaded = assets.LoadMaterial(path))
        return loaded;

    LogA(LogLevel::WARNING, "ResolveMaterialRef: failed to load material '{}'", path);
    return fallback;
}

std::string ResolveModelSlotMaterialPath(const AssetMeta& meta, size_t slotIndex, const std::string& slotName)
{
    if (meta.importSettings.is_object() && meta.importSettings.contains("materialSlots") &&
        meta.importSettings["materialSlots"].is_array())
    {
        for (const nlohmann::json& entry : meta.importSettings["materialSlots"])
        {
            if (!entry.is_object() || !entry.contains("material") || !entry["material"].is_string())
                continue;

            const std::string materialPath = entry["material"].get<std::string>();
            if (entry.contains("slot") && entry["slot"].is_number_integer() &&
                entry["slot"].get<int>() == static_cast<int>(slotIndex))
                return materialPath;
            if (entry.contains("name") && entry["name"].is_string() && entry["name"].get<std::string>() == slotName)
                return materialPath;
        }
    }

    return AssetDataBase::GetImportString(meta, "defaultMaterial");
}

void BindModelMaterials(const std::shared_ptr<Model>& model)
{
    if (!model)
        return;

    std::shared_ptr<Material> engineDefault =
        AssetManager::Get().GetAsset<Material>("engine://materials/DefaultMaterial");
    if (!engineDefault)
    {
        LogA(LogLevel::WARNING, "BindModelMaterials: DefaultMaterial not found");
        return;
    }

    const AssetMeta meta = AssetDataBase::Get().LoadOrCreateMeta(model->m_path);
    const size_t slotCount = model->m_sourceMaterials.empty() ? 1 : model->m_sourceMaterials.size();
    model->m_materials.assign(slotCount, engineDefault);

    std::vector<std::string> dependencies;
    for (size_t i = 0; i < slotCount; ++i)
    {
        const std::string slotName =
            i < model->m_sourceMaterials.size() ? model->m_sourceMaterials[i].name : std::string{};
        const std::string materialPath = ResolveModelSlotMaterialPath(meta, i, slotName);
        model->m_materials[i] = ResolveMaterialRef(materialPath, engineDefault);
        if (!materialPath.empty())
            dependencies.push_back(materialPath);
    }

    if (!dependencies.empty())
        AssetDataBase::Get().UpdateDependencies(model->m_path, dependencies);

    LogA(LogLevel::INFO, "BindModelMaterials: '{}' bound {} material slot(s)", model->m_path, slotCount);
}

bool ShowInUiFromMeta(const AssetMeta& meta)
{
    return AssetDataBase::GetImportBool(meta, "showInUi", true);
}

void ApplyMaterialProperty(Material& mat, const MaterialProperty& prop)
{
    switch (prop.type)
    {
    case MaterialPropertyType::Float:
        mat.SetFloatProperty(prop.name, std::get<float>(prop.value));
        break;
    case MaterialPropertyType::Int:
        mat.SetIntProperty(prop.name, std::get<int>(prop.value));
        break;
    case MaterialPropertyType::Bool:
        mat.SetBoolProperty(prop.name, std::get<bool>(prop.value));
        break;
    case MaterialPropertyType::Vec2:
        mat.SetVec2Property(prop.name, std::get<glm::vec2>(prop.value));
        break;
    case MaterialPropertyType::Vec3:
        mat.SetVec3Property(prop.name, std::get<glm::vec3>(prop.value));
        break;
    case MaterialPropertyType::Vec4:
        mat.SetVec4Property(prop.name, std::get<glm::vec4>(prop.value));
        break;
    case MaterialPropertyType::Mat3:
        mat.SetMat3Property(prop.name, std::get<glm::mat3>(prop.value));
        break;
    case MaterialPropertyType::Mat4:
        mat.SetMat4Property(prop.name, std::get<glm::mat4>(prop.value));
        break;
    case MaterialPropertyType::Texture2D:
    case MaterialPropertyType::TextureCube:
        break;
    }
}
} // namespace

std::string AssetManager::NormalizeAssetKey(const std::string& guidOrPath)
{
    const std::string resolved = ResolveAssetPath(guidOrPath);
    return AssetDataBase::NormalizeSourcePath(resolved);
}

bool AssetManager::IsInitializing()
{
    return s_assetManagerInitializing;
}

AssetManager& AssetManager::Get()
{
    AssetDataBase::Get();
    static AssetManager instance;
    return instance;
}

AssetManager::~AssetManager() = default;

std::string AssetManager::ResolveAssetPath(const std::string& guidOrPath)
{
    return AssetDataBase::Get().ResolveAssetRef(guidOrPath);
}

AssetMeta AssetManager::GetAssetMeta(const std::string& sourcePath)
{
    return AssetDataBase::Get().LoadOrCreateMeta(sourcePath);
}

bool AssetManager::TryGetAssetMeta(const std::string& sourcePath, AssetMeta& outMeta) const
{
    return AssetDataBase::Get().TryGetMeta(sourcePath, outMeta);
}

AssetManager::AssetManager() : m_screenMesh(std::make_unique<Mesh>()) {}

void AssetManager::LoadEngineAssets()
{
    s_assetManagerInitializing = true;
    // Load Icons
    LoadTexture("engine://icon/pointLight.png");
    LoadTexture("engine://icon/skyLight.png");
    LoadTexture("engine://icon/spotLight.png");
    LoadTexture("engine://icon/sunLight.png");

    // Load some Textures
    LoadTexture("engine://textures/black.png");
    LoadTexture("engine://textures/grey.png");
    LoadTexture("engine://textures/normal.png");
    LoadTexture("engine://textures/white.png");
    // Load Shaders
    LoadShader("engine://shaders/ErrorShader.glsl");
    LoadShader("engine://shaders/EquirectMap.glsl");
    LoadShader("engine://shaders/irradiance.glsl");
    LoadShader("engine://shaders/prefilter.glsl");
    LoadShader("engine://shaders/brdf.glsl");
    LoadShader("engine://shaders/SSAO.glsl");
    LoadShader("engine://shaders/SSAO_Blur.glsl");
    LoadShader("engine://shaders/BlurX.glsl");
    LoadShader("engine://shaders/BlurY.glsl");
    LoadShader("engine://shaders/Copy.glsl");
    LoadShader("engine://shaders/Tonemap.glsl");
    LoadShader("engine://shaders/BloomSetup.glsl");
    LoadShader("engine://shaders/BloomUpsample.glsl");
    LoadShader("engine://shaders/DownSample.glsl");
    LoadShader("engine://shaders/DeferredLight.glsl");
    LoadShader("engine://shaders/Billboard.glsl");
    LoadShader("engine://shaders/ShowColor.glsl");
    // Load Materials
    CreateMaterial("engine://materials/DefaultMaterial", LoadShader("engine://shaders/DefaultLit.glsl"));
    CreateMaterial("engine://materials/ShadowGlobal", LoadShader("engine://shaders/ShadowCasterGlobal.glsl"));
    CreateMaterial("engine://materials/ShadowLocal", LoadShader("engine://shaders/ShadowCasterLocal.glsl"));
    CreateMaterial("engine://materials/CustomDepth", LoadShader("engine://shaders/CustomDepth.glsl"));

    // Load Models
    LoadModel("engine://model/cube.fbx");
    LoadModel("engine://model/monkey.fbx");
    LoadModel("engine://model/plane.fbx");
    LoadModel("engine://model/sphere.fbx");
    LoadModel("engine://model/arrow.fbx");
    InitScreenMesh();
    CubemapBaker::EnsureBrdfLut();
    LoadIBL("engine://hdr/sunny_rose_garden.hdr");

    s_assetManagerInitializing = false;
    for (auto& [path, mat] : m_materials)
    {
        (void)path;
        mat->ApplyTextureDefaultsFromShader();
    }

    LogA(LogLevel::INFO, "Default assets initialized (DefaultMaterial + shader)");
}

void AssetManager::ApplyModelMaterials(const std::shared_ptr<Model>& model)
{
    BindModelMaterials(model);
}
void AssetManager::InitScreenMesh()
{
    float positions[4][3] = {{1.f, 1.f, 0.f}, {1.f, -1.f, 0.f}, {-1.f, -1.f, 0.f}, {-1.f, 1.f, 0.f}};
    std::vector<unsigned int> index = {0, 1, 2, 3, 0, 2};
    std::vector<Vertex> vertexs;
    for (int i = 0; i < 4; i++)
    {
        Vertex ver;
        ver.Position = glm::vec3(positions[i][0], positions[i][1], positions[i][2]);
        ver.Color = glm::vec3(0, 0, 0);
        ver.Normal = glm::vec3(0, 0, -1);
        ver.Tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        ver.Texcoord0 = glm::vec2(0, 0);
        ver.Texcoord1 = glm::vec2(0, 0);
        vertexs.push_back(ver);
    }
    m_screenMesh->Create(vertexs, index, Bounds());
}
Mesh* AssetManager::GetScreenMesh()
{
    return m_screenMesh.get();
}

std::shared_ptr<Shader> AssetManager::LoadShader(const std::string& path)
{
    const std::string resolvedPath = ResolveAssetPath(path);
    if (resolvedPath.empty())
    {
        LogA(LogLevel::ERROR, "LoadShader: invalid asset ref '{}'", path);
        return nullptr;
    }

    AssetDataBase::Get().RefreshMetaFromDisk(resolvedPath);
    const AssetMeta meta = AssetDataBase::Get().LoadOrCreateMeta(resolvedPath);
    const bool effectiveShowInUi = ShowInUiFromMeta(meta);

    std::shared_ptr<Shader> shader = std::make_shared<Shader>();
    if (!LoaderManager::Get().LoadShader(resolvedPath, shader))
    {
        LogA(LogLevel::ERROR, "LoadShader failed: {} (note: returning a Shader object anyway; it may have 0 passes)",
             resolvedPath);
        shader->m_showInUi = effectiveShowInUi;
        shader->m_path = resolvedPath;
        shader->m_name = Util::GetNameFromPath(resolvedPath);
        m_shaders[resolvedPath] = shader;
        return shader;
    }
    shader->m_showInUi = effectiveShowInUi;
    shader->m_path = resolvedPath;
    const bool firstRegister = (m_shaders.find(resolvedPath) == m_shaders.end());
    m_shaders[resolvedPath] = shader;
    if (firstRegister)
        LogA(LogLevel::INFO, "Shader registered: '{}' ({})", shader->m_name, resolvedPath);
    return shader;
}

std::shared_ptr<ComputeShader> AssetManager::LoadComputeShader(const std::string& path, bool forceReload)
{
    const std::string key = NormalizeAssetKey(path);
    if (key.empty())
    {
        LogA(LogLevel::ERROR, "LoadComputeShader: invalid asset ref '{}'", path);
        return nullptr;
    }

    AssetDataBase::Get().RefreshMetaFromDisk(key);
    AssetDataBase::Get().LoadOrCreateMeta(key);

    std::shared_ptr<ComputeShader> shader;
    if (!LoaderManager::Get().LoadComputeShader(key, shader, forceReload))
    {
        LogA(LogLevel::ERROR, "LoadComputeShader failed: {}", key);
        return nullptr;
    }

    if (!shader || !shader->IsReady())
    {
        LogA(LogLevel::ERROR, "LoadComputeShader: shader not ready: {}", key);
        return nullptr;
    }

    shader->m_path = key;
    const bool firstRegister = (m_computeShaders.find(key) == m_computeShaders.end());
    m_computeShaders[key] = shader;
    if (firstRegister)
        LogA(LogLevel::INFO, "Compute shader registered: '{}' ({})", shader->m_name, key);
    return shader;
}

std::shared_ptr<Model> AssetManager::LoadModel(const std::string& path)
{
    const std::string resolvedPath = ResolveAssetPath(path);
    if (resolvedPath.empty())
    {
        LogA(LogLevel::ERROR, "LoadModel: invalid asset ref '{}'", path);
        return nullptr;
    }

    AssetDataBase::Get().RefreshMetaFromDisk(resolvedPath);
    const AssetMeta meta = AssetDataBase::Get().LoadOrCreateMeta(resolvedPath);
    const bool effectiveShowInUi = ShowInUiFromMeta(meta);

    std::shared_ptr<Model> model = std::make_shared<Model>();
    if (!LoaderManager::Get().LoadModel(resolvedPath, model))
        LogA(LogLevel::ERROR, "LoadModel failed: {}", resolvedPath);
    else if (model->m_meshSections.empty())
        LogA(LogLevel::WARNING, "LoadModel '{}' succeeded but has 0 mesh sections", resolvedPath);
    model->m_showInUi = effectiveShowInUi;
    m_models[resolvedPath] = model;
    BindModelMaterials(model);
    return model;
}

std::shared_ptr<Material> AssetManager::CreateMaterial(const std::string& sourcePath, std::shared_ptr<Shader> shader)
{
    const std::string resolvedPath = NormalizeAssetKey(sourcePath);
    if (resolvedPath.empty())
    {
        LogA(LogLevel::ERROR, "CreateMaterial: invalid asset ref '{}'", sourcePath);
        return nullptr;
    }

    const bool replaced = (m_materials.find(resolvedPath) != m_materials.end());
    std::shared_ptr<Material> mat = std::make_shared<Material>(shader);
    mat->m_name = Util::GetNameFromPath(resolvedPath);
    mat->m_sourcePath = resolvedPath;
    mat->m_showInUi = shader ? shader->m_showInUi : true;
    m_materials[resolvedPath] = mat;
    if (replaced)
        LogA(LogLevel::WARNING, "CreateMaterial: replaced existing material '{}'", resolvedPath);
    else
        LogA(LogLevel::INFO, "CreateMaterial '{}'", resolvedPath);
    if (!IsInitializing())
        mat->ApplyTextureDefaultsFromShader();
    return mat;
}

std::shared_ptr<Material> AssetManager::BuildMaterialFromFile(const std::string& path,
                                                              std::vector<std::string>* outDependencies)
{
    const std::string resolvedPath = ResolveAssetPath(path);
    if (resolvedPath.empty())
    {
        LogA(LogLevel::ERROR, "BuildMaterialFromFile: invalid asset ref '{}'", path);
        return nullptr;
    }

    AssetDataBase::Get().RefreshMetaFromDisk(resolvedPath);
    MaterialFileDesc desc;
    if (!LoaderManager::Get().ParseMaterialFile(resolvedPath, desc))
        return nullptr;

    const AssetMeta meta = AssetDataBase::Get().LoadOrCreateMeta(resolvedPath);
    const bool materialShowInUi = desc.showInUi && ShowInUiFromMeta(meta);

    std::shared_ptr<Shader> shader = LoadShader(desc.shaderPath);
    if (!shader || shader->m_passes.empty())
    {
        LogA(LogLevel::ERROR, "BuildMaterialFromFile failed: shader '{}' invalid for '{}'", desc.shaderPath,
             resolvedPath);
        return nullptr;
    }

    std::shared_ptr<Material> mat = std::make_shared<Material>(shader);
    mat->m_name = desc.materialName;
    mat->m_showInUi = materialShowInUi;
    if (desc.hasQueueOverride)
        mat->SetRenderQueue(desc.queueOverride);

    if (outDependencies)
    {
        outDependencies->clear();
        outDependencies->push_back(desc.shaderPath);
    }

    for (const auto& [name, prop] : desc.properties)
    {
        MaterialProperty applied = prop;
        applied.name = name;

        switch (applied.type)
        {
        case MaterialPropertyType::Texture2D:
        {
            if (applied.defaultTexturePath.empty())
                break;
            if (outDependencies)
                outDependencies->push_back(applied.defaultTexturePath);
            std::shared_ptr<Texture> tex = LoadTexture(applied.defaultTexturePath);
            if (tex && tex->GetId())
                mat->SetTexture2DProperty(name, std::move(tex));
            break;
        }
        case MaterialPropertyType::TextureCube:
        {
            if (applied.defaultTexturePath.empty())
                break;
            if (outDependencies)
                outDependencies->push_back(applied.defaultTexturePath);
            std::shared_ptr<IBLImage> ibl = LoadIBL(applied.defaultTexturePath);
            if (ibl && ibl->irradiance)
                mat->SetTextureCubeProperty(name, std::static_pointer_cast<Texture>(ibl->irradiance));
            break;
        }
        default:
            ApplyMaterialProperty(*mat, applied);
            break;
        }
    }

    return mat;
}

std::shared_ptr<Material> AssetManager::FindLoadedMaterialBySourcePath(const std::string& sourcePath) const
{
    const std::string path = NormalizeAssetKey(sourcePath);
    if (path.empty())
        return nullptr;

    const auto it = m_materials.find(path);
    return it == m_materials.end() ? nullptr : it->second;
}

std::shared_ptr<Material> AssetManager::LoadMaterial(const std::string& path)
{
    const std::string key = NormalizeAssetKey(path);
    if (key.empty())
    {
        LogA(LogLevel::ERROR, "LoadMaterial: invalid asset ref '{}'", path);
        return nullptr;
    }

    if (const auto cached = m_materials.find(key); cached != m_materials.end())
        return cached->second;

    std::vector<std::string> dependencies;
    std::shared_ptr<Material> mat = BuildMaterialFromFile(path, &dependencies);
    if (!mat)
        return nullptr;

    mat->m_sourcePath = key;
    m_materials[key] = mat;
    AssetDataBase::Get().UpdateDependencies(key, dependencies);
    LogA(LogLevel::INFO, "LoadMaterial OK: '{}' queue={} from {}", mat->m_name, mat->GetRenderQueue(), key);
    return mat;
}

std::shared_ptr<Texture2D> AssetManager::LoadTexture(const std::string& path, bool forceReload)
{
    const std::string resolvedPath = ResolveAssetPath(path);
    if (resolvedPath.empty())
    {
        LogA(LogLevel::ERROR, "LoadTexture: invalid asset ref '{}'", path);
        return nullptr;
    }

    AssetDataBase::Get().RefreshMetaFromDisk(resolvedPath);
    const AssetMeta meta = AssetDataBase::Get().LoadOrCreateMeta(resolvedPath);
    const bool effectiveShowInUi = ShowInUiFromMeta(meta);

    std::shared_ptr<Texture2D> tex = std::make_shared<Texture2D>();
    if (LoaderManager::Get().LoadTextureFromFile(resolvedPath, tex, forceReload))
    {
        tex->m_showInUi = effectiveShowInUi;
        m_textures[resolvedPath] = tex;
    }
    else
        LogA(LogLevel::ERROR, "LoadTexture failed: {}", resolvedPath);
    return tex;
}

std::shared_ptr<Texture2D> AssetManager::CreateSolidColorTexture2D(const std::string& path, const glm::vec3& linearRgb)
{
    const std::string resolvedPath = ResolveAssetPath(path);
    if (resolvedPath.empty())
    {
        LogA(LogLevel::ERROR, "CreateSolidColorTexture2D: invalid path '{}'", path);
        return nullptr;
    }

    const auto existing = m_textures.find(resolvedPath);
    if (existing != m_textures.end())
        return existing->second;

    auto tex = std::make_shared<Texture2D>();
    const float pixel[3] = {linearRgb.r, linearRgb.g, linearRgb.b};
    TextureDesc desc = TextureDesc::MakeExplicit(1, 1, GL_RGB16F, GL_RGB, GL_FLOAT);
    desc.sampler.minFilter = GL_LINEAR;
    desc.sampler.magFilter = GL_LINEAR;
    desc.sampler.wrapS = GL_CLAMP_TO_EDGE;
    desc.sampler.wrapT = GL_CLAMP_TO_EDGE;
    if (!tex->Create(desc, pixel))
    {
        LogA(LogLevel::ERROR, "CreateSolidColorTexture2D failed: {}", resolvedPath);
        return nullptr;
    }

    tex->m_path = resolvedPath;
    tex->m_name = resolvedPath;
    tex->m_showInUi = false;
    m_textures[resolvedPath] = tex;
    return tex;
}

std::shared_ptr<IBLImage> AssetManager::LoadIBL(const std::string& path, bool forceReload)
{
    const std::string resolvedPath = ResolveAssetPath(path);
    if (resolvedPath.empty())
    {
        LogA(LogLevel::ERROR, "LoadIBL: invalid asset ref '{}'", path);
        return nullptr;
    }

    AssetDataBase::Get().RefreshMetaFromDisk(resolvedPath);
    const AssetMeta meta = AssetDataBase::Get().LoadOrCreateMeta(resolvedPath);
    if (meta.type != AssetType::TextureCube)
        LogA(LogLevel::WARNING, "LoadIBL: meta type is not cubemap for '{}'", resolvedPath);
    const bool effectiveShowInUi = ShowInUiFromMeta(meta);

    auto iblImage = std::make_shared<IBLImage>();
    if (LoaderManager::Get().LoadIBLMapFromFile(resolvedPath, iblImage, forceReload))
    {
        iblImage->m_showInUi = effectiveShowInUi;
        m_iblImages[resolvedPath] = iblImage;
    }
    else
    {
        LogA(LogLevel::ERROR, "Load IBL failed: {} (returning nullptr)", resolvedPath);
        return nullptr;
    }
    return iblImage;
}

bool AssetManager::IsAssetLoadedAtPath(const std::string& sourcePath) const
{
    const std::string path = NormalizeAssetKey(sourcePath);
    if (path.empty())
        return false;

    return m_shaders.contains(path) || m_computeShaders.contains(path) || m_textures.contains(path) ||
           m_models.contains(path) || m_iblImages.contains(path) || m_materials.contains(path);
}

#undef MODULE
