#include "SceneSerializer.h"

#include "SceneSession.h"

#include <filesystem>
#include <fstream>
#include <unordered_set>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "../Asset/AssetDatabase.h"
#include "../Asset/AssetManager.h"
#include "../Asset/AssetPaths.h"
#include "../Asset/Types/Material.h"
#include "../Asset/Types/Model.h"
#include "../Asset/Types/Texture/IBLImage.h"
#include "../Core/Log.h"
#include "../Core/LightingConvention.h"
#include "../Core/util.h"
#include "../Entity/Components/Camera.h"
#include "../Entity/Components/light/DirectionalLight.h"
#include "../Entity/Components/light/Light.h"
#include "../Entity/Components/light/LocalLight.h"
#include "../Entity/Components/MeshRender.h"
#include "../Entity/Components/light/SkyBox.h"
#include "../Entity/Components/Transform.h"
#include "../Entity/Entity.h"
#include "../Entity/EntityManager.h"
#include "../Renderer/RenderContext.h"

#define MODULE "SceneSerializer"

namespace
{
constexpr int kSceneVersion = 1;

nlohmann::json Vec3ToJson(const glm::vec3& value)
{
    return nlohmann::json::array({value.x, value.y, value.z});
}

nlohmann::json Vec4ToJson(const glm::vec4& value)
{
    return nlohmann::json::array({value.x, value.y, value.z, value.w});
}

glm::vec3 JsonToVec3(const nlohmann::json& json, const glm::vec3& fallback = glm::vec3(0.f))
{
    if (!json.is_array() || json.size() < 3)
        return fallback;
    return glm::vec3(json[0].get<float>(), json[1].get<float>(), json[2].get<float>());
}

glm::vec4 JsonToVec4(const nlohmann::json& json, const glm::vec4& fallback = glm::vec4(0.f))
{
    if (!json.is_array() || json.size() < 4)
        return fallback;
    return glm::vec4(json[0].get<float>(), json[1].get<float>(), json[2].get<float>(), json[3].get<float>());
}

std::string NormalizeScenePath(const std::string& path)
{
    return AssetDataBase::NormalizeSourcePath(path);
}

std::string AssetRefForSave(const std::string& path)
{
    if (path.empty())
        return {};
    return NormalizeScenePath(path);
}

std::string AssetRefForLoad(const std::string& ref)
{
    if (ref.empty())
        return {};
    const std::string resolved = AssetDataBase::Get().ResolveAssetRef(ref);
    if (!resolved.empty())
        return resolved;
    return NormalizeScenePath(ref);
}

std::string MaterialPathForSave(const std::shared_ptr<Material>& material)
{
    if (!material)
        return {};
    if (!material->m_sourcePath.empty())
        return AssetRefForSave(material->m_sourcePath);
    return {};
}

bool EnsureParentDirectory(const std::string& sourcePath, std::string& outMessage)
{
    const std::filesystem::path absolutePath = AssetPaths::Get().ToAbsolutePath(sourcePath);
    const std::filesystem::path parent = absolutePath.parent_path();
    if (parent.empty())
        return true;

    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec)
    {
        outMessage = "Failed to create directory: " + parent.generic_string();
        LogA(LogLevel::ERROR, "{}", outMessage);
        return false;
    }
    return true;
}

std::string TrimSceneName(std::string name)
{
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front())))
        name.erase(name.begin());
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
        name.pop_back();
    return name;
}

std::string ToLowerExtension(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

std::string MakeUniqueSceneFileName(const std::string& dirPath, const std::string& baseName)
{
    std::vector<std::string> existing;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dirPath, ec))
    {
        if (ec)
            break;
        if (!entry.is_regular_file(ec))
            continue;
        if (ToLowerExtension(entry.path()) == ".scene")
            existing.push_back(entry.path().stem().string());
    }

    const std::string uniqueStem =
        Util::GetUniqueName(baseName.empty() ? std::string("NewScene") : baseName, existing);
    return uniqueStem + ".scene";
}

