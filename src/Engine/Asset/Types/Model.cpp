#include "Model.h"

#include "Material.h"

Model::Model() = default;

void Model::AddSection(std::shared_ptr<Mesh> mesh, int materialIndex)
{
    MeshSection section;
    section.mesh = mesh;
    section.materialIndex = materialIndex;
    section.name = mesh->m_name;
    if (section.name.empty() && materialIndex >= 0 &&
        static_cast<size_t>(materialIndex) < m_sourceMaterials.size())
        section.name = m_sourceMaterials[static_cast<size_t>(materialIndex)].name;
    m_meshSections.push_back(section);

    glm::vec3 max_point = glm::max(mesh->m_bounds.GetMaxPoint(), m_bounds.GetMaxPoint());
    glm::vec3 min_point = glm::min(mesh->m_bounds.GetMinPoint(), m_bounds.GetMinPoint());
    m_bounds.FromMaxMinPoints(max_point, min_point);
}

void Model::addMaterial(std::shared_ptr<Material> mat)
{
    m_materials.push_back(mat);
}
