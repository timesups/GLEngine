#include "Material.h"

#include <variant>

#include "../../Core/Config.h"
#include "../../Core/Log.h"
#include "../../Entity/DrawSetting.h"
#include "../../Renderer/InstanceBuffer.h"
#include "../AssetManager.h"
#include "Mesh.h"
#include "RenderQueue.h"
#include "Shader.h"
#include "ShaderPass.h"
#include "Texture/IBLImage.h"
#include "Texture/Texture2D.h"
#include "Texture/TextureCube.h"

#define MODULE "Material"

namespace
{
void ApplyShaderTextureDefault(MaterialProperty& prop)
{
    if (prop.type != MaterialPropertyType::Texture2D && prop.type != MaterialPropertyType::TextureCube)
        return;
    if (!std::holds_alternative<std::shared_ptr<Texture>>(prop.value))
    {
        return;
    }
    std::shared_ptr<Texture>& tex = std::get<std::shared_ptr<Texture>>(prop.value);
    if (tex && tex->GetId() != 0)
        return;
    if (prop.defaultTexturePath.empty())
        return;
    if (AssetManager::IsInitializing())
        return;

    if (prop.type == MaterialPropertyType::Texture2D)
        tex = AssetManager::Get().LoadTexture(prop.defaultTexturePath);
    else
    {
        std::shared_ptr<IBLImage> ibl = AssetManager::Get().LoadIBL(prop.defaultTexturePath);
        if (ibl && ibl->irradiance)
            tex = std::static_pointer_cast<Texture>(ibl->irradiance);
    }
}

bool CanCopyMaterialPropertyValue(const MaterialProperty& from, const MaterialProperty& to)
{
    if (from.type != to.type)
        return false;
    return from.value.index() == to.value.index();
}
} // namespace

Material::Material(std::shared_ptr<Shader> shader)
{
    SetShader(std::move(shader));
}

Material::~Material()
{
    UnregisterFromShader();
}

void Material::UnregisterFromShader()
{
    if (!m_shader)
        return;

    auto& materials = m_shader->m_rerelativeMaterials;
    const auto it = std::find(materials.begin(), materials.end(), this);
    if (it != materials.end())
        materials.erase(it);
}

void Material::SetShader(std::shared_ptr<Shader> shader)
{
    UnregisterFromShader();
    m_shader = std::move(shader);
    if (!m_shader)
    {
        m_properties.clear();
        m_propertyOrder.clear();
        return;
    }

    if (std::find(m_shader->m_rerelativeMaterials.begin(), m_shader->m_rerelativeMaterials.end(), this) ==
        m_shader->m_rerelativeMaterials.end())
    {
        m_shader->m_rerelativeMaterials.push_back(this);
    }
    SyncPropertiesFromShader();
}

int Material::GetRenderQueue() const
{
    if (m_queueOverride >= 0)
        return m_queueOverride;
    if (m_shader)
        return m_shader->renderQueue;
    return RenderQueue::Geometry;
}

void Material::SetRenderQueue(int queue)
{
    m_queueOverride = queue;
}

void Material::ClearRenderQueueOverride()
{
    m_queueOverride = -1;
}

void Material::ApplyTextureDefaultsFromShader()
{
    for (const std::string& name : m_propertyOrder)
    {
        auto it = m_properties.find(name);
        if (it != m_properties.end())
            ApplyShaderTextureDefault(it->second);
    }
}

void Material::SyncPropertiesFromShader()
{
    if (!m_shader)
        return;

    // 以 shader Properties 为准：已删除的属性会从材质/UI 中移除
    std::unordered_map<std::string, MaterialProperty> merged = m_shader->m_properties;
    for (auto& [name, prop] : merged)
    {
        if (m_properties.find(name) == m_properties.end())
            ApplyShaderTextureDefault(prop);
    }

    for (const auto& [name, oldProp] : m_properties)
    {
        auto it = merged.find(name);
        if (it == merged.end())
            continue;
        if (CanCopyMaterialPropertyValue(oldProp, it->second))
            it->second.value = oldProp.value;
    }
    m_properties = std::move(merged);
    m_propertyOrder = m_shader->m_propertyOrder;
}

