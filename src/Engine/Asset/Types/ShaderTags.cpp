#include "ShaderTags.h"

#include "Shader.h"
#include "ShaderPass.h"

#include <cctype>

namespace
{
std::string ToLowerCopy(std::string value)
{
    for (char& c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
} // namespace

std::string CanonicalizeShaderTagKey(const std::string& keyLower)
{
    if (keyLower == "lightmode")
        return "LightMode";
    if (keyLower == "renderpipeline")
        return "RenderPipeline";
    if (keyLower == "queue")
        return "Queue";
    if (keyLower == "rendertype")
        return "RenderType";
    if (keyLower.empty())
        return keyLower;
    std::string key = keyLower;
    key[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
    return key;
}

bool ShaderTagEqualsIgnoreCase(const std::string& a, const std::string& b)
{
    return ToLowerCopy(a) == ToLowerCopy(b);
}

const std::string* FindShaderTag(const ShaderTagMap& tags, const std::string& key)
{
    const auto direct = tags.find(key);
    if (direct != tags.end())
        return &direct->second;

    const std::string lower = ToLowerCopy(key);
    for (const auto& [tagKey, tagValue] : tags)
    {
        if (ToLowerCopy(tagKey) == lower)
            return &tagValue;
    }
    return nullptr;
}

bool ShaderHasLightModePass(const Shader& shader, const std::string& lightMode)
{
    for (const auto& pass : shader.m_passes)
    {
        if (!pass)
            continue;
        const std::string* passLightMode = FindShaderTag(pass->GetOptions().tags, "LightMode");
        if (passLightMode && ShaderTagEqualsIgnoreCase(*passLightMode, lightMode))
            return true;
    }
    return false;
}

bool PassMatchesLightMode(const ShaderPass& pass, const std::string& lightMode)
{
    const std::string* passLightMode = FindShaderTag(pass.GetOptions().tags, "LightMode");
    const bool defaultDraw =
        lightMode.empty() || ShaderTagEqualsIgnoreCase(lightMode, LightMode::Always);

    if (defaultDraw)
        return passLightMode == nullptr;

    if (!passLightMode)
        return false;

    return ShaderTagEqualsIgnoreCase(*passLightMode, lightMode);
}

std::string GetEffectivePassRenderPipelineTag(const ShaderPass& pass)
{
    if (const std::string* tag = FindShaderTag(pass.GetOptions().tags, "RenderPipeline"))
        return *tag;
    return PipelineTag::kDefault;
}

bool ShouldFilterPassByRenderPipeline(const std::string& lightMode)
{
    return lightMode.empty() || ShaderTagEqualsIgnoreCase(lightMode, LightMode::Always);
}

bool ShaderHasRenderPipelinePass(const Shader& shader, const std::string& renderPipeline)
{
    for (const auto& pass : shader.m_passes)
    {
        if (!pass)
            continue;
        if (PassMatchesRenderPipeline(*pass, renderPipeline))
            return true;
    }
    return false;
}

bool PassMatchesRenderPipeline(const ShaderPass& pass, const std::string& renderPipeline)
{
    if (renderPipeline.empty())
        return true;

    const std::string* passPipeline = FindShaderTag(pass.GetOptions().tags, "RenderPipeline");
    if (!passPipeline)
    {
        // 未标注管线的 Pass 默认归属 Forward。
        return ShaderTagEqualsIgnoreCase(renderPipeline, PipelineTag::Forward) ||
               ShaderTagEqualsIgnoreCase(renderPipeline, PipelineTag::Deferred);
    }

    return ShaderTagEqualsIgnoreCase(*passPipeline, renderPipeline);
}

bool PassMatchesDrawTags(const ShaderPass& pass, const std::string& lightMode, const std::string& renderPipeline)
{
    if (!PassMatchesLightMode(pass, lightMode))
        return false;

    if (!ShouldFilterPassByRenderPipeline(lightMode))
        return true;

    return PassMatchesRenderPipeline(pass, renderPipeline);
}
