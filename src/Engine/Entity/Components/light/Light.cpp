#include "Light.h"

#include "../../../Asset/AssetManager.h"
#include "../../../Asset/Types/Mesh.h"
#include "../../../Asset/Types/Shader.h"
#include "../../../Asset/Types/Texture/Texture2D.h"
#include "../../../Core/Log.h"
#include "../../Entity.h"
#include "../Transform.h"

#define MODULE "Light"

void Light::Init()
{
    EnsureTransform();
    m_iconShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/Billboard.glsl");
    if (!m_iconShader)
        LogA(LogLevel::ERROR, "Failed to load icon shader");
}

void Light::Render() {}

Transform* Light::EnsureTransform()
{
    if (!m_transform)
    {
        m_transform = GetEntity()->GetComponent<Transform>();
        if (!m_transform)
            m_transform = GetEntity()->AddComponent<Transform>();
    }
    return m_transform;
}

void Light::RenderBillboardIcon(const char* iconPath)
{
    if (!m_iconShader || !iconPath || iconPath[0] == '\0')
        return;

    auto tex = AssetManager::Get().GetAsset<Texture>(iconPath);
    if (!tex)
    {
        LogA(LogLevel::ERROR, "Failed to get light icon: {}", iconPath);
        return;
    }

    EnsureTransform();
    m_iconShader->Use();
    tex->Bind(0);
    m_iconShader->SetValue("_iconMap", 0);
    m_iconShader->SetValue("_Color", glm::vec4(m_color * GetIconBrightnessScale(), 1.0f));
    m_iconShader->SetValue("GL_MATRIX_M", m_transform->GetModelMatrix());
    AssetManager::Get().GetScreenMesh()->Draw();
    tex->UnBind();
}

float Light::GetIntensity() const
{
    return m_intensity;
}

void Light::SetIntensity(float intensity)
{
    m_intensity = intensity;
}

glm::vec3 Light::GetColor() const
{
    return m_color;
}

void Light::SetColor(const glm::vec3 color)
{
    m_color = color;
}

void Light::SetColor(float r, float g, float b)
{
    m_color = glm::vec3(r, g, b);
}

const glm::vec3& Light::GetDirection() const
{
    static const glm::vec3 kFallback(1.f, 0.f, 0.f);
    return m_transform ? m_transform->GetForwardVector() : kFallback;
}

glm::vec3 Light::GetPosition()
{
    return m_transform ? m_transform->GetPosition() : glm::vec3(0.f, 0.f, 0.f);
}

float Light::GetShaderRadianceScale() const
{
    return GetIntensity();
}

float Light::GetIconBrightnessScale() const
{
    return GetIntensity();
}

glm::mat4 Light::GetProjectMatrix() const
{
    return glm::mat4(1.f);
}

glm::mat4 Light::GetViewMatrix(Direction /*direction*/) const
{
    return glm::mat4(1.f);
}

#undef MODULE