const char* LightTypeToString(LightType type)
{
    switch (type)
    {
    case LightType::Directional:
        return "Directional";
    case LightType::PointLight:
        return "Point";
    case LightType::SpotLight:
        return "Spot";
    case LightType::SkyLight:
        return "Sky";
    }
    return "Directional";
}

bool LightTypeFromString(const std::string& token, LightType& outType)
{
    if (token == "Directional")
    {
        outType = LightType::Directional;
        return true;
    }
    if (token == "Point" || token == "PointLight")
    {
        outType = LightType::PointLight;
        return true;
    }
    if (token == "Spot" || token == "SpotLight")
    {
        outType = LightType::SpotLight;
        return true;
    }
    if (token == "Sky" || token == "SkyLight")
    {
        outType = LightType::SkyLight;
        return true;
    }
    return false;
}

nlohmann::json SerializeTransform(const Transform* transform)
{
    nlohmann::json json;
    if (!transform)
        return json;

    json["position"] = Vec3ToJson(transform->GetPosition());
    json["rotation"] = Vec3ToJson(transform->GetRotation());
    json["scale"] = Vec3ToJson(transform->GetScale());
    return json;
}

void ApplyTransform(Transform* transform, const nlohmann::json& json)
{
    if (!transform || !json.is_object())
        return;

    if (json.contains("position"))
        transform->SetPosition(JsonToVec3(json.at("position")));
    if (json.contains("rotation"))
        transform->SetRotation(JsonToVec3(json.at("rotation")));
    if (json.contains("scale"))
        transform->SetScale(JsonToVec3(json.at("scale"), glm::vec3(1.f)));
}

nlohmann::json SerializeMeshRender(const MeshRender* meshRender)
{
    nlohmann::json json;
    if (!meshRender || !meshRender->GetModel())
        return json;

    json["model"] = AssetRefForSave(meshRender->GetModel()->m_path);

    nlohmann::json materials = nlohmann::json::array();
    const int sectionCount = static_cast<int>(meshRender->GetModel()->m_meshSections.size());
    for (int slot = 0; slot < sectionCount; ++slot)
    {
        const std::shared_ptr<Material> overrideMat = meshRender->GetMaterialOverride(slot);
        if (!overrideMat)
            continue;

        const std::string materialPath = MaterialPathForSave(overrideMat);
        if (materialPath.empty())
            continue;

        materials.push_back({{"slot", slot}, {"material", materialPath}});
    }

    if (!materials.empty())
        json["materials"] = std::move(materials);

    const MeshRenderSetting& settings = meshRender->GetRenderSetting();
    if (settings.drawCustomDepth || !settings.castShadow)
    {
        json["renderSettings"] = {{"drawCustomDepth", settings.drawCustomDepth},
                                  {"castShadow", settings.castShadow}};
    }
    return json;
}

