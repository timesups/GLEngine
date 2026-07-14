#pragma once

#include "RenderQueue.h"
#include "ShaderPass.h"
#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>

class Texture;
class Material;

enum class MaterialPropertyType
{
    Float,
    Int,
    Bool,
    Vec2,
    Vec3,
    Vec4,
    Mat3,
    Mat4,
    Texture2D,
    TextureCube
};

using MaterialPropertyValue =
    std::variant<float, int, bool, glm::vec2, glm::vec3, glm::vec4, glm::mat3, glm::mat4, std::shared_ptr<Texture>>;

struct MaterialProperty
{
    std::string name;
    MaterialPropertyType type;
    MaterialPropertyValue value;
    bool isDirty = true;
    /// Properties 块中显式写了 `=` 时为 true
    bool hasExplicitDefault = false;
    /// sampler2D/samplerCube 的默认贴图路径（已由 ResolveShaderDefaultTexturePath 解析）
    std::string defaultTexturePath;
    /// 从材质 JSON 加载贴图时是否按 sRGB 上传（默认 true）
    bool textureSrgb = true;
};

class Shader
{
  public:
    Shader();
    void CompileShaderFromCode(const std::vector<PassCode>& codes, const std::vector<PassOption>& options,
                               const std::unordered_map<std::string, MaterialProperty>& property,
                               const std::vector<std::string>& propertyOrder, int explicitRenderQueue = -1);
    void RebuildTaggedPassMap();
    void Use(const int index = 0);

    void SetValue(const std::string& name, const glm::vec3& value) const;
    void SetValue(const std::string& name, const glm::vec2& value) const;
    void SetValue(const std::string& name, float x, float y, float z) const;
    void SetValue(const std::string& name, float value) const;
    void SetValue(const std::string& name, const glm::vec4& value) const;
    void SetValue(const std::string& name, float x, float y, float z, float w) const;
    void SetValue(const std::string& name, int value) const;
    void SetValue(const std::string& name, const glm::mat4& value) const;
    void SetValue(const std::string& name, const glm::mat3& value) const;

  public:
    std::vector<Material*> m_rerelativeMaterials;
    std::unordered_map<std::string, MaterialProperty> m_properties;
    /// 与 shader Properties 块中声明顺序一致
    std::vector<std::string> m_propertyOrder;
    std::vector<std::unique_ptr<ShaderPass>> m_passes;
    /// "Key:Value" -> Pass；加载/编译后由 RebuildTaggedPassMap 构建。
    std::unordered_map<std::string, ShaderPass*> m_taggedPasses;
    std::string m_name;
    std::string m_path;
    int renderQueue = RenderQueue::Geometry;
    bool m_showInUi = true;
};