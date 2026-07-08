#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Mesh.h"
#include "Shader.h"
#include "ShaderPass.h"
#include "ShaderTags.h"
#include "Texture/Texture.h"

struct MaterialApplyData
{
    glm::mat4 mModel;
    glm::mat4 mNormal;
    glm::vec3 boundingBoxMax{};
    glm::vec3 boundingBoxMin{};
    MeshSection* section;
};

struct MaterialInstancedApplyData
{
    MeshSection* section = nullptr;
    int instanceOffset = 0;
    int instanceCount = 0;
};

class AssetManager;
class ShaderPass;

class Material
{
  public:
    Material(std::shared_ptr<Shader> shader);
    ~Material();
    void SetShader(std::shared_ptr<Shader> shader);
    void SyncPropertiesFromShader();
    void ApplyTextureDefaultsFromShader();
    void Update();
    void Apply(const MaterialApplyData& applyData, const std::string& lightMode = "");
    void ApplyInstanced(const MaterialInstancedApplyData& applyData, const std::string& lightMode = "");
    void SetFloatProperty(const std::string& name, float value);
    void SetIntProperty(const std::string& name, int value);
    void SetBoolProperty(const std::string& name, bool value);
    void SetVec2Property(const std::string& name, const glm::vec2& value);
    void SetVec3Property(const std::string& name, const glm::vec3& value);
    void SetVec4Property(const std::string& name, const glm::vec4& value);
    void SetMat3Property(const std::string& name, const glm::mat3& value);
    void SetMat4Property(const std::string& name, const glm::mat4& value);
    void SetTexture2DProperty(const std::string& name, std::shared_ptr<Texture> texture);
    void SetTextureCubeProperty(const std::string& name, std::shared_ptr<Texture> texture);

    int GetRenderQueue() const;
    void SetRenderQueue(int queue);
    void ClearRenderQueueOverride();
    bool HasRenderQueueOverride() const;
    std::shared_ptr<Shader> GetShader() const;
    const std::unordered_map<std::string, MaterialProperty>& GetProperties() const;
    const std::vector<std::string>& GetPropertyOrder() const;

    std::string m_name;
    std::string m_sourcePath;
    bool m_showInUi = true;

  private:
    template <typename T> void setProperty(const std::string& name, MaterialPropertyType type, T&& value)
    {
        MaterialProperty prop;
        prop.name = name;
        prop.type = type;
        prop.value = std::forward<T>(value);
        prop.isDirty = true;
        m_properties[name] = std::move(prop);
    }
    void UnregisterFromShader();
    void applyShaderProperties(ShaderPass& pass);
    void unbindShaderTextures();
    std::shared_ptr<Shader> m_shader;
    std::unordered_map<std::string, MaterialProperty> m_properties;
    std::vector<std::string> m_propertyOrder;
    int m_queueOverride = -1;
    int m_nextTextureUnit = 0;
};
