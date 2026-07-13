#include "Ocean.h"

#include "../../Asset/Types/Material.h"
#include "../../Core/Log.h"
#include "../Entity.h"
#include "MeshRender.h"

#include <algorithm>
#include <cmath>
#include <random>

#define MODULE "Ocean"

Ocean::Ocean() : m_waveBuffer(sizeof(OceanWaveBufferData), GL_DYNAMIC_DRAW) {}

void Ocean::Init()
{
    if (!GetMeshRender())
    {
        LogA(LogLevel::WARNING, "Ocean on entity '{}' requires MeshRender on the same entity",
             GetEntity() ? GetEntity()->m_name : "?");
        return;
    }

    if (m_autoConfigureMeshRender)
        ApplyDefaultMeshRenderSettings();

    RegenerateWaves();
    m_dirty = true;
    SyncMaterialProperties();
}

void Ocean::Update(float deltaTime)
{
    (void)deltaTime;
    if (!m_dirty)
        return;

    SyncMaterialProperties();
    m_dirty = false;
}

void Ocean::BindUniformBuffer()
{
    UploadWaveBufferIfDirty();
    m_waveBuffer.BindBase(kOceanWaveBufferBinding);
}

void Ocean::RegenerateWaves()
{
    const int count = std::clamp(m_waveCount, 1, kMaxOceanWaves);
    m_waveData.globals.x = static_cast<float>(count);
    m_waveData.globals.y = m_params.gravity;

    std::mt19937 rng(m_waveSeed);
    std::uniform_real_distribution<float> wavelengthJitter(0.75f, 1.25f);
    std::uniform_real_distribution<float> amplitudeJitter(0.85f, 1.15f);
    std::uniform_real_distribution<float> directionJitter(-0.35f, 0.35f);

    glm::vec2 baseDirection = m_params.direction;
    if (glm::length(baseDirection) < 1e-4f)
        baseDirection = glm::vec2(1.f, 0.f);
    baseDirection = glm::normalize(baseDirection);

    for (int i = 0; i < count; ++i)
    {
        const float harmonic = static_cast<float>(i + 1);
        const float wavelength = (m_params.wavelength / harmonic) * wavelengthJitter(rng);
        const float amplitude = (m_params.amplitude / harmonic) * amplitudeJitter(rng);

        m_waveData.waves[i].params =
            glm::vec4(amplitude, wavelength, m_params.speed, m_params.steepness);

        const float angle = directionJitter(rng);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const glm::vec2 rotated{baseDirection.x * c - baseDirection.y * s, baseDirection.x * s + baseDirection.y * c};
        m_waveData.waves[i].direction = glm::vec4(glm::normalize(rotated), 0.f, 0.f);
    }

    for (int i = count; i < kMaxOceanWaves; ++i)
        m_waveData.waves[i] = {};

    m_waveBufferDirty = true;
}

void Ocean::SetParams(const GerstnerParams& params)
{
    m_params = params;
    m_dirty = true;
    RegenerateWaves();
}

void Ocean::SetWaveCount(int count)
{
    m_waveCount = std::clamp(count, 1, kMaxOceanWaves);
    RegenerateWaves();
}

void Ocean::SetWaveSeed(std::uint32_t seed)
{
    m_waveSeed = seed == 0 ? 1u : seed;
    RegenerateWaves();
}

void Ocean::SetMaterialSlot(int slot)
{
    if (slot < 0)
        return;
    m_materialSlot = slot;
    m_dirty = true;
}

void Ocean::SetAutoConfigureMeshRender(bool enabled)
{
    m_autoConfigureMeshRender = enabled;
}

void Ocean::ApplyDefaultMeshRenderSettings()
{
    MeshRender* meshRender = GetMeshRender();
    if (!meshRender)
        return;

    meshRender->SetDrawCustomDepth(true);
    meshRender->SetCastShadow(true);
    meshRender->SetPerObjectRender(true);
}

void Ocean::SyncMaterialProperties()
{
    const std::shared_ptr<Material> material = GetTargetMaterial();
    if (!material)
        return;

    PushParamsToMaterial(*material);
}

MeshRender* Ocean::GetMeshRender() const
{
    return GetEntity() ? GetEntity()->GetComponent<MeshRender>() : nullptr;
}

std::shared_ptr<Material> Ocean::GetTargetMaterial() const
{
    MeshRender* meshRender = GetMeshRender();
    if (!meshRender)
        return nullptr;
    return meshRender->GetMaterial(m_materialSlot);
}

void Ocean::PushParamsToMaterial(Material& material) const
{
    material.SetFloatProperty("_A1", m_params.amplitude);
    material.SetFloatProperty("_L1", m_params.wavelength);
    material.SetFloatProperty("_S1", m_params.speed);
    material.SetFloatProperty("_Q1", m_params.steepness);
    material.SetVec2Property("_D1", m_params.direction);
    material.SetFloatProperty("_gravity", m_params.gravity);
}

void Ocean::UploadWaveBufferIfDirty()
{
    if (!m_waveBufferDirty)
        return;

    m_waveBuffer.UploadStruct(m_waveData);
    m_waveBufferDirty = false;
}

#undef MODULE
