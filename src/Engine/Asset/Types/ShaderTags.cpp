#include "ShaderTags.h"

#include "../../Core/Config.h"
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

bool DrawTagsContainKey(const std::vector<std::string>& drawTags, const std::string& key)
{
    for (const std::string& tag : drawTags)
    {
        std::string tagKey;
        std::string tagValue;
        if (ParseShaderTag(tag, tagKey, tagValue) && ShaderTagEqualsIgnoreCase(tagKey, key))
            return true;
    }
    return false;
}

bool PassValueMatchesDrawTag(const ShaderPass& pass, const std::string& key, const std::string& value)
{
    const std::string* passValue = FindShaderTag(pass.GetOptions().tags, key);
    if (!passValue)
        return false;
    return ShaderTagEqualsIgnoreCase(*passValue, value);
}
} // namespace

std::string MakeShaderTag(const std::string& key, const std::string& value)
{
    return key + ":" + value;
}

bool ParseShaderTag(const std::string& tag, std::string& outKey, std::string& outValue)
{
    const size_t sep = tag.find(':');
    if (sep == std::string::npos)
        return false;

    outKey = tag.substr(0, sep);
    outValue = tag.substr(sep + 1);
    return !outKey.empty() && !outValue.empty();
}

bool ShaderTagEqualsIgnoreCase(const std::string& a, const std::string& b)
{
    return ToLowerCopy(a) == ToLowerCopy(b);
}

std::string NormalizeShaderTag(const std::string& tag)
{
    return ToLowerCopy(tag);
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

std::string DefaultLightModeForActivePipeline()
{
    const std::string& pipeline = Config::Get().renderPipeline;
    if (ShaderTagEqualsIgnoreCase(pipeline, "Forward"))
        return "Forward";
    return "Deferred";
}

std::vector<std::string> BuildEffectiveShaderTags(const std::vector<std::string>& drawTags)
{
    std::vector<std::string> effective = drawTags;
    if (!DrawTagsContainKey(effective, "LightMode"))
        effective.push_back(MakeShaderTag("LightMode", DefaultLightModeForActivePipeline()));
    return effective;
}

bool PassMatchesShaderTags(const ShaderPass& pass, const std::vector<std::string>& drawTags)
{
    if (drawTags.empty())
        return FindShaderTag(pass.GetOptions().tags, "LightMode") == nullptr;

    for (const std::string& tag : drawTags)
    {
        std::string key;
        std::string value;
        if (!ParseShaderTag(tag, key, value))
            continue;

        // LightMode:Always（或空）匹配没有 LightMode Tag 的 Pass
        if (ShaderTagEqualsIgnoreCase(key, "LightMode") &&
            (ShaderTagEqualsIgnoreCase(value, "Always") || value.empty()))
        {
            if (FindShaderTag(pass.GetOptions().tags, "LightMode") != nullptr)
                return false;
            continue;
        }

        if (!PassValueMatchesDrawTag(pass, key, value))
            return false;
    }
    return true;
}

ShaderPass* FindShaderPassForTags(Shader& shader, const std::vector<std::string>& drawTags)
{
    const std::vector<std::string> effectiveTags = BuildEffectiveShaderTags(drawTags);

    if (effectiveTags.size() == 1)
    {
        const auto indexed = shader.m_taggedPasses.find(NormalizeShaderTag(effectiveTags.front()));
        if (indexed != shader.m_taggedPasses.end() && indexed->second &&
            PassMatchesShaderTags(*indexed->second, effectiveTags))
            return indexed->second;
    }

    for (const auto& passPtr : shader.m_passes)
    {
        if (!passPtr)
            continue;
        if (PassMatchesShaderTags(*passPtr, effectiveTags))
            return passPtr.get();
    }
    return nullptr;
}

bool ShaderHasShaderTag(const Shader& shader, const std::string& tag)
{
    return shader.m_taggedPasses.find(NormalizeShaderTag(tag)) != shader.m_taggedPasses.end();
}
