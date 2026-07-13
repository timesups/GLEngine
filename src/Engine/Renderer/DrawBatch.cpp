#include "DrawBatch.h"

#include "../Asset/Types/Material.h"
#include "../Asset/Types/Mesh.h"
#include "../Asset/Types/Model.h"
#include "../Entity/Components/MeshRender.h"
#include "../Entity/Components/Ocean.h"
#include "../Entity/Entity.h"
#include "../Entity/EntityManager.h"

#include <unordered_map>

std::vector<DrawBatch> BuildDrawBatches(const std::vector<RenderUnit>& renderUnits, Material* materialOverride,
                                        std::function<Material*(const RenderUnit&)> resolveMaterial,
                                        std::function<GPUInstanceData(const RenderUnit&)> resolveTransform,
                                        RenderUnitFilter filter)
{
    std::unordered_map<BatchKey, DrawBatch, BatchKeyHash> map;
    map.reserve(renderUnits.size());

    for (const RenderUnit& unit : renderUnits)
    {
        if (!filter.Accepts(unit))
            continue;

        Material* mat = materialOverride ? materialOverride : resolveMaterial(unit);
        if (!mat)
            continue;

        MeshRender* meshRender = unit.meshRender;
        if (!meshRender || !meshRender->m_model)
            continue;
        if (unit.sectionIndex >= meshRender->m_model->m_meshSections.size())
            continue;

        MeshSection& section = meshRender->m_model->m_meshSections[unit.sectionIndex];
        if (!section.mesh)
            continue;

        Entity* splitEntity = nullptr;
        if (Entity* entity = meshRender->GetEntity())
        {
            if (meshRender->GetPerObjectRender() || entity->HasComponent<Ocean>())
                splitEntity = entity;
        }

        BatchKey key{section.mesh.get(), mat, unit.sectionIndex, splitEntity};

        DrawBatch& batch = map[key];
        if (!batch.section)
        {
            batch.key = key;
            batch.section = &section;
            batch.sourceEntity = splitEntity;
        }
        batch.instances.push_back(resolveTransform(unit));
    }

    std::vector<DrawBatch> result;
    result.reserve(map.size());
    for (auto& [_, batch] : map)
        result.push_back(std::move(batch));
    return result;
}