bool ApplyMeshRender(MeshRender* meshRender, const nlohmann::json& json, std::string& outMessage)
{
    if (!meshRender || !json.is_object())
        return false;

    if (!json.contains("model"))
    {
        outMessage = "MeshRender missing model reference.";
        return false;
    }

    const std::string modelPath = AssetRefForLoad(json.at("model").get<std::string>());
    if (modelPath.empty())
    {
        outMessage = "MeshRender has empty model reference.";
        return false;
    }

    auto model = AssetManager::Get().LoadModel(modelPath);
    if (!model)
    {
        outMessage = "Failed to load model: " + modelPath;
        return false;
    }

    meshRender->SetModel(std::move(model));

    if (!json.contains("materials") || !json.at("materials").is_array())
        return true;

    for (const nlohmann::json& entry : json.at("materials"))
    {
        if (!entry.is_object() || !entry.contains("slot") || !entry.contains("material"))
            continue;

        const int slot = entry.at("slot").get<int>();
        const std::string materialPath = AssetRefForLoad(entry.at("material").get<std::string>());
        if (materialPath.empty())
        {
            LogA(LogLevel::WARNING, "MeshRender slot {} skipped: empty material ref", slot);
            continue;
        }

        auto material = AssetManager::Get().LoadMaterial(materialPath);
        if (!material)
        {
            LogA(LogLevel::WARNING, "MeshRender slot {} skipped: failed to load '{}'", slot, materialPath);
            continue;
        }

        meshRender->SetMaterial(slot, std::move(material));
    }

    if (json.contains("renderSettings") && json.at("renderSettings").is_object())
    {
        const nlohmann::json& settings = json.at("renderSettings");
        if (settings.contains("drawCustomDepth"))
            meshRender->SetDrawCustomDepth(settings.at("drawCustomDepth").get<bool>());
        if (settings.contains("castShadow"))
            meshRender->SetCastShadow(settings.at("castShadow").get<bool>());
    }

    return true;
}

nlohmann::json SerializeCamera(const Camera* camera)
{
    nlohmann::json json;
    if (!camera)
        return json;

    json["fov"] = camera->GetFov();
    json["near"] = camera->GetNear();
    json["far"] = camera->GetFar();
    json["imaging"] = {
        {"aperture", camera->imaging.aperture},
        {"shutterSpeed", camera->imaging.shutterSpeed},
        {"iso", camera->imaging.iso},
        {"exposureCompensationEv", camera->imaging.exposureCompensationEv},
    };
    json["postSetting"] = {
        {"tonemap", Vec4ToJson(camera->postSetting.tonemap_setting)},
        {"bloom", Vec4ToJson(camera->postSetting.bloom_setting)},
        {"ssao", Vec4ToJson(camera->postSetting.sso_setting)},
        {"aoMode", Vec4ToJson(camera->postSetting.sso_extra)},
    };
    return json;
}

void ApplyCameraImaging(Camera* camera, const nlohmann::json& imagingJson)
{
    if (!camera || !imagingJson.is_object())
        return;

    if (imagingJson.contains("aperture"))
        camera->imaging.aperture = imagingJson.at("aperture").get<float>();
    if (imagingJson.contains("shutterSpeed"))
        camera->imaging.shutterSpeed = imagingJson.at("shutterSpeed").get<float>();
    if (imagingJson.contains("iso"))
        camera->imaging.iso = imagingJson.at("iso").get<float>();
    if (imagingJson.contains("exposureCompensationEv"))
        camera->imaging.exposureCompensationEv = imagingJson.at("exposureCompensationEv").get<float>();
}

