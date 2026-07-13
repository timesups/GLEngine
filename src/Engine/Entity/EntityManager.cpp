#include "EntityManager.h"
#include "Components/Camera.h"
#include "Components/light/DirectionalLight.h"
#include "Components/light/Light.h"
#include "Components/light/PointLight.h"
#include "Components/light/SkyBox.h"
#include "Components/light/SpotLight.h"
#include "Components/MeshRender.h"
#include "Components/Ocean.h"
#include "Components/Transform.h"
#include "Entity.h"

#include <algorithm>

#include "../Asset/AssetManager.h"
#include "../Asset/Types/Material.h"
#include "../Asset/Types/Model.h"
#include "../Asset/Types/RenderQueue.h"
#include "../Asset/Types/Texture/IBLImage.h"
#include "../Core/LightingConvention.h"
#include "../Core/Log.h"
#include "../Core/util.h"
#include "../Renderer/DrawBatch.h"
#include "../Renderer/InstanceBuffer.h"
#include "../Renderer/RenderContext.h"

#define MODULE "EntityManager"

namespace
{
constexpr int kMinInstancesToBatch = 2;
}

EntityManager::EntityManager() = default;

EntityManager::~EntityManager() = default;

EntityManager& EntityManager::Get()
{
    static EntityManager instance;
    return instance;
}

std::shared_ptr<Entity> EntityManager::CreateEntity(const std::string& name)
{
    auto entity = std::make_shared<Entity>();
    entity->m_name = Util::GetUniqueName(name, GetAllEntityNames());
    m_entities.push_back(entity);
    return entity;
}

std::shared_ptr<Entity> EntityManager::CreateCameraEntity(const std::string& name)
{
    auto entity = CreateEntity(name);
    entity->AddComponent<Transform>();
    entity->AddComponent<Camera>();
    LogA(LogLevel::INFO, "CreateCameraEntity '{}'", name);
    return entity;
}

std::shared_ptr<Entity> EntityManager::CreateMeshRenderEntity(const std::string& name, std::shared_ptr<Model> model)
{
    if (!model)
        LogA(LogLevel::WARNING, "CreateMeshRenderEntity '{}' with null model", name);
    auto entity = CreateEntity(name);
    entity->AddComponent<Transform>();
    entity->AddComponent<MeshRender>(std::move(model));
    return entity;
}

std::shared_ptr<Entity> EntityManager::CreateMeshRenderEntity(const std::string& name, const std::string& modelPath)
{
    LogA(LogLevel::INFO, "CreateMeshRenderEntity '{}' model: {}", name, modelPath);
    auto model = AssetManager::Get().LoadModel(modelPath);
    if (!model || model->m_meshSections.empty())
        LogA(LogLevel::WARNING, "MeshRenderEntity '{}' has no mesh sections (model load may have failed)", name);
    return CreateMeshRenderEntity(name, std::move(model));
}

std::shared_ptr<Entity> EntityManager::CreateOceanEntity(const std::string& name, const std::string& modelPath,
                                                         const std::string& materialPath)
{
    auto entity = CreateMeshRenderEntity(name, modelPath);
    if (!entity)
        return entity;

    entity->AddComponent<Ocean>();
    if (!materialPath.empty())
    {
        if (auto material = AssetManager::Get().LoadMaterial(materialPath))
            entity->GetComponent<MeshRender>()->SetMaterial(0, std::move(material));
        else
            LogA(LogLevel::WARNING, "CreateOceanEntity '{}': failed to load material '{}'", name, materialPath);
    }

    LogA(LogLevel::INFO, "CreateOceanEntity '{}'", name);
    return entity;
}

