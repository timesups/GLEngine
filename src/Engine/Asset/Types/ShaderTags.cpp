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
