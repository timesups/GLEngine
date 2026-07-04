#include "SkyBox.h"
#include "../../Asset/AssetManager.h"
#include "../../Asset/Types/Mesh.h"
#include "../../Asset/Types/Shader.h"
#include "../../Asset/Types/Texture/IBLImage.h"
#include "../../Asset/Types/Texture/TextureCube.h"
#include "../../Core/Log.h"
#include <memory>

#define MODULE "SkyBox"

namespace
{
std::shared_ptr<TextureCube> SelectSkyboxCubemap(const std::shared_ptr<IBLImage>& ibl, SkyBox::CubemapPreview preview)
{
    if (!ibl)
        return nullptr;
    if (preview == SkyBox::CubemapPreview::Irradiance)
        return ibl->irradiance;
    return ibl->prefiltered;
}
} // namespace

SkyBox::SkyBox() : m_cubeMesh(std::make_unique<Mesh>()) {}

SkyBox::~SkyBox() = default;

void SkyBox::Init()
{
    static const glm::vec3 corners[8] = {
        {-1.0f, 1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f},   {1.0f, -1.0f, 1.0f},  {1.0f, 1.0f, 1.0f},
    };

    static const unsigned int skyboxIndices[36] = {
        0, 1, 2, 2, 3, 0, 4, 1, 0, 0, 5, 4, 2, 6, 7, 7, 3, 2, 4, 5, 7, 7, 6, 4, 0, 3, 7, 7, 5, 0, 1, 4, 2, 2, 4, 6,
    };

    LogA(LogLevel::INFO, "Init skybox mesh (8 vertices, 36 indices)");

    std::vector<unsigned int> index(std::begin(skyboxIndices), std::end(skyboxIndices));

    std::vector<Vertex> vertexs;
    vertexs.reserve(8);
    for (const glm::vec3& p : corners)
    {
        Vertex ver;
        ver.Position = p;
        ver.Color = glm::vec3(0, 0, 0);
        ver.Normal = glm::vec3(0, 0, -1);
        ver.Tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        ver.Texcoord0 = glm::vec2(0, 0);
        ver.Texcoord1 = glm::vec2(0, 0);
        vertexs.push_back(ver);
    }
    m_cubeMesh->Create(vertexs, index, Bounds());

    m_skyShader = AssetManager::Get().LoadShader("engine://shaders/SkyBox.glsl");
    if (!m_skyShader)
    {
        LogA(LogLevel::ERROR, "SkyBox shader load returned null (engine://shaders/SkyBox.glsl)");
        return;
    }
    if (m_skyShader->m_passes.empty() || !m_skyShader->m_passes[0])
    {
        LogA(LogLevel::ERROR, "SkyBox shader has no valid pass[0] (path={})", m_skyShader->m_path);
        return;
    }
    if (!m_ibl || !m_ibl->prefiltered)
        LogA(LogLevel::WARNING, "SkyBox has no IBL assigned yet; Render will be skipped");
}

void SkyBox::Update(float deltaTime) {}

void SkyBox::SetIBL(std::shared_ptr<IBLImage>& ibl)
{
    if (!ibl || !ibl->prefiltered || !ibl->irradiance)
    {
        LogA(LogLevel::WARNING, "SetIBL called with null or incomplete IBL (need prefiltered + irradiance)");
        return;
    }
    m_ibl = ibl;
}

void SkyBox::SetCubemapPreview(CubemapPreview preview)
{
    m_cubemapPreview = preview;
}

SkyBox::CubemapPreview SkyBox::GetCubemapPreview() const
{
    return m_cubemapPreview;
}

float SkyBox::GetBrightness() const
{
    return m_skyboxBrightness;
}

void SkyBox::SetBrightness(float brightness)
{
    m_skyboxBrightness = brightness;
}

bool SkyBox::IsIBLLightingEnabled() const
{
    return m_iblLightingEnabled;
}

void SkyBox::SetIBLLightingEnabled(bool enabled)
{
    m_iblLightingEnabled = enabled;
}

float SkyBox::GetIBLLightingIntensity() const
{
    return m_iblLightingIntensity;
}

void SkyBox::SetIBLLightingIntensity(float intensity)
{
    m_iblLightingIntensity = intensity;
}

bool SkyBox::IsDrawBackgroundEnabled() const
{
    return m_drawBackground;
}

void SkyBox::SetDrawBackgroundEnabled(bool enabled)
{
    m_drawBackground = enabled;
}

std::shared_ptr<IBLImage>& SkyBox::GetIBL()
{
    return m_ibl;
}

void SkyBox::Render()
{
    if (!m_drawBackground)
        return;

    static bool s_loggedInvalid = false;
    const std::shared_ptr<TextureCube> previewCube = SelectSkyboxCubemap(m_ibl, m_cubemapPreview);
    if (!m_ibl || !previewCube || !m_skyShader || m_skyShader->m_passes.empty() || !m_skyShader->m_passes[0])
    {
        if (!s_loggedInvalid)
        {
            LogA(LogLevel::WARNING,
                 "SkyBox::Render skipped: ibl={} previewCube={} preview={} skyShader={} passCount={}",
                 m_ibl ? "ok" : "null", previewCube ? "ok" : "null",
                 m_cubemapPreview == CubemapPreview::Irradiance ? "irradiance" : "prefiltered",
                 m_skyShader ? "ok" : "null", m_skyShader ? static_cast<int>(m_skyShader->m_passes.size()) : 0);
            s_loggedInvalid = true;
        }
        return;
    }

    m_skyShader->Use();
    m_skyShader->SetValue("_skyBoxBrightness", GetBrightness());
    previewCube->Bind(0);
    m_skyShader->m_passes[0]->SetValue("skybox", 0);
    m_cubeMesh->Draw(GL_TRIANGLES);
    previewCube->UnBind();
}

#undef MODULE
