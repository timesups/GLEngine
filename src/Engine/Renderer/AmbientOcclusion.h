#pragma once

/// 环境光遮蔽算法（与 PostProcessSetting::sso_extra.x / UBO _ao_mode 对应）
enum class AmbientOcclusionMode : int
{
    SSAO = 0,
    HBAO = 1,
};

inline AmbientOcclusionMode AmbientOcclusionModeFromFloat(const float value)
{
    const int mode = static_cast<int>(value + 0.5f);
    if (mode == static_cast<int>(AmbientOcclusionMode::HBAO))
        return AmbientOcclusionMode::HBAO;
    return AmbientOcclusionMode::SSAO;
}

inline float AmbientOcclusionModeToFloat(const AmbientOcclusionMode mode)
{
    return static_cast<float>(static_cast<int>(mode));
}
