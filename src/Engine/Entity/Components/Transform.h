#pragma once
#include <glm/glm.hpp>

#include "Component.h"

class Transform : public Component
{
  public:
    Transform();
    // Set
    void SetPosition(const glm::vec3& value);
    void SetPosition(const float x, const float y, const float z);
    void SetRotation(const glm::vec3& value);
    void SetRotation(const float x, const float y, const float z);
    void SetScale(const glm::vec3& value);
    void SetScale(const float x, const float y, const float z);
    // Add
    void AddPosition(const glm::vec3& value);
    void AddPosition(float x, float y, float z);
    void AddRotation(const glm::vec3& value);
    void AddRotation(float x, float y, float z);
    // Get
    /// 与 GetViewMatrix / GetForwardVector 共用同一套朝向 basis（局部 -Z 为 forward）
    const glm::mat4& GetModelMatrix() const;
    /// 与 `lookAt(position, position + GetForwardVector(), GetUpVector())` 一致，供相机、灯光阴影等复用
    const glm::mat4& GetViewMatrix() const;
    const glm::mat4 GetNormalMatrix() const;
    const glm::vec3& GetPosition() const;
    const glm::vec3& GetRotation() const;
    const glm::vec3& GetScale() const;
    /// 与视图矩阵 lookAt 一致：rotation.x 俯仰、y 偏航、z 滚转（度）
    const glm::vec3& GetForwardVector() const;
    const glm::vec3& GetUpVector() const;
    const glm::vec3& GetRightVector() const;

  private:
    glm::vec3 ComputeViewForward() const;
    static glm::vec3 ComputeBaseRight(const glm::vec3& forward);
    void ComputeViewBasis(const glm::vec3& forward, glm::vec3& outRight, glm::vec3& outUp) const;
    void UpdateMatrix();
    glm::vec3 m_position = glm::vec3(0, 0, 0);
    glm::vec3 m_rotation = glm::vec3(0, 0, 0);
    glm::vec3 m_scale = glm::vec3(1, 1, 1);
    glm::vec3 m_forwardVector = glm::vec3(0.0, 0.0, -1.0f);
    glm::vec3 m_upVector = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_rightVector = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::mat4 mModel = glm::mat4(1);
    glm::mat4 m_view = glm::mat4(1);
};