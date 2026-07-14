#include "Shader.h"

#include "../../Core/Log.h"
#include "RenderQueue.h"
#include "ShaderTags.h"

#define MODULE "Shader"

Shader::Shader() = default;

void Shader::CompileShaderFromCode(const std::vector<PassCode>& codes, const std::vector<PassOption>& options,
                                   const std::unordered_map<std::string, MaterialProperty>& property,
                                   const std::vector<std::string>& propertyOrder, int explicitRenderQueue)
{
    bool hasBlend = false;
    m_passes.clear();
    for (int i = 0; i < codes.size(); i++)
    {
        if (options[i].enableBlend)
            hasBlend = true;
        std::unique_ptr<ShaderPass> pass = std::make_unique<ShaderPass>(codes[i], options[i], i);
        m_passes.push_back(std::move(pass));
    }

    if (explicitRenderQueue >= 0)
        renderQueue = explicitRenderQueue;
    else
        renderQueue = RenderQueue::DefaultFromBlendEnabled(hasBlend);
    // 热重载时保留 shader 上已有的同名属性值
    std::unordered_map<std::string, MaterialProperty> merged = property;
    for (const auto& [name, oldProp] : m_properties)
    {
        auto it = merged.find(name);
        if (it == merged.end() || oldProp.type != it->second.type)
            continue;
        if (oldProp.value.index() == it->second.value.index())
            it->second.value = oldProp.value;
    }
    m_properties = std::move(merged);
    m_propertyOrder = propertyOrder;
    RebuildTaggedPassMap();
}

void Shader::RebuildTaggedPassMap()
{
    m_taggedPasses.clear();
    for (const auto& passPtr : m_passes)
    {
        if (!passPtr)
            continue;

        for (const auto& [key, value] : passPtr->GetOptions().tags)
        {
            if (key.empty() || value.empty())
                continue;

            const std::string tag = MakeShaderTag(key, value);
            const std::string tagKey = NormalizeShaderTag(tag);
            const auto existing = m_taggedPasses.find(tagKey);
            if (existing != m_taggedPasses.end() && existing->second != passPtr.get())
            {
                LogA(LogLevel::WARNING, "Shader '{}' duplicate tag '{}' on multiple passes; using latest",
                     m_path.empty() ? m_name : m_path, tag);
            }
            m_taggedPasses[tagKey] = passPtr.get();
        }
    }
}

void Shader::Use(const int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_passes.size())
    {
        LogA(LogLevel::ERROR, "Shader::Use invalid pass index {} (pass count={})", index, m_passes.size());
        return;
    }
    m_passes[index]->SetState();
    m_passes[index]->use();
}

void Shader::SetValue(const std::string& name, const glm::vec3& value) const
{
    for (auto& pass : m_passes)
    {
        pass->SetValue(name, value);
    }
}
void Shader::SetValue(const std::string& name, const glm::vec2& value) const
{
    for (auto& pass : m_passes)
    {
        pass->SetValue(name, value);
    }
}
void Shader::SetValue(const std::string& name, float x, float y, float z) const
{
    for (auto& pass : m_passes)
    {
        pass->SetValue(name, x, y, z);
    }
}
void Shader::SetValue(const std::string& name, float value) const
{
    for (auto& pass : m_passes)
    {
        pass->SetValue(name, value);
    }
}
void Shader::SetValue(const std::string& name, const glm::vec4& value) const
{
    for (auto& pass : m_passes)
    {
        pass->SetValue(name, value);
    }
}
void Shader::SetValue(const std::string& name, float x, float y, float z, float w) const
{
    for (auto& pass : m_passes)
    {
        pass->SetValue(name, x, y, z, w);
    }
}
void Shader::SetValue(const std::string& name, int value) const
{
    for (auto& pass : m_passes)
    {
        pass->SetValue(name, value);
    }
}
void Shader::SetValue(const std::string& name, const glm::mat4& value) const
{
    for (auto& pass : m_passes)
    {
        pass->SetValue(name, value);
    }
}
void Shader::SetValue(const std::string& name, const glm::mat3& value) const
{
    for (auto& pass : m_passes)
    {
        pass->SetValue(name, value);
    }
}

#undef MODULE