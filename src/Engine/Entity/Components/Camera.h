#pragma once
#include <glm/glm.hpp>

#include "Component.h"
/// GLSL 中 vec3 + float 占同一 16 字节槽位，CPU 侧用 vec4 表达以免与 glm::vec3 对齐差异错位。
struct alignas(16) CameraInputData
{
    glm::mat4 GL_MATRIX_V_NO_MOVE{};
    glm::mat4 GL_MATRIX_V{};
    glm::mat4 GL_I_MATRIX_V{};
    glm::mat4 GL_MATRIX_P{};
    glm::vec4 _CameraPosition_zNear{}; // xyz = _CameraPosition, w = _zNear
    /*
    x:zfar
    y:screenwidth
    z:screenheight
    w:pad
    */
    glm::vec4 _zFar_pad{};
    glm::mat4 GL_I_MATRIX_P{};
    /*
    x:time
    y:fov
    z:pad
    w:pad
    */
    glm::vec4 _time_pad{};
};

struct alignas(16) PostProcessSetting
{
    /*
    x : exposure (由 CameraImagingSettings 驱动)
    y : gamma
    z : pad
    w : pad
    */
    glm::vec4 tonemap_setting = glm::vec4(1.0, 2.2, 0.0, 0.0);
    /*
    x : threadshold
    y : softkeen
    z : intensity
    w : enable
    */
    glm::vec4 bloom_setting = glm::vec4(0.85, 0.2, 0.5, 0.0);
    /*
    x : _ssao_raduis
    y : _ssao_bias
    z : _ssao_pow
    w : _ssao_intensity
    */
    glm::vec4 sso_setting = glm::vec4(1.0, 0.005, 1.0, 1.0);
    /*
    x : AO 算法 (0=SSAO, 1=HBAO)
    yzw : pad
    */
    glm::vec4 sso_extra = glm::vec4(0.0, 0.0, 0.0, 0.0);
};

struct CameraImagingSettings
{
    float aperture = 2.8f;
    float shutterSpeed = 1.f / 60.f;
    float iso = 100.f;
    float exposureCompensationEv = 0.f;
};

class Transform;
class Camera : public Component
{
  public:
    Camera();
    virtual void Init() override;
    virtual void Update(float deltaTime) override;
    void QueueScrollDeltaY(float dy);
    /// 本帧鼠标位移（像素），在 Update 中转为视角旋转
    void QueueMouseLookDelta(double dx, double dy);
    /// -1 / 0 / 1，分别对应前后、右左、世界上下（E/Q）
    void SetMoveAxes(float forward, float right, float worldUp);
    void SetSprint(bool sprint);
    const glm::mat4& GetViewMatrix() const;
    const glm::mat4& GetProjectionMatrix() const;
    glm::vec3 GetPosition() const;
    glm::vec3 GetRightVector() const;
    /// 与视图矩阵 `lookAt(pos, pos + forward, up)` 一致，用于飞行、半透明排序等
    glm::vec3 GetForwardVector() const;
    void SetAspect(float aspect);
    float GetNear() const;
    float GetFar() const;
    float GetFov() const;
    void SetFov(float fov);
    void SetNear(float nearPlane);
    void SetFar(float farPlane);
    void SyncExposureFromImaging();
    float GetComputedExposureLinear() const;

  public:
    CameraImagingSettings imaging;
    PostProcessSetting postSetting;

  private:
    bool MoveFreeFly(float deltaTime);
    void UpdateProjectionMatrix();

  private:
    Transform* m_transform = nullptr;
    float m_fov = 60.f;
    float m_aspect = 16.f / 9.f;
    float m_near = 0.1f;
    float m_far = 1000.f;
    glm::mat4 mProjection = glm::mat4(1);

    float m_pendingScrollDeltaY = 0.f;
    float m_fovMin = 10.f;
    float m_fovMax = 120.f;
    float m_fovDegreesPerScroll = 0.1f;

    double m_pendingMouseDx = 0.0;
    double m_pendingMouseDy = 0.0;
    float m_mouseSensitivity = 0.12f;
    float m_pitchMin = -89.f;
    float m_pitchMax = 89.f;

    float m_axisForward = 0.f;
    float m_axisRight = 0.f;
    float m_axisWorldUp = 0.f;
    float m_moveSpeed = 6.f;
    bool m_sprint = false;
    float m_sprintMultiplier = 2.5f;
};