void Material::Update()
{
    SyncPropertiesFromShader();
}

void Material::applyShaderProperties(ShaderPass& pass)
{
    m_nextTextureUnit = 0;
    for (auto& [name, property] : m_properties)
    {
        switch (property.type)
        {
        case MaterialPropertyType::Float:
            pass.SetValue(property.name, std::get<float>(property.value));
            break;
        case MaterialPropertyType::Int:
            pass.SetValue(property.name, std::get<int>(property.value));
            break;
        case MaterialPropertyType::Bool:
            pass.SetValue(property.name, std::get<bool>(property.value));
            break;
        case MaterialPropertyType::Vec2:
            pass.SetValue(property.name, std::get<glm::vec2>(property.value));
            break;
        case MaterialPropertyType::Vec3:
            pass.SetValue(property.name, std::get<glm::vec3>(property.value));
            break;
        case MaterialPropertyType::Vec4:
            pass.SetValue(property.name, std::get<glm::vec4>(property.value));
            break;
        case MaterialPropertyType::Mat3:
            pass.SetValue(property.name, std::get<glm::mat3>(property.value));
            break;
        case MaterialPropertyType::Mat4:
            pass.SetValue(property.name, std::get<glm::mat4>(property.value));
            break;
        case MaterialPropertyType::Texture2D:
        case MaterialPropertyType::TextureCube:
            if (!std::holds_alternative<std::shared_ptr<Texture>>(property.value))
                continue;
            ApplyShaderTextureDefault(property);
            const std::shared_ptr<Texture> texture = std::get<std::shared_ptr<Texture>>(property.value);
            if (!texture)
                continue;
            texture->Bind(m_nextTextureUnit);
            pass.SetValue(property.name, m_nextTextureUnit);
            m_nextTextureUnit += 1;
            break;
        }
    }
}

void Material::unbindShaderTextures()
{
    for (auto& [name, property] : m_properties)
    {
        if (property.type == MaterialPropertyType::Texture2D || property.type == MaterialPropertyType::TextureCube)
        {
            if (!std::holds_alternative<std::shared_ptr<Texture>>(property.value))
                continue;
            const std::shared_ptr<Texture> texture = std::get<std::shared_ptr<Texture>>(property.value);
            if (!texture)
                continue;
            texture->UnBind();
        }
    }
}

void Material::Apply(const MaterialApplyData& applyData, const std::vector<std::string>& shaderTags,
                     bool requireMatchingPass)
{
    if (!applyData.section || !applyData.section->mesh || !m_shader)
        return;

    ShaderPass* pass = FindShaderPassForTags(*m_shader, shaderTags);
    if (!pass)
    {
        // 无匹配 ShaderTag 的 Pass：跳过绘制；材质覆盖时回退到第一个可用 Pass。
        if (requireMatchingPass)
            return;
        for (auto& p : m_shader->m_passes)
        {
            if (p)
            {
                pass = p.get();
                break;
            }
        }
        if (!pass)
            return;
    }

    if (!pass->IsReady())
    {
        static bool s_loggedFallback = false;
        if (!s_loggedFallback)
        {
            LogA(LogLevel::WARNING, "Material '{}' shader pass not ready, using ErrorShader fallback", m_name);
            s_loggedFallback = true;
        }
        auto errorShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/ErrorShader.glsl");
        errorShader->m_passes[0]->use();
        errorShader->m_passes[0]->SetValue("_UseInstancing", 0);
        errorShader->m_passes[0]->SetValue("GL_MATRIX_M", applyData.mModel);
        errorShader->m_passes[0]->SetValue("GL_MATRIX_N", applyData.mNormal);
        errorShader->m_passes[0]->SetState();
        applyData.section->mesh->Draw(GL_TRIANGLES);
        return;
    }

    pass->use();
    pass->SetValue("_UseInstancing", 0);
    pass->SetValue("GL_MATRIX_M", applyData.mModel);
    pass->SetValue("GL_MATRIX_N", applyData.mNormal);
    pass->SetValue("_BoundingBoxMax", applyData.boundingBoxMax);
    pass->SetValue("_BoundingBoxMin", applyData.boundingBoxMin);
    applyShaderProperties(*pass);
    pass->SetState();
    pass->SetValue("_DrawCount", pass->GetOptions().DrawTimes);
    for (int i = 0; i < pass->GetOptions().DrawTimes; i++)
    {
        pass->SetValue("_PassIndex", i);
        applyData.section->mesh->Draw((GLenum)pass->GetOptions().mode);
    }
    unbindShaderTextures();
}

