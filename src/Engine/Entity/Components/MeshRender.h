#pragma once
#include "../../Asset/Types/Mesh.h"
#include "Component.h"

#include <memory>
#include <vector>

class Model;
class Material;

struct MeshRenderSetting
{
    bool drawCustomDepth = false;
    bool drawOutline = false;
    bool castShadow = true;
    bool perObjectRender = false;
};

class MeshRender : public Component
{
  public:
    MeshRender(std::shared_ptr<Model> model);
    ~MeshRender();
    void SetModel(std::shared_ptr<Model> model);
    virtual void Update(float deltaTime) override;
    void ReComputeBound();
    void RenderSection(const size_t index, std::shared_ptr<Material> mat);
    void RenderSection(const size_t index);
    std::shared_ptr<Material> GetMaterial(const int index);
    std::shared_ptr<Material> GetMaterialOverride(const int index) const;
    void SetMaterial(const int index, std::shared_ptr<Material> mat);
    void ClearMaterialOverride(const int index);
    const std::shared_ptr<Model>& GetModel() const;

    const MeshRenderSetting& GetRenderSetting() const
    {
        return m_renderSetting;
    }
    MeshRenderSetting& GetRenderSetting()
    {
        return m_renderSetting;
    }
    bool GetDrawCustomDepth() const
    {
        return m_renderSetting.drawCustomDepth;
    }
    void SetDrawCustomDepth(bool value)
    {
        m_renderSetting.drawCustomDepth = value;
    }
    bool GetDrawOutline() const
    {
        return m_renderSetting.drawOutline;
    }
    void SetDrawOutline(bool value)
    {
        m_renderSetting.drawOutline = value;
    }
    bool GetCastShadow() const
    {
        return m_renderSetting.castShadow;
    }
    void SetCastShadow(bool value)
    {
        m_renderSetting.castShadow = value;
    }
    bool GetPerObjectRender() const
    {
        return m_renderSetting.perObjectRender;
    }
    void SetPerObjectRender(bool value)
    {
        m_renderSetting.perObjectRender = value;
    }

    std::shared_ptr<Model> m_model;

  public:
    Bounds m_bounds;

  private:
    std::vector<std::shared_ptr<Material>> m_materialOverride;
    MeshRenderSetting m_renderSetting;
};