void ApplyCamera(Camera* camera, const nlohmann::json& json)
{
    if (!camera || !json.is_object())
        return;

    if (json.contains("fov"))
        camera->SetFov(json.at("fov").get<float>());
    if (json.contains("near"))
        camera->SetNear(json.at("near").get<float>());
    if (json.contains("far"))
        camera->SetFar(json.at("far").get<float>());

    if (json.contains("postSetting") && json.at("postSetting").is_object())
    {
        const nlohmann::json& post = json.at("postSetting");
        if (post.contains("tonemap"))
            camera->postSetting.tonemap_setting =
                JsonToVec4(post.at("tonemap"), camera->postSetting.tonemap_setting);
        if (post.contains("bloom"))
            camera->postSetting.bloom_setting = JsonToVec4(post.at("bloom"), camera->postSetting.bloom_setting);
        if (post.contains("ssao"))
            camera->postSetting.sso_setting = JsonToVec4(post.at("ssao"), camera->postSetting.sso_setting);
        if (post.contains("aoMode"))
            camera->postSetting.sso_extra = JsonToVec4(post.at("aoMode"), camera->postSetting.sso_extra);
        else if (post.contains("ssaoExtra"))
        {
            glm::vec4 aoExtra = JsonToVec4(post.at("ssaoExtra"), camera->postSetting.sso_extra);
            // 旧版 ssaoExtra.x 为 enable 开关 (0/1)，迁移为 AO 算法选择 + intensity
            if (aoExtra.y == 0.f && aoExtra.z == 0.f && aoExtra.w == 0.f && (aoExtra.x == 0.f || aoExtra.x == 1.f))
            {
                if (aoExtra.x == 0.f)
                    camera->postSetting.sso_setting.w = 0.f;
                aoExtra.x = 0.f;
            }
            camera->postSetting.sso_extra = aoExtra;
        }
    }

    if (json.contains("imaging"))
    {
        ApplyCameraImaging(camera, json.at("imaging"));
    }
    else if (json.contains("postSetting") && json.at("postSetting").is_object() &&
             json.at("postSetting").contains("tonemap"))
    {
        const float legacyExposure = camera->postSetting.tonemap_setting.x;
        camera->imaging = {};
        camera->imaging.aperture = LightingConvention::CameraImagingBaseline::kReferenceAperture;
        camera->imaging.shutterSpeed = LightingConvention::CameraImagingBaseline::kReferenceShutterSpeed;
        camera->imaging.iso = LightingConvention::CameraImagingBaseline::kReferenceIso;
        if (legacyExposure > 1e-6f)
            camera->imaging.exposureCompensationEv = std::log2(legacyExposure);
    }

    camera->SyncExposureFromImaging();
}

nlohmann::json SerializeLight(const Light* light)
{
    nlohmann::json json;
    if (!light)
        return json;

    json["type"] = LightTypeToString(light->GetType());
    json["color"] = Vec3ToJson(light->GetColor());
    json["intensity"] = light->GetIntensity();
    json["nearPlane"] = light->m_nearPlane;
    json["farPlane"] = light->m_farPlane;
    json["bias"] = light->m_bias;
    json["pcfSample"] = light->m_pcfSample;
    json["shadowRes"] = static_cast<int>(light->m_ShadowRes);

    if (const DirectionalLight* directional = dynamic_cast<const DirectionalLight*>(light))
    {
        json["shadowArea"] = directional->m_shadow_area;
    }
    if (const LocalLight* local = dynamic_cast<const LocalLight*>(light))
    {
        json["inCutoff"] = local->m_inCutoff;
        json["outCutoff"] = local->m_outCutoff;
    }
    return json;
}

void ApplyLight(Light* light, const nlohmann::json& json)
{
    if (!light || !json.is_object())
        return;

    if (json.contains("color"))
        light->SetColor(JsonToVec3(json.at("color"), light->GetColor()));
    if (json.contains("intensity"))
        light->SetIntensity(json.at("intensity").get<float>());
    if (json.contains("nearPlane"))
        light->m_nearPlane = json.at("nearPlane").get<float>();
    if (json.contains("farPlane"))
        light->m_farPlane = json.at("farPlane").get<float>();
    if (json.contains("bias"))
        light->m_bias = json.at("bias").get<float>();
    if (json.contains("pcfSample"))
        light->m_pcfSample = json.at("pcfSample").get<int>();
    if (json.contains("shadowRes"))
        light->m_ShadowRes = static_cast<ShadowRes>(json.at("shadowRes").get<int>());

    if (DirectionalLight* directional = dynamic_cast<DirectionalLight*>(light))
    {
        if (json.contains("shadowArea"))
            directional->m_shadow_area = json.at("shadowArea").get<float>();
    }
    if (LocalLight* local = dynamic_cast<LocalLight*>(light))
    {
        if (json.contains("inCutoff"))
            local->m_inCutoff = json.at("inCutoff").get<float>();
        if (json.contains("outCutoff"))
            local->m_outCutoff = json.at("outCutoff").get<float>();
    }
}

