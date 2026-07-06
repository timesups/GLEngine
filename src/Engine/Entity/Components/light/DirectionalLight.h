#pragma once
#include "Light.h"

class Model;
class Shader;

class DirectionalLight : public Light
{
  public:
    LightType GetType() const override
    {
        return LightType::Directional;
    }

    void Init() override;
    void Render() override;

    float GetShaderRadianceScale() const override;
    float GetIconBrightnessScale() const override;
    glm::mat4 GetProjectMatrix() const override;
    glm::mat4 GetViewMatrix(Direction direction = Direction::RIGHT) const override;

    float m_shadow_area = 20.0f;

  private:
    void RenderArrow();

    std::shared_ptr<Model> m_arrowModel;
    std::shared_ptr<Shader> m_lightMeshShader;
};
