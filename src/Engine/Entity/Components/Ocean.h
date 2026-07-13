#pragma once

#include "../../Renderer/UniformBuffer.h"
#include "Component.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>

class Material;
class MeshRender;

inline constexpr int kMaxOceanWaves = 16;
inline constexpr GLuint kOceanWaveBufferBinding = 4;

struct alignas(16) OceanWaveGpu
{
    glm::vec4 params{};    // x=amplitude, y=wavelength, z=speed, w=steepness
    glm::vec4 direction{}; // xy=direction
};

struct alignas(16) OceanWaveBufferData
{
    glm::vec4 globals{}; // x=waveCount, y=gravity
    OceanWaveGpu waves[kMaxOceanWaves]{};
};

static_assert(sizeof(OceanWaveBufferData) == 16 + kMaxOceanWaves * 32,
              "OceanWaveBufferData must match ocean_wave_buffer std140 layout");

/// Gerstner 海面参数；与同一实体上的 MeshRender 配合，渲染前绑定 UBO 向 shader 传递波浪数据。
class Ocean : public Component
{
  public:
    struct GerstnerParams
    {
        float amplitude = 3.42f;
        float wavelength = 800.f;
        float speed = 21.42f;
        float steepness = 0.71f;
        glm::vec2 direction{-1.41f, -0.49f};
        float gravity = 9.8f;
    };

    Ocean();

    void Init() override;
    void Update(float deltaTime) override;

    void BindUniformBuffer();
    void RegenerateWaves();

    void SyncMaterialProperties();

    const GerstnerParams& GetParams() const
    {
        return m_params;
    }
    GerstnerParams& GetParams()
    {
        m_dirty = true;
        return m_params;
    }
    void SetParams(const GerstnerParams& params);

    int GetWaveCount() const
    {
        return m_waveCount;
    }
    void SetWaveCount(int count);

    std::uint32_t GetWaveSeed() const
    {
        return m_waveSeed;
    }
    void SetWaveSeed(std::uint32_t seed);

    int GetMaterialSlot() const
    {
        return m_materialSlot;
    }
    void SetMaterialSlot(int slot);

    bool GetAutoConfigureMeshRender() const
    {
        return m_autoConfigureMeshRender;
    }
    void SetAutoConfigureMeshRender(bool enabled);
    void ApplyDefaultMeshRenderSettings();

  private:
    MeshRender* GetMeshRender() const;
    std::shared_ptr<Material> GetTargetMaterial() const;
    void PushParamsToMaterial(Material& material) const;
    void UploadWaveBufferIfDirty();

    GerstnerParams m_params{};
    OceanWaveBufferData m_waveData{};
    UniformBuffer m_waveBuffer;
    int m_waveCount = 15;
    std::uint32_t m_waveSeed = 1;
    int m_materialSlot = 0;
    bool m_autoConfigureMeshRender = true;
    bool m_dirty = true;
    bool m_waveBufferDirty = true;
};
