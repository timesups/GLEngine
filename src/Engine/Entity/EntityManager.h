#pragma once
#include "../Asset/Types/RenderQueue.h"
#include "Entity.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class RenderContext;
class Material;
struct IBLImage;
class Model;
class MeshRender;

enum class LightType;

struct RenderUnit
{
    MeshRender* meshRenser;
    size_t sectionIndex;
    int renderQueue = RenderQueue::Geometry;
};

#include "RenderUnitFilter.h"

class EntityManager
{
  public:
    static EntityManager& Get();
    ~EntityManager();
    EntityManager(const EntityManager&) = delete;
    EntityManager& operator=(const EntityManager&) = delete;

    std::shared_ptr<Entity> CreateEntity(const std::string& name);
    std::shared_ptr<Entity> CreateCameraEntity(const std::string& name);
    std::shared_ptr<Entity> CreateMeshRenderEntity(const std::string& name, std::shared_ptr<Model> model);
    std::shared_ptr<Entity> CreateMeshRenderEntity(const std::string& name, const std::string& modelPath);
    std::shared_ptr<Entity> CreateSkyBoxEntity(const std::string& name, std::shared_ptr<IBLImage> ibl);
    std::shared_ptr<Entity> CreateLight(const std::string& name, LightType type);
    std::vector<std::string> GetAllEntityNames();

    void GatherSceneRenderUnit(RenderContext& context);
    void DrawRenderQueue(int minQueueInclusive, int maxQueueExclusive,
                         std::shared_ptr<Material> materialOverride = nullptr,
                         RenderUnitFilter filter = RenderUnitFilter::None(),
                         const std::string& lightMode = "");
    void DrawIcons();

    void DestroyEntity(Entity* entity);
    void DestroyEntity(const std::shared_ptr<Entity>& entity);
    void ClearScene();
    void NewEmptyScene();
    /// 供 RenderContext 注册，删除实体时同步更新当前相机
    void SetCurrentCameraRef(std::shared_ptr<Entity>* cameraRef);
    void Init();
    void Update(float deltaTime);
    const std::vector<std::shared_ptr<Entity>>& GetEntities() const;
    size_t GetEntityCount() const;

  public:
    std::vector<std::shared_ptr<Entity>> m_Lights;
    std::shared_ptr<Entity> skyBox = nullptr;
    std::vector<RenderUnit> renderUnits;

  private:
    void RenderSkyBoxIfPresent();
    void CleanupEntityReferences(const std::shared_ptr<Entity>& entity);
    EntityManager();
    std::vector<std::shared_ptr<Entity>> m_entities;
    std::shared_ptr<Entity>* m_currentCameraRef = nullptr;
};
