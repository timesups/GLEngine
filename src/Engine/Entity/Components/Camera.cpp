#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include "../../Core/LightingConvention.h"
#include "../Entity.h"
#include "Transform.h"

Camera::Camera() = default;

void Camera::Init()
{
    m_transform = GetEntity()->GetComponent<Transform>();
    if (!m_transform)
        m_transform = GetEntity()->AddComponent<Transform>();
    SyncExposureFromImaging();
    UpdateProjectionMatrix();
}

void Camera::Update(float deltaTime)
{
    if (!m_transform)
        return;

    if (m_pendingScrollDeltaY != 0.f)
    {
        m_moveSpeed += m_pendingScrollDeltaY * m_fovDegreesPerScroll;
        m_moveSpeed = glm::max(m_moveSpeed, 0.0f);
        m_pendingScrollDeltaY = 0.f;
    }

    if (m_pendingMouseDx != 0.0 || m_pendingMouseDy != 0.0)
    {
        glm::vec3 rot = m_transform->GetRotation();
        rot.y += static_cast<float>(m_pendingMouseDx) * m_mouseSensitivity;
        rot.x -= static_cast<float>(m_pendingMouseDy) * m_mouseSensitivity;
        rot.x = glm::clamp(rot.x, m_pitchMin, m_pitchMax);
        m_transform->SetRotation(rot);
        m_pendingMouseDx = 0.0;
        m_pendingMouseDy = 0.0;
    }

    MoveFreeFly(deltaTime);
}

void Camera::QueueScrollDeltaY(float dy)
{
    m_pendingScrollDeltaY += dy;
}

/// 本帧鼠标位移（像素），在 Update 中转为视角旋转
void Camera::QueueMouseLookDelta(double dx, double dy)
{
    m_pendingMouseDx += dx;
    m_pendingMouseDy += dy;
}

/// -1 / 0 / 1，分别对应前后、右左、世界上下（E/Q）
void Camera::SetMoveAxes(float forward, float right, float worldUp)
{
    m_axisForward = forward;
    m_axisRight = right;
    m_axisWorldUp = worldUp;
}

void Camera::SetSprint(bool sprint)
{
    m_sprint = sprint;
}

const glm::mat4& Camera::GetViewMatrix() const
{
    static const glm::mat4 kIdentity(1.f);
    return m_transform ? m_transform->GetViewMatrix() : kIdentity;
}
const glm::mat4& Camera::GetProjectionMatrix() const
{
    return mProjection;
}

glm::vec3 Camera::GetPosition() const
{
    return m_transform ? m_transform->GetPosition() : glm::vec3(0);
}

glm::vec3 Camera::GetRightVector() const
{
    return m_transform ? m_transform->GetRightVector() : glm::vec3(1.f, 0.f, 0.f);
}

glm::vec3 Camera::GetForwardVector() const
{
    return m_transform ? m_transform->GetForwardVector() : glm::vec3(0.f, 0.f, -1.f);
}
void Camera::SetAspect(float aspect)
{
    m_aspect = aspect;
    UpdateProjectionMatrix();
}

float Camera::GetNear() const
{
    return m_near;
}
float Camera::GetFar() const
{
    return m_far;
}
float Camera::GetFov() const
{
    return m_fov;
}
void Camera::SetFov(float fov)
{
    m_fov = glm::clamp(fov, m_fovMin, m_fovMax);
    UpdateProjectionMatrix();
}

void Camera::SetNear(float nearPlane)
{
    m_near = glm::max(nearPlane, 0.001f);
    UpdateProjectionMatrix();
}

void Camera::SetFar(float farPlane)
{
    m_far = glm::max(farPlane, m_near + 0.001f);
    UpdateProjectionMatrix();
}

float Camera::GetComputedExposureLinear() const
{
    return LightingConvention::ComputeExposureLinear(imaging.aperture, imaging.shutterSpeed, imaging.iso,
                                                     imaging.exposureCompensationEv);
}

void Camera::SyncExposureFromImaging()
{
    postSetting.tonemap_setting.x = GetComputedExposureLinear();
}

bool Camera::MoveFreeFly(float deltaTime)
{
    const glm::vec3 forward = m_transform->GetForwardVector();
    const glm::vec3 right = m_transform->GetRightVector();
    const glm::vec3 worldUp(0.f, 1.f, 0.f);

    glm::vec3 delta = forward * m_axisForward + right * m_axisRight + worldUp * m_axisWorldUp;
    const float lenSq = glm::dot(delta, delta);
    if (lenSq < 1e-8f)
        return false;

    delta = glm::normalize(delta) * m_moveSpeed * (m_sprint ? m_sprintMultiplier : 1.f) * deltaTime;
    m_transform->AddPosition(delta);
    return true;
}

void Camera::UpdateProjectionMatrix()
{
    mProjection = glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
}
