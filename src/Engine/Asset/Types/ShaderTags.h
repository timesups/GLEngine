#pragma once

#include <string>
#include <unordered_map>

using ShaderTagMap = std::unordered_map<std::string, std::string>;

/// 与 Unity LightMode 对齐的常用 Pass Tag 值。
namespace LightMode
{
constexpr const char* Always = "Always";
constexpr const char* ForwardBase = "ForwardBase";
constexpr const char* ForwardAdd = "ForwardAdd";
constexpr const char* Deferred = "Deferred";
constexpr const char* ShadowCaster = "ShadowCaster";
constexpr const char* EndfieldGBuffer = "EndfieldGBuffer";
} // namespace LightMode

/// Pass 所属渲染管线；未指定 RenderPipeline 标签时视为 Forward。
namespace PipelineTag
{
constexpr const char* Forward = "Forward";
constexpr const char* Deferred = "Deferred";
constexpr const char* Endfield = "Endfield";
constexpr const char* kDefault = Forward;
} // namespace PipelineTag

class Shader;
class ShaderPass;

std::string CanonicalizeShaderTagKey(const std::string& keyLower);
bool ShaderTagEqualsIgnoreCase(const std::string& a, const std::string& b);
const std::string* FindShaderTag(const ShaderTagMap& tags, const std::string& key);
std::string GetEffectivePassRenderPipelineTag(const ShaderPass& pass);
bool ShouldFilterPassByRenderPipeline(const std::string& lightMode);
bool ShaderHasLightModePass(const Shader& shader, const std::string& lightMode);
bool ShaderHasRenderPipelinePass(const Shader& shader, const std::string& renderPipeline);
bool PassMatchesLightMode(const ShaderPass& pass, const std::string& lightMode);
bool PassMatchesRenderPipeline(const ShaderPass& pass, const std::string& renderPipeline);
bool PassMatchesDrawTags(const ShaderPass& pass, const std::string& lightMode, const std::string& renderPipeline);
