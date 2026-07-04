#pragma once

#include <cmath>

/// 引擎光照与场景尺度约定（非 IBL 直接光部分）。
/// 标定、调参、资产导入尺度均以此为准。
namespace LightingConvention
{

// -----------------------------------------------------------------------------
// 场景长度
// -----------------------------------------------------------------------------
/// 世界空间 1 单位 = 1 米（SI）。
/// 适用于：Transform 位置、阴影正交范围、点光距离衰减、相机移动速度等。
inline constexpr float kMetersPerWorldUnit = 1.0f;
inline constexpr float kWorldUnitsPerMeter = 1.0f;

// -----------------------------------------------------------------------------
// 定向光（Directional / Main Light）— 单位：lux（照度, lm/m²）
// -----------------------------------------------------------------------------
/// 组件 `Light::m_intensity`（定向光）：**lux**，表示垂直于光线方向的照度。
/// CPU 上传前转换：L_i = color × DirectionalLuxToShaderLi(lux)
/// 写入 light_buffer._MainLightColor.rgb；shader 内为 Cook-Torrance 入射 radiance 乘子。
///
/// 标定锚点（见 DirectionalCalibrationMeasured）：
///   100000 lux → 灰板 HDR ≈ 0.03516（与旧 unitless intensity=1 等价）
///
/// 无距离衰减；方向 normalize(-_MainLightDirection)。
/// 显示仍受 Camera exposure / gamma 影响。
inline constexpr float kReferenceDirectionalLux = 100000.0f;

inline constexpr float DirectionalLuxToShaderLi(float lux)
{
    return lux / kReferenceDirectionalLux;
}

inline constexpr float DirectionalShaderLiToLux(float shaderLi)
{
    return shaderLi * kReferenceDirectionalLux;
}

struct DirectionalLight
{
    static constexpr const char* kIntensityUnit = "lux";
    static constexpr const char* kEffectiveColorFormula = "color * DirectionalLuxToShaderLi(lux)";
};

/// 标定场景固定条件下测得的 HDR（灰板中心，IBL 关）。
/// lux → HDR（2026-07-03 实测；100000 lux ≡ 旧 intensity 1）
namespace DirectionalCalibrationMeasured
{
struct LuxToHdrSample
{
    float lux;
    float hdrLinear;
};

inline static constexpr LuxToHdrSample kGray18FloorSamples[] = {
    {50000.0f, 0.02124f},
    {100000.0f, 0.03516f},
    {200000.0f, 0.07129f},
};

/// 100000 lux 时的参考锚点
inline static constexpr float kReferenceHdrGray18AtReferenceLux = 0.03516f;
inline static constexpr float kReferenceLux = kReferenceDirectionalLux;

/// HDR ≈ kReferenceHdrGray18AtReferenceLux × (lux / kReferenceLux)
inline static constexpr float kHdrPerLux = kReferenceHdrGray18AtReferenceLux / kReferenceLux;

/// @deprecated 旧 unitless 命名
inline static constexpr float kReferenceHdrGray18AtIntensity1 = kReferenceHdrGray18AtReferenceLux;
} // namespace DirectionalCalibrationMeasured

// -----------------------------------------------------------------------------
// 定向光标定基线（Todo #3，引擎默认值 + CreateDirectionalLightCalibrationScene）
// -----------------------------------------------------------------------------
/// 标定条件见本 namespace；HDR 实测见 DirectionalCalibrationMeasured。
namespace DirectionalCalibrationBaseline
{
    // --- Light 组件默认（Directional）---
    inline static constexpr float kDefaultColorR = 1.0f;
    inline static constexpr float kDefaultColorG = 1.0f;
    inline static constexpr float kDefaultColorB = 1.0f;
    /// 定向光默认 100000 lux（晴天下量级；与标定锚点一致）
    inline static constexpr float kDefaultDirectionalLux = kReferenceDirectionalLux;
    inline static constexpr float kDefaultIntensity = kDefaultDirectionalLux;
    /// Upload 后 _MainLightColor.rgb = color × DirectionalLuxToShaderLi(lux)
    inline static constexpr float kDefaultEffectiveLi = 1.0f;

    inline static constexpr float kDefaultShadowNear = -20.0f; // 米，CreateLight(Directional) 覆盖
    inline static constexpr float kDefaultShadowFar = 20.0f;   // 米，CreateLight 通用
    inline static constexpr float kDefaultShadowArea = 20.0f;  // 米，正交半宽
    inline static constexpr float kDefaultShadowBias = 0.002f;
    inline static constexpr int kDefaultPcfSample = 1;