std::shared_ptr<Entity> EntityManager::CreateSkyBoxEntity(const std::string& name, std::shared_ptr<IBLImage> ibl)
{
    if (skyBox)
    {
        LogA(LogLevel::WARNING, "CreateSkyBoxEntity skipped: skyBox already exists ('{}')", skyBox->m_name);
        return skyBox;
    }

    skyBox = CreateEntity(name);
    skyBox->AddComponent<SkyBox>();
    skyBox->GetComponent<SkyBox>()->SetIBL(ibl);
    LogA(LogLevel::INFO, "CreateSkyBoxEntity '{}", name);
    return skyBox;
}

std::shared_ptr<Entity> EntityManager::CreateLight(const std::string& name, LightType type)
{
    auto entity = CreateEntity(name);
    entity->AddComponent<Transform>();
    Light* light = nullptr;

    switch (type)
    {
    case LightType::Directional:
    {
        auto* directional = entity->AddComponent<DirectionalLight>();
        directional->m_ShadowRes = ShadowRes::L1024;
        directional->m_farPlane = 20.0f;
        directional->m_nearPlane = -20.0f;
        directional->m_bias = 0.002f;
        directional->SetIntensity(LightingConvention::DirectionalCalibrationBaseline::kDefaultDirectionalLux);
        light = directional;
        break;
    }
    case LightType::PointLight:
    {
        auto* point = entity->AddComponent<PointLight>();
        point->m_ShadowRes = ShadowRes::L1024;
        point->m_farPlane = 20.0f;
        point->m_inCutoff = 180.f;
        point->m_outCutoff = 180.f;
        point->m_pcfSample = 4;
        point->m_bias = 0.01f;
        point->SetIntensity(LightingConvention::PointLightBaseline::kDefaultPointLumens);
        light = point;
        break;
    }
    case LightType::SpotLight:
    {
        auto* spot = entity->AddComponent<SpotLight>();
        spot->m_ShadowRes = ShadowRes::L1024;
        spot->m_farPlane = 20.0f;
        spot->m_pcfSample = 4;
        spot->m_bias = 0.5f;
        spot->SetIntensity(LightingConvention::PointLightBaseline::kDefaultSpotLumens);
        light = spot;
        break;
    }
    case LightType::SkyLight:
        LogA(LogLevel::WARNING, "CreateLight(SkyLight) is deprecated; use CreateSkyBoxEntity instead");
        return entity;
    }

    if (light)
        m_Lights.push_back(entity);

    LogA(LogLevel::INFO, "CreateLight '{}' type={}", entity->m_name, static_cast<int>(type));
    return entity;
}

std::vector<std::string> EntityManager::GetAllEntityNames()
{
    std::vector<std::string> names;
    for (auto& entiey : m_entities)
    {
        names.push_back(entiey->m_name);
    }
    return names;
}

void EntityManager::GatherSceneRenderUnit(RenderContext& context)
{
    renderUnits.clear();

    for (auto& e : m_entities)
    {
        if (!e->HasComponent<MeshRender>())
            continue;

        auto meshRender = e->GetComponent<MeshRender>();
        if (!meshRender->m_model)
        {
            static bool s_loggedNoModel = false;
            if (!s_loggedNoModel)
            {
                LogA(LogLevel::WARNING, "GatherSceneRenderUnit: entity '{}' MeshRender has no model", e->m_name);
                s_loggedNoModel = true;
            }
            continue;
        }

        for (int i = 0; i < meshRender->m_model->m_meshSections.size(); ++i)
        {
            auto mat = meshRender->GetMaterial(i);
            if (!mat)
                continue;

            RenderUnit unit;
            unit.meshRender = meshRender;
            unit.sectionIndex = static_cast<size_t>(i);
            unit.renderQueue = mat->GetRenderQueue();
            renderUnits.push_back(unit);
        }
    }

    m_hasSortCamera = false;
    if (context.currentCamera)
    {
        if (Camera* cam = context.currentCamera->GetComponent<Camera>())
        {
            m_sortCameraPos = cam->GetPosition();
            m_sortViewForward = cam->GetForwardVector();
            m_hasSortCamera = true;
        }
    }
}

