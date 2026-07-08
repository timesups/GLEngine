#pragma once
#include "ShaderTags.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

enum SHADERTYPE
{
    VERTEX,
    FRAGMENT,
    GEOMETRY,
    PROGRAM,
    COMPUTE,
};

enum class CullMode
{
    BACK = GL_BACK,
    FRONT = GL_FRONT,
    OFF = -1,
};

// 与 glDepthFunc / glStencilFunc 的比较方式一致：深度为片段深度与深度缓冲比较，模板为 ref 与模板缓冲比较。
enum class Func
{
    ALWAYS = GL_ALWAYS,     // 始终通过比较
    NEVER = GL_NEVER,       // 始终不通过比较
    LESS = GL_LESS,         // 小于时通过（深度：片段深度 < 缓冲值；模板：ref < 缓冲值）
    EQUAL = GL_EQUAL,       // 相等时通过
    LEQUAL = GL_LEQUAL,     // 小于等于时通过
    GREATER = GL_GREATER,   // 大于时通过
    NOTEQUAL = GL_NOTEQUAL, // 不相等时通过
    GEQUAL = GL_GEQUAL,     // 大于等于时通过
};

enum class BlendFunc
{
    ZERO = GL_ZERO,
    ONE = GL_ONE,
    SRC_COLOR = GL_SRC_COLOR,
    ONE_MINUS_SRC_COLOR = GL_ONE_MINUS_SRC_COLOR,
    DST_COLOR = GL_DST_COLOR,
    ONE_MINUS_DST_COLOR = GL_ONE_MINUS_DST_COLOR,
    SRC_ALPHA = GL_SRC_ALPHA,
    ONE_MINUS_SRC_ALPHA = GL_ONE_MINUS_SRC_ALPHA,
    DST_ALPHA = GL_DST_ALPHA,
    ONE_MINUS_DST_ALPHA = GL_ONE_MINUS_DST_ALPHA,
};

enum class BlendEq
{
    ADD = GL_FUNC_ADD,
    SUBTRACT = GL_FUNC_SUBTRACT,
    REVERSE_SUBTRACT = GL_FUNC_REVERSE_SUBTRACT,
    MIN = GL_MIN,
    MAX = GL_MAX,
};

enum class StencilOp
{
    KEEP = GL_KEEP,           // 保持当前
    ZERO = GL_ZERO,           // 将模板设置为0
    REPLACE = GL_REPLACE,     // 将模板值设置为glStencilFunc函数设置的ref值
    INCR = GL_INCR,           // 如果模板值小于最大值则将模板值加1
    INCR_WRAP = GL_INCR_WRAP, //
    DECR = GL_DECR,
    DECR_WRAP = GL_DECR_WRAP,
    INVERT = GL_INVERT,
};

enum class DrawMode
{
    POINTS = GL_POINTS,
    LINES = GL_LINES,
    TRIANGLES = GL_TRIANGLES
};

struct PassCode
{
    std::string VertexShader;
    std::string GeometryShader;
    std::string FragmentShader;
    std::string passName;
};

struct WriteColor
{
    GLboolean r = GL_TRUE;
    GLboolean g = GL_TRUE;
    GLboolean b = GL_TRUE;
    GLboolean a = GL_TRUE;
};

struct PassOption
{
    // 开关
    bool enableDepthTest = true;
    bool enableStencilTest = false;

    // 深度测试
    Func zTest = Func::LESS;
    bool ZWrite = true;

    // 面剔除
    CullMode cullMode = CullMode::OFF;

    // 模板测试
    int BitMask = 0;
    int AndMask = 0xff;
    Func StencilFunc = Func::ALWAYS;
    int Stencilref = 1;
    StencilOp stencilFail = StencilOp::KEEP;   // 模板测试失败
    StencilOp stencilDpFail = StencilOp::KEEP; // 模板通过,深度失败
    StencilOp stencilDpPass = StencilOp::KEEP; // 模板深度都通过

    // 混合
    bool enableBlend = false;
    BlendFunc sFactor = BlendFunc::ZERO;
    BlendFunc dFactor = BlendFunc::ZERO;
    BlendEq blendEq = BlendEq::ADD;

    // 颜色通道
    WriteColor writeColor;

    // 绘制模式
    DrawMode mode = DrawMode::TRIANGLES;

    /// Pass 级渲染队列；-1 表示未设置，由 Shader 级或混合状态推断。
    int renderQueue = -1;
    // Pass绘制次数
    int DrawTimes = 1;

    /// Pass Tags（如 LightMode）；键为 CanonicalizeShaderTagKey 后的形式。
    ShaderTagMap tags;
};

class ShaderPass
{
  public:
    ShaderPass(const PassCode& code, PassOption option = PassOption(), const unsigned int index = 0);
    ~ShaderPass();
    void LoadFromCode(const char* vs, const char* fs);
    void LoadFromCode(const char* vs, const char* fs, const char* gs);
    void use();
    void SetValue(const std::string& name, const glm::vec3& value) const;
    void SetValue(const std::string& name, const glm::vec2& value) const;
    void SetValue(const std::string& name, float x, float y, float z) const;
    void SetValue(const std::string& name, float value) const;
    void SetValue(const std::string& name, const glm::vec4& value) const;
    void SetValue(const std::string& name, float x, float y, float z, float w) const;
    void SetValue(const std::string& name, int value) const;
    void SetValue(const std::string& name, const glm::mat4& value) const;
    void SetValue(const std::string& name, const glm::mat3& value) const;
    PassOption& GetOptions();
    const PassOption& GetOptions() const;
    void SetOptions(PassOption options);
    void SetName(const std::string& name);
    const std::string& GetName();
    void SetState() const;
    bool IsReady() const;
    // 与状态缓存协同：当外部代码直接修改了 GL 状态，可调用此接口同步/失效缓存
    static void SetGlobalColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
    static void InvalidateStateCache();

  public:
    unsigned int m_passIndex;

  private:
    unsigned int m_id;
    PassOption m_options;
    std::string m_name;
    bool m_IsReady = false;
};

inline bool CheckShaderCompileState(unsigned int ID, SHADERTYPE type);