#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include <glm/glm.hpp>

#include "../Entity/RenderUnitFilter.h"

class Mesh;
class Material;
struct MeshSection;

struct RenderUnit;

struct GPUInstanceData
{
    glm::mat4 mModel;
    glm::mat4 mNormal;
    /// xyz = world-space AABB; w 保留对齐，与 GLSL std430 一致
    glm::vec4 boundingBoxMax{0.f};
    glm::vec4 boundingBoxMin{0.f};
};

static_assert(sizeof(GPUInstanceData) == 160, "GPUInstanceData must match GLSL InstanceData (std430)");

inline GPUInstanceData MakeGPUInstanceData(const glm::mat4& model, const glm::mat4& normal, const glm::vec3& bboxMax,
                                           const glm::vec3& bboxMin)
{
    GPUInstanceData data{};
    data.mModel = model;
    data.mNormal = normal;
    data.boundingBoxMax = glm::vec4(bboxMax, 0.f);
    data.boundingBoxMin = glm::vec4(bboxMin, 0.f);
    return data;
}

struct BatchKey
{
    Mesh* mesh = nullptr;
    Material* material = nullptr;
    size_t sectionIndex = 0;

    bool operator==(const BatchKey& other) const
    {
        return mesh == other.mesh && material == other.material && sectionIndex == other.sectionIndex;
    }
};

struct BatchKeyHash
{
    size_t operator()(const BatchKey& key) const noexcept
    {
        size_t h = std::hash<void*>{}(key.mesh);
        h ^= std::hash<void*>{}(key.material) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<size_t>{}(key.sectionIndex) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

struct DrawBatch
{
    BatchKey key;
    MeshSection* section = nullptr;
    std::vector<GPUInstanceData> instances;
};

std::vector<DrawBatch> BuildDrawBatches(const std::vector<RenderUnit>& renderUnits, int minQueueInclusive,
                                        int maxQueueExclusive, Material* materialOverride,
                                        std::function<Material*(const RenderUnit&)> resolveMaterial,
                                        std::function<GPUInstanceData(const RenderUnit&)> resolveTransform,
                                        RenderUnitFilter filter = RenderUnitFilter::None());
