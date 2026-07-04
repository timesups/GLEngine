#pragma once

#include <string>

/// 与 Unity 类似的整型渲染队列；数值越小越早绘制。
namespace RenderQueue
{
constexpr int Background = 1000;
constexpr int Geometry = 2000;
constexpr int AlphaTest = 2450;
constexpr int Skybox = 2500;
constexpr int Transparent = 3000;
constexpr int Overlay = 4000;

/// 参与 GBuffer / 不透明几何 pass 的队列上界（不含 Skybox 及 Transparent）
constexpr int OpaqueUpperBound = Skybox;

/// 将 shader / material 文件中的 Queue 字段解析为整型；失败返回 false。
bool TryParseToken(const std::string& token, int& outQueue);

/// 未显式指定 Queue 时，根据 pass 是否开启混合推断默认值。
int DefaultFromBlendEnabled(bool blendEnabled);
} // namespace RenderQueue
