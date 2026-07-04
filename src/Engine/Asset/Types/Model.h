#pragma once

#include "Mesh.h"

#include <memory>
#include <string>
#include <vector>

class Material;

struct SourceMaterialInfo
{
    std::string name;
    std::string diffusePath;
};

class Model
{
  public:
    Model();
    void AddSection(std::shared_ptr<Mesh> mesh, int materialIndex);
    void addMaterial(std::shared_ptr<Material> mat);

  public:
    std::string m_name;
    std::string m_path;
    bool m_showInUi = true;
    std::vector<MeshSection> m_meshSections;
    std::vector<std::shared_ptr<Material>> m_materials;
    std::vector<SourceMaterialInfo> m_sourceMaterials;
    Bounds m_bounds;
};