nlohmann::json SerializeSkyBox(SkyBox* skyBox)
{
    nlohmann::json json;
    if (!skyBox)
        return json;

    if (std::shared_ptr<IBLImage>& ibl = skyBox->GetIBL(); ibl && !ibl->m_path.empty())
        json["ibl"] = AssetRefForSave(ibl->m_path);

    json["intensity"] = skyBox->GetIntensity();
    json["color"] = Vec3ToJson(skyBox->GetColor());
    json["brightness"] = skyBox->GetBrightness();
    json["iblLightingEnabled"] = skyBox->IsIBLLightingEnabled();
    json["iblLightingIntensity"] = skyBox->GetIBLLightingIntensity();
    json["drawBackground"] = skyBox->IsDrawBackgroundEnabled();
    json["cubemapPreview"] = static_cast<int>(skyBox->GetCubemapPreview());
    return json;
}

void ApplySkyBox(SkyBox* skyBox, const nlohmann::json& json)
{
    if (!skyBox || !json.is_object())
        return;

    if (json.contains("ibl"))
    {
        const std::string iblPath = AssetRefForLoad(json.at("ibl").get<std::string>());
        if (!iblPath.empty())
        {
            if (auto ibl = AssetManager::Get().LoadIBL(iblPath))
                skyBox->SetIBL(ibl);
        }
    }

    if (json.contains("intensity"))
        skyBox->SetIntensity(json.at("intensity").get<float>());
    if (json.contains("color"))
        skyBox->SetColor(JsonToVec3(json.at("color"), skyBox->GetColor()));
    if (json.contains("brightness"))
        skyBox->SetBrightness(json.at("brightness").get<float>());
    if (json.contains("iblLightingEnabled"))
        skyBox->SetIBLLightingEnabled(json.at("iblLightingEnabled").get<bool>());
    if (json.contains("iblLightingIntensity"))
        skyBox->SetIBLLightingIntensity(json.at("iblLightingIntensity").get<float>());
    if (json.contains("drawBackground"))
        skyBox->SetDrawBackgroundEnabled(json.at("drawBackground").get<bool>());
    if (json.contains("cubemapPreview"))
        skyBox->SetCubemapPreview(static_cast<SkyBox::CubemapPreview>(json.at("cubemapPreview").get<int>()));
}

bool EntityHasSerializableComponents(const std::shared_ptr<Entity>& entity)
{
    return entity->HasComponent<MeshRender>() || entity->HasComponent<Camera>() || entity->HasComponent<Light>() ||
           entity->HasComponent<SkyBox>();
}

nlohmann::json SerializeEntity(const std::shared_ptr<Entity>& entity)
{
    nlohmann::json json;
    json["name"] = entity->m_name;

    nlohmann::json components = nlohmann::json::object();
    if (const Transform* transform = entity->GetComponent<Transform>())
        components["Transform"] = SerializeTransform(transform);
    if (const MeshRender* meshRender = entity->GetComponent<MeshRender>())
    {
        nlohmann::json meshJson = SerializeMeshRender(meshRender);
        if (!meshJson.empty())
            components["MeshRender"] = std::move(meshJson);
    }
    if (const Camera* camera = entity->GetComponent<Camera>())
        components["Camera"] = SerializeCamera(camera);
    if (SkyBox* skyBox = entity->GetComponent<SkyBox>())
        components["SkyBox"] = SerializeSkyBox(skyBox);
    else if (const Light* light = entity->GetComponent<Light>())
        components["Light"] = SerializeLight(light);

    if (!components.empty())
        json["components"] = std::move(components);
    return json;
}

