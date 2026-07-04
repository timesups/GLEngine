#include "MeshRender.h"

#include "../../Asset/Types/Material.h"
#include "../../Asset/Types/Model.h"
#include "../../Asset/AssetManager.h"
#include "../../Core/Log.h"
#include "../Entity.h"
#include "Transform.h"


#define MODULE "MeshRender"

MeshRender::MeshRender(std::shared_ptr<Model> model)
{
    SetModel(model);
}

MeshRender::~MeshRender() = default;

void MeshRender::SetModel(std::shared_ptr<Model> model)
{
    if (!model)
    {
        LogA(LogLevel::WARNING, "SetModel called with null model on entity '{}'",
             GetEntity() ? GetEntity()->m_name : "?");
        m_model.reset();
        m_materialOverride.clear();
        return;
    }
    std::vector<std::shared_ptr<Material>> old_materials = m_materialOverride;
    m_materialOverride.clear();
    m_model = model;
    m_materialOverride.resize(model->m_meshSections.size());

    // 传递材质
    int count = glm::min(old_materials.size(), m_materialOverride.size());
    for (int i = 0; i < count; i++)
    {
        m_materialOverride[i] = old_materials[i];
    }

    m_bounds = m_model->m_bounds;
}

void MeshRender::Update(float deltaTime)
{
    // 重计算边界
    ReComputeBound();
}
void MeshRender::ReComputeBound()
{
    Transform* transform = GetEntity()->GetComponent<Transform>();
    if (!transform || !m_model)
        return;
    m_bounds = m_model->m_bounds;
    m_bounds.ApplyMatrix(transform->GetModelMatrix());
}
void MeshRender::RenderSection(const size_t index, std::shared_ptr<Material> mat)
{
    if (!m_model)
    {
        static bool s_loggedNoModel = false;
        if (!s_loggedNoModel)
        {
            LogA(LogLevel::WARNING, "RenderSection skipped: no model");
            s_loggedNoModel = true;
        }
        return;
    }
    if (index >= m_model->m_meshSections.size())
    {
        LogA(LogLevel::ERROR, "RenderSection index {} out of range (sections={})", index,
             m_model->m_meshSections.size());
        return;
    }
    if (!mat)
    {
        LogA(LogLevel::WARNING, "RenderSection index {} has null material", index);
        return;
    }
    Transform* transform = GetEntity()->GetComponent<Transform>();
    MaterialApplyData data;
    data.section = &m_model->m_meshSections[index];
    data.mModel = glm::mat4(1);
    data.mNormal = glm::mat4(1);
    if (transform)
    {
        data.mModel = transform->GetModelMatrix();
        data.mNormal = transform->GetNormalMatrix();
    }
    mat->Apply(data);
}
void MeshRender::RenderSection(const size_t index)
{
    RenderSection(index, GetMaterial(index));
}
std::shared_ptr<Material> MeshRender::GetMaterialOverride(const int index) const
{
    if (!m_model || index < 0 || static_cast<size_t>(index) >= m_materialOverride.size())
        return {};
    return m_materialOverride[index];
}

std::shared_ptr<Material> MeshRender::GetMaterial(const int index)
{
    std::shared_ptr<Material> mat = GetMaterialOverride(index);
    if (!mat && m_model && index >= 0 && static_cast<size_t>(index) < m_model->m_meshSections.size())
    {
        const int materialIndex = m_model->m_meshSections[static_cast<size_t>(index)].materialIndex;
        if (materialIndex >= 0 && static_cast<size_t>(materialIndex) < m_model->m_materials.size())
            mat = m_model->m_materials[static_cast<size_t>(materialIndex)];
    }
    if (!mat)
        mat = AssetManager::Get().GetAsset<Material>("engine://materials/DefaultMaterial");
    return mat;
}
void MeshRender::SetMaterial(const int index, std::shared_ptr<Material> mat)
{
    if (!m_model || index < 0)
        return;
    if (static_cast<size_t>(index) >= m_materialOverride.size())
        m_materialOverride.resize(static_cast<size_t>(index) + 1);
    m_materialOverride[index] = std::move(mat);
}

void MeshRender::ClearMaterialOverride(const int index)
{
    SetMaterial(index, nullptr);
}

const std::shared_ptr<Model>& MeshRender::GetModel() const
{
    return m_model;
}

#undef MODULE
