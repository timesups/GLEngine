#pragma once
#include "../Asset/Types/RenderQueue.h"
#include "DrawSetting.h"
#include "Entity.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class RenderContext;
class Material;
struct IBLImage;
class Model;
class MeshRender;

enum class LightType;

struct RenderUnit
{
    MeshRender* meshRender = nullptr;
    size_t sectionIndex = 0;
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
    std::shared_ptr<Entity> CreateOceanEntity(const std::string& name, const std::string& modelPath,
                                              const std::string& materialPath);
    std::shared_ptr<Entity> CreateSkyBoxEntity(const std::string& name, std::shared_ptr<IBLImage> ibl);
    std::shared_ptr<Entity> CreateLight(const std::string& name, LightType type);
    std::vector<std::string> GetAllEntityNames();

    void GatherSceneRenderUnit(RenderContext& context);
    void DrawRenderQueue(const DrawSetting& setting = DrawSetting::Default());
    void DrawSkyBox();
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
    void CleanupEntityReferences(const std::shared_ptr<Entity>& entity);
    void SortRenderUnitsForDraw(std::vector<RenderUnit>& units, DrawSortMode sortMode) const;
    EntityManager();
    std::vector<std::shared_ptr<Entity>> m_entities;
    std::shared_ptr<Entity>* m_currentCameraRef = nullptr;
    glm::vec3 m_sortCameraPos{};
    glm::vec3 m_sortViewForward{0.f, 0.f, -1.f};
    bool m_hasSortCamera = false;
};
