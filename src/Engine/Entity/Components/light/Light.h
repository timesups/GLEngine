#pragma once
#include <glm/glm.hpp>
#include <memory>

#include "../../../Core/Config.h"
#include "../../../Core/LightingConvention.h"
#include "../Component.h"

enum class LightType
{
    Directional = 0,
    PointLight = 1,
    SpotLight = 2,
    SkyLight = 3,
};

enum class ShadowRes
{
    L512 = 512,
    L1024 = 1024,
    L2048 = 2048,
    L4096 = 4096
};

enum class Direction
{
    RIGHT = 0,
    LEFT,
    TOP,
    BOTTOM,
    FRONT,
    BACK
};

struct alignas(16) LocalLightData
{
    glm::vec3 Position;
    float inCutOff;
    glm::vec3 Direction;
    float outCutOff;
    glm::vec4 Color;
    glm::mat4 matrix[6];
    /*
    x:zNear
    y:zFar
    z:bias
    w:pcfSample
    */
    glm::vec4 param1;
};

struct alignas(16) LightInputData
{
    glm::vec4 _MainLightDir;
    glm::vec4 _MainLightColor;
    glm::vec3 _AmbientColor;
    float LocalLightCount;
    glm::mat4 _MainLightMatrix;
    /*
    x:zNear
    y:zFar
    z:IBL lighting enabled (0/1)
    w:IBL lighting intensity
    */
    glm::vec4 param1;
    float _MaxReflectionLOD = 4.0f;
    float _padIbl0 = 0.0f;
    float _padIbl1 = 0.0f;
    float _padIbl2 = 0.0f;
    LocalLightData _localLights[Config::MaxLocalLight];
};

class Transform;
class Shader;

class Light : public Component
{
  public:
    ~Light() override = default;

    virtual LightType GetType() const = 0;

    void Init() override;
    void Render() override;

    float GetIntensity() const;
    void SetIntensity(float intensity);
    glm::vec3 GetColor() const;
    void SetColor(const glm::vec3 color);
    void SetColor(float r, float g, float b);
    /// 与 Transform::GetForwardVector / 相机视线一致：光锥轴、定向光朝向
    const glm::vec3& GetDirection() const;
    glm::vec3 GetPosition();

    virtual float GetShaderRadianceScale() const;
    virtual float GetIconBrightnessScale() const;
    virtual glm::mat4 GetProjectMatrix() const;
    virtual glm::mat4 GetViewMatrix(Direction direction = Direction::RIGHT) const;

    float m_nearPlane = 0.01f;
    float m_farPlane = 7.5f;
    float m_bias = 0.0005f;
    int m_pcfSample = 1;
    ShadowRes m_ShadowRes = ShadowRes::L512;

  protected:
    void RenderBillboardIcon(const char* iconPath);
    Transform* EnsureTransform();
    Transform* TransformPtr() const
    {
        return m_transform;
    }

  private:
    /// 定向光：lux（lm/m²）；点光/聚光：lumen（lm）
    float m_intensity = 1.0f;
    glm::vec3 m_color = glm::vec3(1, 1, 1);
    Transform* m_transform = nullptr;
    std::shared_ptr<Shader> m_iconShader;
};
