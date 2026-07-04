#include "DrawBatch.h"

#include "../Asset/Types/Material.h"
#include "../Asset/Types/Mesh.h"
#include "../Asset/Types/Model.h"
#include "../Entity/Components/MeshRender.h"
#include "../Entity/EntityManager.h"

#include <unordered_map>

std::vector<DrawBatch> BuildDrawBatches(const std::vector<RenderUnit>& renderUnits, int minQueueInclusive,
                                        int maxQueueExclusive, Material* materialOverride,
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

        if (unit.renderQueue < minQueueInclusive || unit.renderQueue >= maxQueueExclusive)
            continue;

        Material* mat = materialOverride ? materialOverride : resolveMaterial(unit);
        if (!mat)
            continue;

        MeshRender* meshRender = unit.meshRenser;
        if (!meshRender || !meshRender->m_model)
            continue;
        if (unit.sectionIndex >= meshRender->m_model->m_meshSections.size())
            continue;

        MeshSection& section = meshRender->m_model->m_meshSections[unit.sectionIndex];
        if (!section.mesh)
            continue;

        BatchKey key{section.mesh.get(), mat, unit.sectionIndex};

        DrawBatch& batch = map[key];
        if (!batch.section)
        {
            batch.key = key;
            batch.section = &section;
        }
        batch.instances.push_back(resolveTransform(unit));
    }

    std::vector<DrawBatch> result;
    result.reserve(map.size());
    for (auto& [_, batch] : map)
        result.push_back(std::move(batch));
    return result;
}
