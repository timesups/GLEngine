#include "LocalLight.h"

#include <glm/gtc/matrix_transform.hpp>

#include "../../../Core/LightingConvention.h"
#include "../Transform.h"

float LocalLight::GetShaderRadianceScale() const
{
    return GetIntensity();
}

float LocalLight::GetIconBrightnessScale() const
{
    return GetIntensity() / LightingConvention::PointLightBaseline::kDefaultPointLumens;
}

glm::mat4 LocalLight::GetProjectMatrix() const
{
    return glm::perspective(glm::radians(90.0f), 1.0f, m_nearPlane, m_farPlane);
}

glm::mat4 LocalLight::GetViewMatrix(Direction direction) const
{
    if (!TransformPtr())
        return glm::mat4(1.f);

    const glm::vec3 lightPos = TransformPtr()->GetPosition();
    switch (direction)
    {
    case Direction::RIGHT:
        return glm::lookAt(lightPos, lightPos + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
    case Direction::LEFT:
        return glm::lookAt(lightPos, lightPos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
    case Direction::TOP:
        return glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
    case Direction::BOTTOM:
        return glm::lookAt(lightPos, lightPos + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0));
    case Direction::FRONT:
        return glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0));
    case Direction::BACK:
        return glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0));
    }
    return glm::mat4(1.f);
}