void Material::ApplyInstanced(const MaterialInstancedApplyData& applyData, const std::vector<std::string>& shaderTags,
                              bool requireMatchingPass)
{
    if (!applyData.section || !applyData.section->mesh || applyData.instanceCount <= 0 || !m_shader)
        return;

    ShaderPass* pass = FindShaderPassForTags(*m_shader, shaderTags);
    if (!pass)
    {
        if (requireMatchingPass)
            return;
        for (auto& p : m_shader->m_passes)
        {
            if (p)
            {
                pass = p.get();
                break;
            }
        }
        if (!pass)
            return;
    }

    if (!pass->IsReady())
        return;

    InstanceBuffer::Get().Bind();

    pass->use();
    pass->SetValue("_UseInstancing", 1);
    pass->SetValue("_InstanceOffset", applyData.instanceOffset);
    pass->SetValue("_InstanceCount", applyData.instanceCount);
    applyShaderProperties(*pass);
    pass->SetState();
    pass->SetValue("_DrawCount", pass->GetOptions().DrawTimes);
    for (int i = 0; i < pass->GetOptions().DrawTimes; i++)
    {
        pass->SetValue("_PassIndex", i);
        applyData.section->mesh->DrawInstanced((GLenum)pass->GetOptions().mode, applyData.instanceCount);
    }
    unbindShaderTextures();
}

void Material::SetFloatProperty(const std::string& name, float value)
{
    setProperty(name, MaterialPropertyType::Float, value);
}

void Material::SetIntProperty(const std::string& name, int value)
{
    setProperty(name, MaterialPropertyType::Int, value);
}

void Material::SetBoolProperty(const std::string& name, bool value)
{
    setProperty(name, MaterialPropertyType::Bool, value);
}

void Material::SetVec2Property(const std::string& name, const glm::vec2& value)
{
    setProperty(name, MaterialPropertyType::Vec2, value);
}

void Material::SetVec3Property(const std::string& name, const glm::vec3& value)
{
    setProperty(name, MaterialPropertyType::Vec3, value);
}

void Material::SetVec4Property(const std::string& name, const glm::vec4& value)
{
    setProperty(name, MaterialPropertyType::Vec4, value);
}

void Material::SetMat3Property(const std::string& name, const glm::mat3& value)
{
    setProperty(name, MaterialPropertyType::Mat3, value);
}

void Material::SetMat4Property(const std::string& name, const glm::mat4& value)
{
    setProperty(name, MaterialPropertyType::Mat4, value);
}

void Material::SetTexture2DProperty(const std::string& name, std::shared_ptr<Texture> texture)
{
    setProperty(name, MaterialPropertyType::Texture2D, std::move(texture));
}

void Material::SetTextureCubeProperty(const std::string& name, std::shared_ptr<Texture> texture)
{
    setProperty(name, MaterialPropertyType::TextureCube, std::move(texture));
}

bool Material::HasRenderQueueOverride() const
{
    return m_queueOverride >= 0;
}

std::shared_ptr<Shader> Material::GetShader() const
{
    return m_shader;
}

const std::unordered_map<std::string, MaterialProperty>& Material::GetProperties() const
{
    return m_properties;
}

const std::vector<std::string>& Material::GetPropertyOrder() const
{
    return m_propertyOrder;
}

#undef MODULE