    // --- main.cpp CreateDefaultScene：定向光 "sun" ---
  inline static constexpr float kExampleSunRotationYaw = 220.0f;   // 度
  inline static constexpr float kExampleSunRotationPitch = 40.0f;  // 度
  inline static constexpr float kExampleSunRotationRoll = 0.0f;
    /// sun 未调用 SetColor/SetIntensity → 使用定向光默认 lux
    inline static constexpr float kExampleSunLux = kDefaultDirectionalLux;

    // --- Camera / Tonemap（RenderContext 默认相机 + Camera::postSetting）---
    inline static constexpr float kDefaultCameraPosX = 0.0f;
    inline static constexpr float kDefaultCameraPosY = 0.0f;
    inline static constexpr float kDefaultCameraPosZ = 5.0f; // 米
    inline static constexpr float kDefaultFovDeg = 60.0f;
    inline static constexpr float kDefaultNear = 0.1f;
    inline static constexpr float kDefaultFar = 1000.0f;
    inline static constexpr float kDefaultExposure = 1.0f;
    inline static constexpr float kDefaultGamma = 2.2f;
    inline static constexpr float kDefaultBloomEnabled = 0.0f; // bloom_setting.w

    // --- 相机成像物理参数（与 Tonemap exposure 联动，见 CameraImagingBaseline）---
    inline static constexpr float kDefaultAperture = 2.8f;           // f/2.8
    inline static constexpr float kDefaultShutterSpeed = 1.f / 60.f;  // 秒
    inline static constexpr float kDefaultIso = 100.f;

    // --- SSAO（Deferred 默认开启，影响最终亮度）---
    inline static constexpr float kDefaultSsaoRadius = 1.0f;
    inline static constexpr float kDefaultSsaoBias = 0.005f;
    inline static constexpr float kDefaultSsaoPow = 1.0f;
    inline static constexpr float kDefaultSsaoIntensity = 1.0f;

    // --- 示例场景几何（CreateDefaultScene）---
    static constexpr const char* kExampleMaterial = "engine://materials/DefaultMaterial";
    static constexpr const char* kExampleShader = "engine://shaders/DefaultLit.glsl";
    static constexpr const char* kExampleBaseColorTex = "engine://textures/white.png";
    static constexpr const char* kExampleRoughnessTex = "engine://textures/grey.png";
    static constexpr const char* kExampleMetallicTex = "engine://textures/grey.png";
    /// DefaultLit：base=白(1)，rough=灰(≈0.5)，metallic=grey×metaMul(0)
    inline static constexpr float kExampleBaseColorLinear = 1.0f;
    inline static constexpr float kExampleRoughness = 0.5f;
    inline static constexpr float kExampleMetallic = 0.0f;
    inline static constexpr float kExampleAo = 1.0f;
    /// 18% 中性灰板 albedo（线性）
    inline static constexpr float kTargetCalibrationAlbedo = 0.18f;
    // --- 定向光标定场景（CreateDirectionalLightCalibrationScene）---
    inline static constexpr float kCalibrationCameraPitchDeg = 15.0f;
    inline static constexpr float kCalibrationFloorScaleM = 4.0f;
    inline static constexpr float kCalibrationSphereCenterY = 0.5f;
    inline static constexpr bool kCalibrationIblLightingEnabled = false;
    inline static constexpr bool kCalibrationDrawSkyBackground = false;
    static constexpr const char* kCalibrationGray18Tex = "engine://textures/gray18_linear.png";
    static constexpr const char* kCalibrationGrayMaterial = "engine://materials/CalibrationGray18";

    // --- IBL（示例场景有，隔离定向光标定时需固定或禁用）---
    static constexpr const char* kExampleIbl = "engine://hdr/sunny_rose_garden.hdr";
    inline static constexpr float kExampleSkyBoxIntensity = 0.2f; // SkyBox::SetIntensity，仅 Fur/_AmbientColor
    inline static constexpr float kExampleSkyBoxBrightness = 1.0f;

    // --- 渲染配置 ---
    static constexpr const char* kDefaultRenderPipeline = "Deferred"; // Config::renderPipeline
    inline static constexpr int kDefaultViewportWidth = 1280;
    inline static constexpr int kDefaultViewportHeight = 720;

