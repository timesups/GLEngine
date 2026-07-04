#include "Transform.h"

#include "glm/matrix.hpp"
#include <glm/gtc/matrix_transform.hpp>


Transform::Transform()
{
    UpdateMatrix();
}
// Set
void Transform::SetPosition(const glm::vec3& value)
{
    m_position = value;
    UpdateMatrix();
}
void Transform::SetPosition(const float x, const float y, const float z)
{
    SetPosition(glm::vec3(x, y, z));
}
void Transform::SetRotation(const glm::vec3& value)
{
    m_rotation = value;
    UpdateMatrix();
}
void Transform::SetRotation(const float x, const float y, const float z)
{
    SetRotation(glm::vec3(x, y, z));
}
void Transform::SetScale(const glm::vec3& value)
{
    m_scale = value;
    UpdateMatrix();
}
void Transform::SetScale(const float x, const float y, const float z)
{
    SetScale(glm::vec3(x, y, z));
}
// Add
void Transform::AddPosition(const glm::vec3& value)
{
    m_position += value;
    UpdateMatrix();
}
void Transform::AddPosition(float x, float y, float z)
{
    AddPosition(glm::vec3(x, y, z));
}
void Transform::AddRotation(const glm::vec3& value)
{
    m_rotation += value;
    UpdateMatrix();
}
void Transform::AddRotation(float x, float y, float z)
{
    AddRotation(glm::vec3(x, y, z));
}
// Get
const glm::mat4& Transform::GetModelMatrix() const
{
    return mModel;
}
/// 与 `lookAt(position, position + GetForwardVector(), GetUpVector())` 一致，供相机、灯光阴影等复用
const glm::mat4& Transform::GetViewMatrix() const
{
    return m_view;
}
const glm::mat4 Transform::GetNormalMatrix() const
{
    return glm::transpose(glm::inverse(glm::mat4(glm::mat3(mModel))));
}
const glm::vec3& Transform::GetPosition() const
{
    return m_position;
}
const glm::vec3& Transform::GetRotation() const
{
    return m_rotation;
}
const glm::vec3& Transform::GetScale() const
{
    return m_scale;
}
const glm::vec3& Transform::GetForwardVector() const
{
    return m_forwardVector;
}
const glm::vec3& Transform::GetUpVector() const
{
    return m_upVector;
}
const glm::vec3& Transform::GetRightVector() const
{
    return m_rightVector;
}
glm::vec3 Transform::ComputeViewForward() const
{
    const float yaw = glm::radians(m_rotation.y);
    const float pitch = glm::radians(m_rotation.x);
    glm::vec3 f;
    f.x = glm::sin(yaw) * glm::cos(pitch);
    f.y = glm::sin(pitch);
    f.z = -glm::cos(yaw) * glm::cos(pitch);
    return glm::normalize(f);
}
glm::vec3 Transform::ComputeBaseRight(const glm::vec3& forward)
{
    glm::vec3 r = glm::cross(forward, glm::vec3(0.f, 1.f, 0.f));
    const float len = glm::length(r);
    if (len < 1e-6f)
        return glm::vec3(1.f, 0.f, 0.f);
    return r / len;
}
void Transform::ComputeViewBasis(const glm::vec3& forward, glm::vec3& outRight, glm::vec3& outUp) const
{
    const glm::vec3 baseRight = ComputeBaseRight(forward);
    const glm::vec3 baseUp = glm::normalize(glm::cross(baseRight, forward));

    const float roll = glm::radians(m_rotation.z);
    const float c = glm::cos(roll);
    const float s = glm::sin(roll);
    outRight = glm::normalize(baseRight * c - baseUp * s);
    outUp = glm::normalize(baseUp * c + baseRight * s);
}

void Transform::UpdateMatrix()
{
    m_forwardVector = ComputeViewForward();
    ComputeViewBasis(m_forwardVector, m_rightVector, m_upVector);

    // 与 lookAt / 相机 / 灯光共用同一套 basis，局部 -Z 为 forward
    glm::mat4 rotation(1.f);
    rotation[0] = glm::vec4(m_rightVector, 0.f);
    rotation[1] = glm::vec4(m_upVector, 0.f);
    rotation[2] = glm::vec4(-m_forwardVector, 0.f);
    rotation[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);

    mModel = glm::translate(glm::mat4(1.f), m_position) * rotation * glm::scale(glm::mat4(1.f), m_scale);
    m_view = glm::lookAt(m_position, m_position + m_forwardVector, m_upVector);
}