void EntityManager::SetCurrentCameraRef(std::shared_ptr<Entity>* cameraRef)
{
    m_currentCameraRef = cameraRef;
}

void EntityManager::ClearScene()
{
    m_entities.clear();
    m_Lights.clear();
    skyBox = nullptr;
    renderUnits.clear();
    if (m_currentCameraRef)
        *m_currentCameraRef = nullptr;
    LogA(LogLevel::INFO, "Scene cleared");
}

void EntityManager::NewEmptyScene()
{
    ClearScene();
    auto camera = CreateCameraEntity("MainCamera");
    if (m_currentCameraRef)
        *m_currentCameraRef = camera;
    if (Transform* transform = camera->GetComponent<Transform>())
        transform->SetPosition(0.f, 0.f, 5.f);
    LogA(LogLevel::INFO, "New empty scene created");
}

void EntityManager::CleanupEntityReferences(const std::shared_ptr<Entity>& entity)
{
    m_Lights.erase(std::remove(m_Lights.begin(), m_Lights.end(), entity), m_Lights.end());

    if (skyBox == entity)
        skyBox = nullptr;

    if (m_currentCameraRef && *m_currentCameraRef == entity)
    {
        *m_currentCameraRef = nullptr;
        for (const std::shared_ptr<Entity>& candidate : m_entities)
        {
            if (candidate == entity || !candidate->GetComponent<Camera>())
                continue;
            *m_currentCameraRef = candidate;
            break;
        }
    }
}

void EntityManager::DestroyEntity(Entity* entity)
{
    if (!entity)
    {
        LogA(LogLevel::WARNING, "DestroyEntity called with null pointer");
        return;
    }
    const auto it = std::find_if(m_entities.begin(), m_entities.end(),
                                 [entity](const std::shared_ptr<Entity>& p) { return p.get() == entity; });
    if (it == m_entities.end())
    {
        LogA(LogLevel::WARNING, "DestroyEntity: pointer not found in entity list");
        return;
    }
    LogA(LogLevel::INFO, "DestroyEntity '{}'", entity->m_name);
    CleanupEntityReferences(*it);
    m_entities.erase(it);
}

void EntityManager::DestroyEntity(const std::shared_ptr<Entity>& entity)
{
    if (!entity)
    {
        LogA(LogLevel::WARNING, "DestroyEntity(shared_ptr) called with empty ptr");
        return;
    }
    const auto it = std::find(m_entities.begin(), m_entities.end(), entity);
    if (it != m_entities.end())
    {
        LogA(LogLevel::INFO, "DestroyEntity '{}'", entity->m_name);
        CleanupEntityReferences(entity);
        m_entities.erase(it);
    }
    else
    {
        LogA(LogLevel::WARNING, "DestroyEntity: '{}' not found in entity list", entity->m_name);
    }
}

void EntityManager::Init()
{
    LogA(LogLevel::INFO, "EntityManager::Init ({} entities)", m_entities.size());
    for (auto& e : m_entities)
        e->Init();
    LogA(LogLevel::INFO, "Scene init done, {} entities", EntityManager::Get().GetEntityCount());
}

void EntityManager::Update(float deltaTime)
{
    for (auto& e : m_entities)
        e->Update(deltaTime);
}

void EntityManager::SortRenderUnitsForDraw(std::vector<RenderUnit>& units, DrawSortMode sortMode) const
{
    switch (sortMode)
    {
    case DrawSortMode::None:
        return;
    case DrawSortMode::RenderQueue:
        std::stable_sort(units.begin(), units.end(),
                         [](const RenderUnit& a, const RenderUnit& b) { return a.renderQueue < b.renderQueue; });
        return;
    case DrawSortMode::RenderQueueThenBackToFront:
        std::stable_sort(units.begin(), units.end(),
                         [this](const RenderUnit& a, const RenderUnit& b)
                         {
                             if (a.renderQueue != b.renderQueue)
                                 return a.renderQueue < b.renderQueue;
                             if (m_hasSortCamera && a.renderQueue >= RenderQueue::Transparent)
                             {
                                 const float depthA = glm::dot(a.meshRender->m_bounds.GetCenterPoint() - m_sortCameraPos,
                                                             m_sortViewForward);
                                 const float depthB = glm::dot(b.meshRender->m_bounds.GetCenterPoint() - m_sortCameraPos,
                                                             m_sortViewForward);
                                 return depthA > depthB;
                             }
                             return false;
                         });
        return;
    }
}