    /// @deprecated 使用 DirectionalCalibrationMeasured::kReferenceHdrGray18AtReferenceLux
    inline static constexpr float kReferenceHdrGray18 =
        DirectionalCalibrationMeasured::kReferenceHdrGray18AtReferenceLux;
} // namespace DirectionalCalibrationBaseline

// -----------------------------------------------------------------------------
// 点光源 / 聚光灯（Point / Spot）— 单位：lumen（光通量, lm）
// -----------------------------------------------------------------------------
/// 组件 `Light::m_intensity`（点光/聚光）：**lumen**，各向同性点光源总光通量。
/// CPU 上传：LocalLight.color.rgb = color × lumens
/// Shader 衰减：1 / (4π · dist²) × spotCone（dist 为米，见 kMetersPerWorldUnit）
/// 垂直受光面照度：E(lux) = lumens / (4π · dist²)
///
/// 与旧 unitless intensity 的等价锚点（dist=1m）：lumens = 4π ≈ 12.57 lm ↔ 旧 intensity=1
inline constexpr float kFourPi = 12.566370614359172f;
inline constexpr float kInvFourPi = 1.0f / kFourPi;

/// 旧 unitless intensity=1 @ 1m 等价的参考光通量
inline constexpr float kReferencePointLumens = kFourPi;

inline constexpr float PointLuxAtDistance(float lumens, float distanceMeters)
{
    const float d2 = distanceMeters * distanceMeters;
    return d2 > 0.0f ? lumens / (kFourPi * d2) : 0.0f;
}

struct PointLight
{
    static constexpr const char* kIntensityUnit = "lm";
    static constexpr const char* kEffectiveColorFormula = "color * lumens";
    static constexpr const char* kShaderAttenuation = "1 / (4π · dist²) × spotCone";
};

namespace PointLightBaseline
{
    /// 新建点光默认 1000 lm（约普通 LED 灯泡量级）
    inline static constexpr float kDefaultPointLumens = 1000.0f;
    inline static constexpr float kDefaultSpotLumens = 1000.0f;

    /// 1m 处垂直照度 ≈ 79.6 lux
    inline static constexpr float kDefaultLuxAtOneMeter = kDefaultPointLumens * kInvFourPi;

    /// 旧 unitless intensity=1 @ 1m
    inline static constexpr float kLegacyUnitlessIntensity = 1.0f;
    inline static constexpr float kLegacyEquivalentLumens = kReferencePointLumens;
} // namespace PointLightBaseline

// -----------------------------------------------------------------------------
// 相机成像（Physical Camera MVP）— 光圈 / 快门 / ISO → Tonemap exposure
// -----------------------------------------------------------------------------
/// 参考设置映射到 `DirectionalCalibrationBaseline::kDefaultExposure`（=1.0）。
/// 在 100k lux 标定场景下，默认成像参数与旧版手动 exposure=1 视觉一致。
namespace CameraImagingBaseline
{
inline static constexpr float kReferenceAperture = DirectionalCalibrationBaseline::kDefaultAperture;
inline static constexpr float kReferenceShutterSpeed = DirectionalCalibrationBaseline::kDefaultShutterSpeed;
inline static constexpr float kReferenceIso = DirectionalCalibrationBaseline::kDefaultIso;
inline static constexpr float kReferenceExposureLinear = DirectionalCalibrationBaseline::kDefaultExposure;

inline static constexpr float kMinAperture = 1.0f;
inline static constexpr float kMaxAperture = 32.0f;
inline static constexpr float kMinShutterSpeed = 1.f / 8000.f;
inline static constexpr float kMaxShutterSpeed = 30.f;
inline static constexpr float kMinIso = 50.f;
inline static constexpr float kMaxIso = 12800.f;
} // namespace CameraImagingBaseline

/// 相对参考光圈/快门/ISO 计算 Tonemap 线性 exposure；EV 补偿为 log2 倍率。
inline float ComputeExposureLinear(float aperture, float shutterSpeed, float iso,
                                   float exposureCompensationEv = 0.f)
{
    using namespace CameraImagingBaseline;
    const float apertureClamped = std::max(aperture, 0.1f);
    const float shutterClamped = std::max(shutterSpeed, 1e-6f);
    const float isoClamped = std::max(iso, 1.f);

    const float refOverAperture = kReferenceAperture / apertureClamped;
    const float apertureFactor = refOverAperture * refOverAperture;
    const float shutterFactor = shutterClamped / kReferenceShutterSpeed;
    const float isoFactor = isoClamped / kReferenceIso;

    return kReferenceExposureLinear * apertureFactor * shutterFactor * isoFactor *
           std::exp2(exposureCompensationEv);
}

} // namespace LightingConvention