bool DeserializeEntity(const nlohmann::json& json, std::string& outMessage)
{
    if (!json.is_object() || !json.contains("name"))
    {
        outMessage = "Invalid entity entry: missing name.";
        return false;
    }

    const std::string name = json.at("name").get<std::string>();
    if (name.empty())
    {
        outMessage = "Invalid entity entry: empty name.";
        return false;
    }

    if (!json.contains("components") || !json.at("components").is_object())
        return true;

    const nlohmann::json& components = json.at("components");
    const bool hasSkyBox = components.contains("SkyBox");
    const bool hasCamera = components.contains("Camera");
    const bool hasLight = components.contains("Light");
    const bool hasMeshRender = components.contains("MeshRender");
    const bool hasTransform = components.contains("Transform");

    if (!hasSkyBox && !hasCamera && !hasLight && !hasMeshRender && !hasTransform)
        return true;

    std::shared_ptr<Entity> entity;
    if (hasSkyBox)
    {
        if (!components.at("SkyBox").is_object())
        {
            outMessage = "Entity '" + name + "' has invalid SkyBox data.";
            return false;
        }

        std::string iblPath;
        if (components.at("SkyBox").contains("ibl"))
            iblPath = AssetRefForLoad(components.at("SkyBox").at("ibl").get<std::string>());

        std::shared_ptr<IBLImage> ibl;
        if (!iblPath.empty())
            ibl = AssetManager::Get().LoadIBL(iblPath);

        entity = EntityManager::Get().CreateSkyBoxEntity(name, ibl);
        ApplySkyBox(entity->GetComponent<SkyBox>(), components.at("SkyBox"));
    }
    else if (hasCamera)
    {
        entity = EntityManager::Get().CreateCameraEntity(name);
        ApplyCamera(entity->GetComponent<Camera>(), components.at("Camera"));
        if (hasTransform)
            ApplyTransform(entity->GetComponent<Transform>(), components.at("Transform"));
    }
    else if (hasLight)
    {
        LightType type = LightType::Directional;
        if (components.at("Light").is_object() && components.at("Light").contains("type"))
            LightTypeFromString(components.at("Light").at("type").get<std::string>(), type);

        entity = EntityManager::Get().CreateLight(name, type);
        ApplyLight(entity->GetComponent<Light>(), components.at("Light"));
        if (hasTransform)
            ApplyTransform(entity->GetComponent<Transform>(), components.at("Transform"));
    }
    else if (hasMeshRender)
    {
        if (!components.at("MeshRender").is_object() || !components.at("MeshRender").contains("model"))
        {
            outMessage = "Entity '" + name + "' has invalid MeshRender data.";
            return false;
        }

        const std::string modelPath =
            AssetRefForLoad(components.at("MeshRender").at("model").get<std::string>());
        entity = EntityManager::Get().CreateMeshRenderEntity(name, modelPath);

        if (hasTransform)
            ApplyTransform(entity->GetComponent<Transform>(), components.at("Transform"));

        MeshRender* meshRender = entity->GetComponent<MeshRender>();
        if (!meshRender)
        {
            outMessage = "Entity '" + name + "' failed to create MeshRender.";
            return false;
        }

        if (!ApplyMeshRender(meshRender, components.at("MeshRender"), outMessage))
            return false;
    }
    else
    {
        entity = EntityManager::Get().CreateEntity(name);
        if (hasTransform)
        {
            entity->AddComponent<Transform>();
            ApplyTransform(entity->GetComponent<Transform>(), components.at("Transform"));
        }
    }

    return true;
}

void EnsureDefaultCamera(RenderContext& context)
{
    if (context.currentCamera)
        return;

    auto camera = EntityManager::Get().CreateCameraEntity("MainCamera");
    context.currentCamera = camera;
    if (Transform* transform = camera->GetComponent<Transform>())
        transform->SetPosition(0.f, 0.f, 5.f);
}

void RestoreActiveCamera(const nlohmann::json& json, RenderContext& context)
{
    if (!json.contains("activeCamera"))
    {
        EnsureDefaultCamera(context);
        return;
    }

    const std::string activeCameraName = json.at("activeCamera").get<std::string>();
    if (activeCameraName.empty())
    {
        EnsureDefaultCamera(context);
        return;
    }

    for (const std::shared_ptr<Entity>& entity : EntityManager::Get().GetEntities())
    {
        if (entity->m_name != activeCameraName || !entity->HasComponent<Camera>())
            continue;
        context.currentCamera = entity;
        return;
    }

    EnsureDefaultCamera(context);
}