void EntityManager::DrawSkyBox()
{
    static bool s_loggedNoSkyBox = false;
    if (!skyBox)
    {
        if (!s_loggedNoSkyBox)
        {
            LogA(LogLevel::WARNING, "Skybox draw skipped: no skyBox entity (CreateSkyBoxEntity not called or failed)");
            s_loggedNoSkyBox = true;
        }
        return;
    }
    skyBox->Render();
}

void EntityManager::DrawRenderQueue(const DrawSetting& setting)
{
    Material* overridePtr = setting.GetMaterialOverride();
    const bool allowInstancing = setting.filter.AllowsInstancing();

    std::vector<RenderUnit> units = renderUnits;
    SortRenderUnitsForDraw(units, setting.sortMode);

    auto resolveMaterial = [&](const RenderUnit& unit) -> Material*
    { return unit.meshRender->GetMaterial(static_cast<int>(unit.sectionIndex)).get(); };

    auto resolveTransform = [&](const RenderUnit& unit) -> GPUInstanceData
    {
        Transform* transform = unit.meshRender->GetEntity()->GetComponent<Transform>();
        const glm::mat4 model = transform ? transform->GetModelMatrix() : glm::mat4(1.0f);
        const glm::mat4 normal = transform ? transform->GetNormalMatrix() : glm::mat4(1.0f);
        return MakeGPUInstanceData(model, normal, unit.meshRender->m_bounds.GetMaxPoint(),
                                   unit.meshRender->m_bounds.GetMinPoint());
    };

    std::vector<DrawBatch> batches =
        BuildDrawBatches(units, overridePtr, resolveMaterial, resolveTransform, setting.filter);

    InstanceBuffer& instanceBuffer = InstanceBuffer::Get();

    for (DrawBatch& batch : batches)
    {
        Material* mat = batch.key.material;
        if (!mat || !batch.section || batch.instances.empty())
            continue;

        if (batch.sourceEntity)
        {
            if (Ocean* ocean = batch.sourceEntity->GetComponent<Ocean>())
                ocean->BindUniformBuffer();
        }

        if (allowInstancing && static_cast<int>(batch.instances.size()) >= kMinInstancesToBatch)
        {
            const int allocId = instanceBuffer.Allocate(batch.instances);
            MaterialInstancedApplyData data{};
            data.section = batch.section;
            data.instanceOffset = instanceBuffer.GetBatchOffset(allocId);
            data.instanceCount = instanceBuffer.GetBatchCount(allocId);
            mat->ApplyInstanced(data, setting.passTags);
        }
        else
        {
            MaterialApplyData data{};
            data.section = batch.section;
            data.mModel = batch.instances[0].mModel;
            data.mNormal = batch.instances[0].mNormal;
            data.boundingBoxMax = glm::vec3(batch.instances[0].boundingBoxMax);
            data.boundingBoxMin = glm::vec3(batch.instances[0].boundingBoxMin);
            mat->Apply(data, setting.passTags);
        }
    }
}

void EntityManager::DrawIcons()
{
    for (auto& l : m_Lights)
    {
        l->GetComponent<Light>()->Render();
    }
}

const std::vector<std::shared_ptr<Entity>>& EntityManager::GetEntities() const
{
    return m_entities;
}

size_t EntityManager::GetEntityCount() const
{
    return m_entities.size();
}

#undef MODULE