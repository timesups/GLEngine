#include "Light.h"

#include <glm/gtc/matrix_transform.hpp>

#include "../../Asset/AssetManager.h"
#include "../../Asset/Types/Mesh.h"
#include "../../Asset/Types/Model.h"
#include "../../Asset/Types/Shader.h"
#include "../../Asset/Types/Texture/Texture2D.h"
#include "../../Core/LightingConvention.h"
#include "../../Core/Log.h"
#include "../Entity.h"
#include "glm/detail/qualifier.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"
#include "Transform.h"
#include <cstdint>
#include <string>

#define MODULE "Light"
void Light::Init()
{
    m_transform = GetEntity()->GetComponent<Transform>();
    if (!m_transform)
        m_transform = GetEntity()->AddComponent<Transform>();
    m_iconShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/Billboard.glsl");
    if (!m_iconShader)
    {
        LogA(LogLevel::ERROR, "Failed to load icon shader");
    }
    arrowModel = AssetManager::Get().GetAsset<Model>("engine://model/arrow.fbx");
    m_lightMeshShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/ShowColor.glsl");
}
void Light::Render()
{
    if (!m_iconShader)
        return;
    std::string iconPath;
    switch (type)
    {
    case LightType::PointLight:
        iconPath = "engine://icon/pointLight.png";
        break;
    case LightType::Directional:
        iconPath = "engine://icon/sunLight.png";
        RenderDirectionalLight();
        break;
    case LightType::SkyLight:
        iconPath = "engine://icon/skyLight.png";
        break;
    case LightType::SpotLight:
        iconPath = "engine://icon/spotLight.png";
        break;
    default:
        return;
    }
    if (iconPath.empty())
        return;

    auto tex = AssetManager::Get().GetAsset<Texture>(iconPath);
    if (!tex)
    {
        LogA(LogLevel::ERROR, "Faild to get light icon");
        return;
    }

    m_iconShader->Use();
    tex->Bind(0);
    m_iconShader->SetValue("_iconMap", 0);
    m_iconShader->SetValue("_Color",
                           glm::vec4(m_color * GetIconBrightnessScale(), 1.0f));
    m_iconShader->SetValue("GL_MATRIX_M", m_transform->GetModelMatrix());
    AssetManager::Get().GetScreenMesh()->Draw();
    tex->UnBind();
}

void Light::RenderDirectionalLight()
{
    m_lightMeshShader->Use();
    m_lightMeshShader->SetValue("GL_MATRIX_M",m_transform->GetModelMatrix());
    m_lightMeshShader->SetValue("_Color",GetColor());
    arrowModel->m_meshSections[0].mesh->Draw(GL_LINES);    
}
float Light::GetIntensity() const
{
    return m_intensity;
}
void Light::SetIntensity(float intensity)
{
    m_intensity = intensity;
}
float Light::GetShaderRadianceScale() const
{
    if (type == LightType::Directional)
        return LightingConvention::DirectionalLuxToShaderLi(m_intensity);
    if (type == LightType::PointLight || type == LightType::SpotLight)
        return m_intensity; // lm；shader 应用 1/(4π·d²)
    return m_intensity;
}
float Light::GetIconBrightnessScale() const
{
    if (type == LightType::Directional)
        return GetShaderRadianceScale();
    if (type == LightType::PointLight || type == LightType::SpotLight)
        return m_intensity / LightingConvention::PointLightBaseline::kDefaultPointLumens;
    return m_intensity;
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

glm::mat4 Light::GetProjectMatrix() const
{
    if (type == LightType::Directional)
    {
        return glm::ortho(-m_shadow_area, m_shadow_area, -m_shadow_area, m_shadow_area, m_nearPlane, m_farPlane);
    }
    else
    {
        return glm::perspective(glm::radians(90.0f), 1.0f, m_nearPlane, m_farPlane);
    }
}
glm::mat4 Light::GetViewMatrix(Direction direction) const
{
    if (!m_transform)
        return glm::mat4(1.f);
    if (type == LightType::Directional)
    {
        return glm::mat4(glm::mat3(m_transform->GetViewMatrix()));
    }
    else
    {
        glm::vec3 lightPos = m_transform->GetPosition();
        switch (direction)
        {
        case Direction::RIGHT:
            return glm::lookAt(lightPos, lightPos + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
            break;
        case Direction::LEFT:
            return glm::lookAt(lightPos, lightPos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
            break;
        case Direction::TOP:
            return glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
            break;
        case Direction::BOTTOM:
            return glm::lookAt(lightPos, lightPos + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0));
            break;
        case Direction::FRONT:
            return glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0));
            break;
        case Direction::BACK:
            return glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0));
            break;
        }
    }
}

#undef MODULE