void CollectEntityDependencies(const nlohmann::json& components, std::vector<std::string>& dependencies,
                               std::unordered_set<std::string>& seen)
{
    auto addDependency = [&](const std::string& path)
    {
        const std::string normalized = AssetRefForSave(path);
        if (normalized.empty() || !seen.insert(normalized).second)
            return;
        dependencies.push_back(normalized);
    };

    if (!components.is_object())
        return;

    if (components.contains("MeshRender") && components.at("MeshRender").is_object())
    {
        const nlohmann::json& meshRender = components.at("MeshRender");
        if (meshRender.contains("model"))
            addDependency(meshRender.at("model").get<std::string>());

        if (meshRender.contains("materials") && meshRender.at("materials").is_array())
        {
            for (const nlohmann::json& materialEntry : meshRender.at("materials"))
            {
                if (materialEntry.is_object() && materialEntry.contains("material"))
                    addDependency(materialEntry.at("material").get<std::string>());
            }
        }
    }

    if (components.contains("SkyBox") && components.at("SkyBox").is_object())
    {
        const nlohmann::json& skyBox = components.at("SkyBox");
        if (skyBox.contains("ibl"))
            addDependency(skyBox.at("ibl").get<std::string>());
    }
}

std::vector<std::string> CollectSceneDependencies(const nlohmann::json& sceneJson)
{
    std::vector<std::string> dependencies;
    std::unordered_set<std::string> seen;

    if (!sceneJson.contains("entities") || !sceneJson.at("entities").is_array())
        return dependencies;

    for (const nlohmann::json& entityJson : sceneJson.at("entities"))
    {
        if (!entityJson.is_object() || !entityJson.contains("components"))
            continue;
        CollectEntityDependencies(entityJson.at("components"), dependencies, seen);
    }

    return dependencies;
}
} // namespace

bool SceneSerializer::CreateEmptySceneFile(const std::string& dirPath, const char* nameInput, std::string& outFilePath,
                                           std::string& outMessage)
{
    std::error_code ec;
    if (!std::filesystem::exists(dirPath, ec) || !std::filesystem::is_directory(dirPath, ec))
    {
        outMessage = "Target directory does not exist.";
        return false;
    }

    const std::string baseName = TrimSceneName(nameInput);
    const std::string fileName = MakeUniqueSceneFileName(dirPath, baseName);
    outFilePath = NormalizeScenePath((std::filesystem::path(dirPath) / fileName).generic_string());

    nlohmann::json sceneJson;
    sceneJson["version"] = kSceneVersion;
    sceneJson["name"] = std::filesystem::path(fileName).stem().string();
    sceneJson["entities"] = nlohmann::json::array();

    if (!EnsureParentDirectory(outFilePath, outMessage))
        return false;

    std::ofstream file(AssetPaths::Get().ToAbsolutePath(outFilePath));
    if (!file.is_open())
    {
        outMessage = "Failed to create scene file: " + outFilePath;
        return false;
    }

    try
    {
        file << sceneJson.dump(4);
    }
    catch (const std::exception& e)
    {
        outMessage = std::string("Failed to serialize scene JSON: ") + e.what();
        return false;
    }

    AssetDataBase::Get().LoadOrCreateMeta(outFilePath);
    outMessage = "Created scene file: " + outFilePath;
    LogA(LogLevel::INFO, "Asset browser: {}", outMessage);
    return true;
}

