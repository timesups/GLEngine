#include "DirectionalLight.h"

#include <glm/gtc/matrix_transform.hpp>

#include "../../../Asset/AssetManager.h"
#include "../../../Asset/Types/Mesh.h"
#include "../../../Asset/Types/Model.h"
#include "../../../Asset/Types/Shader.h"
#include "../../../Core/LightingConvention.h"
#include "../../../Core/Log.h"
#include "../Transform.h"

#define MODULE "DirectionalLight"

void DirectionalLight::Init()
{
    Light::Init();
    m_arrowModel = AssetManager::Get().GetAsset<Model>("engine://model/arrow.fbx");
    m_lightMeshShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/ShowColor.glsl");
}

void DirectionalLight::Render()
{
    RenderBillboardIcon("engine://icon/sunLight.png");
    RenderArrow();
}

void DirectionalLight::RenderArrow()
{
    if (!m_lightMeshShader || !m_arrowModel || m_arrowModel->m_meshSections.empty() ||
        !m_arrowModel->m_meshSections[0].mesh)
        return;

    EnsureTransform();
    m_lightMeshShader->Use();
    m_lightMeshShader->SetValue("GL_MATRIX_M", TransformPtr()->GetModelMatrix());
    m_lightMeshShader->SetValue("_Color", GetColor());
    m_arrowModel->m_meshSections[0].mesh->Draw(GL_LINES);
}

float DirectionalLight::GetShaderRadianceScale() const
{
    return LightingConvention::DirectionalLuxToShaderLi(GetIntensity());
}

float DirectionalLight::GetIconBrightnessScale() const
{
    return GetShaderRadianceScale();
}

glm::mat4 DirectionalLight::GetProjectMatrix() const
{
    return glm::ortho(-m_shadow_area, m_shadow_area, -m_shadow_area, m_shadow_area, m_nearPlane, m_farPlane);
}

glm::mat4 DirectionalLight::GetViewMatrix(Direction /*direction*/) const
{
    if (!TransformPtr())
        return glm::mat4(1.f);
    return glm::mat4(glm::mat3(TransformPtr()->GetViewMatrix()));
}

#undef MODULE