bool SceneSerializer::SaveScene(const std::string& path, const RenderContext& context, std::string& outMessage)
{
    if (path.empty())
    {
        outMessage = "No scene path set.";
        return false;
    }

    const std::string sourcePath = NormalizeScenePath(path);
    if (sourcePath.empty())
    {
        outMessage = "Invalid scene path.";
        return false;
    }

    if (!EnsureParentDirectory(sourcePath, outMessage))
        return false;

    nlohmann::json sceneJson;
    sceneJson["version"] = kSceneVersion;
    sceneJson["name"] = Util::GetNameFromPath(sourcePath);
    if (context.currentCamera)
        sceneJson["activeCamera"] = context.currentCamera->m_name;

    nlohmann::json entities = nlohmann::json::array();
    for (const std::shared_ptr<Entity>& entity : EntityManager::Get().GetEntities())
    {
        if (!EntityHasSerializableComponents(entity))
            continue;
        entities.push_back(SerializeEntity(entity));
    }
    sceneJson["entities"] = std::move(entities);

    const std::filesystem::path absolutePath = AssetPaths::Get().ToAbsolutePath(sourcePath);
    std::ofstream file(absolutePath);
    if (!file.is_open())
    {
        outMessage = "Failed to open scene file for writing: " + sourcePath;
        LogA(LogLevel::ERROR, "{}", outMessage);
        return false;
    }

    try
    {
        file << sceneJson.dump(4);
    }
    catch (const std::exception& e)
    {
        outMessage = std::string("Failed to serialize scene JSON: ") + e.what();
        LogA(LogLevel::ERROR, "{}", outMessage);
        return false;
    }

    AssetDataBase::Get().LoadOrCreateMeta(sourcePath);
    AssetDataBase::Get().UpdateDependencies(sourcePath, CollectSceneDependencies(sceneJson));

    SceneSession::Get().SetCurrentPath(sourcePath);
    SceneSession::Get().ClearDirty();

    outMessage = "Saved scene: " + sourcePath;
    LogA(LogLevel::INFO, "{}", outMessage);
    return true;
}

bool SceneSerializer::LoadScene(const std::string& path, RenderContext& context, std::string& outMessage)
{
    const std::string sourcePath = NormalizeScenePath(path);
    if (sourcePath.empty())
    {
        outMessage = "Invalid scene path.";
        return false;
    }

    std::error_code ec;
    const std::filesystem::path absolutePath = AssetPaths::Get().ToAbsolutePath(sourcePath);
    if (!std::filesystem::is_regular_file(absolutePath, ec))
    {
        outMessage = "Scene file does not exist: " + sourcePath;
        LogA(LogLevel::ERROR, "{}", outMessage);
        return false;
    }

    nlohmann::json sceneJson;
    try
    {
        std::ifstream file(absolutePath);
        file >> sceneJson;
    }
    catch (const std::exception& e)
    {
        outMessage = std::string("Failed to parse scene file: ") + e.what();
        LogA(LogLevel::ERROR, "{}", outMessage);
        return false;
    }

    const int version = sceneJson.value("version", 0);
    if (version != kSceneVersion)
    {
        outMessage = "Unsupported scene version: " + std::to_string(version);
        LogA(LogLevel::ERROR, "{}", outMessage);
        return false;
    }

    EntityManager::Get().ClearScene();

    if (sceneJson.contains("entities") && sceneJson.at("entities").is_array())
    {
        for (const nlohmann::json& entityJson : sceneJson.at("entities"))
        {
            if (!DeserializeEntity(entityJson, outMessage))
            {
                EntityManager::Get().ClearScene();
                EnsureDefaultCamera(context);
                return false;
            }
        }
    }

    RestoreActiveCamera(sceneJson, context);
    EntityManager::Get().Init();

    SceneSession::Get().SetCurrentPath(sourcePath);
    SceneSession::Get().ClearDirty();

    outMessage = "Loaded scene: " + sourcePath;
    LogA(LogLevel::INFO, "{}", outMessage);
    return true;
}

#undef MODULE
