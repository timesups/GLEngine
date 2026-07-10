#include "ShaderPass.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../../Core/Log.h"

#define MODULE "Shader"

namespace
{
struct GLStateCache
{
    bool initialized = false;

    // enable flags
    bool depthTestEnabled = true;
    bool cullEnabled = false;
    bool stencilEnabled = false;
    bool blendEnabled = false;

    // depth
    bool depthMask = true;
    GLenum depthFunc = GL_LESS;

    // cull
    GLenum cullFace = GL_BACK;

    // stencil
    GLuint stencilWriteMask = 0;
    GLenum stencilFunc = GL_ALWAYS;
    GLint stencilRef = 0;
    GLuint stencilFuncMask = 0xFF;
    GLenum stencilFail = GL_KEEP;
    GLenum stencilDpFail = GL_KEEP;
    GLenum stencilDpPass = GL_KEEP;

    // blend
    GLenum blendSrc = GL_ONE;
    GLenum blendDst = GL_ZERO;
    GLenum blendEq = GL_FUNC_ADD;

    // color mask
    GLboolean colorMaskR = GL_TRUE;
    GLboolean colorMaskG = GL_TRUE;
    GLboolean colorMaskB = GL_TRUE;
    GLboolean colorMaskA = GL_TRUE;

    void Apply(const PassOption& opt)
    {
        // 深度测试
        if (!initialized || depthTestEnabled != opt.enableDepthTest)
        {
            depthTestEnabled = opt.enableDepthTest;
            if (depthTestEnabled)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
        }
        if (depthTestEnabled)
        {
            const GLboolean desiredMask = static_cast<GLboolean>(opt.ZWrite);
            const GLenum desiredFunc = static_cast<GLenum>(opt.zTest);
            if (!initialized || depthMask != (desiredMask == GL_TRUE))
            {
                depthMask = (desiredMask == GL_TRUE);
                glDepthMask(desiredMask);
            }
            if (!initialized || depthFunc != desiredFunc)
            {
                depthFunc = desiredFunc;
                glDepthFunc(desiredFunc);
            }
        }
        else
        {
            // 关闭深度测试时通常也不写深度，避免污染深度缓冲
            if (!initialized || depthMask != false)
            {
                depthMask = false;
                glDepthMask(GL_FALSE);
            }
        }

        // 面剔除
        const bool desiredCullEnabled = (opt.cullMode != CullMode::OFF);
        if (!initialized || cullEnabled != desiredCullEnabled)
        {
            cullEnabled = desiredCullEnabled;
            if (cullEnabled)
                glEnable(GL_CULL_FACE);
            else
                glDisable(GL_CULL_FACE);
        }
        if (cullEnabled)
        {
            const GLenum desiredCullFace = static_cast<GLenum>(opt.cullMode);
            if (!initialized || cullFace != desiredCullFace)
            {
                cullFace = desiredCullFace;
                glCullFace(desiredCullFace);
            }
        }

        // 模板测试
        if (!initialized || stencilEnabled != opt.enableStencilTest)
        {
            stencilEnabled = opt.enableStencilTest;
            if (stencilEnabled)
                glEnable(GL_STENCIL_TEST);
            else
                glDisable(GL_STENCIL_TEST);
        }
        if (stencilEnabled)
        {
            const GLuint desiredWriteMask = static_cast<GLuint>(opt.BitMask);
            const GLenum desiredFunc = static_cast<GLenum>(opt.StencilFunc);
            const GLint desiredRef = opt.Stencilref;
            const GLuint desiredFuncMask = static_cast<GLuint>(opt.AndMask);
            const GLenum desiredFail = static_cast<GLenum>(opt.stencilFail);
            const GLenum desiredDpFail = static_cast<GLenum>(opt.stencilDpFail);
            const GLenum desiredDpPass = static_cast<GLenum>(opt.stencilDpPass);

            if (!initialized || stencilWriteMask != desiredWriteMask)
            {
                stencilWriteMask = desiredWriteMask;
                glStencilMask(desiredWriteMask);
            }
            if (!initialized || stencilFunc != desiredFunc || stencilRef != desiredRef ||
                stencilFuncMask != desiredFuncMask)
            {
                stencilFunc = desiredFunc;
                stencilRef = desiredRef;
                stencilFuncMask = desiredFuncMask;
                glStencilFunc(desiredFunc, desiredRef, desiredFuncMask);
            }
            if (!initialized || stencilFail != desiredFail || stencilDpFail != desiredDpFail ||
                stencilDpPass != desiredDpPass)
            {
                stencilFail = desiredFail;
                stencilDpFail = desiredDpFail;
                stencilDpPass = desiredDpPass;
                glStencilOp(desiredFail, desiredDpFail, desiredDpPass);
            }
        }
        else
        {
            // 尽量保证关闭 stencil 时不会写入
            if (!initialized || stencilWriteMask != 0)
            {
                stencilWriteMask = 0;
                glStencilMask(0);
            }
        }

        // 混合
        if (!initialized || blendEnabled != opt.enableBlend)
        {
            blendEnabled = opt.enableBlend;
            if (blendEnabled)
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);
        }
        if (blendEnabled)
        {
            const GLenum desiredSrc = static_cast<GLenum>(opt.sFactor);
            const GLenum desiredDst = static_cast<GLenum>(opt.dFactor);
            const GLenum desiredEq = static_cast<GLenum>(opt.blendEq);
            if (!initialized || blendSrc != desiredSrc || blendDst != desiredDst)
            {
                blendSrc = desiredSrc;
                blendDst = desiredDst;
                glBlendFunc(desiredSrc, desiredDst);
            }
            if (!initialized || blendEq != desiredEq)
            {
                blendEq = desiredEq;
                glBlendEquation(desiredEq);
            }
        }

        // 颜色通道写入
        if (!initialized || colorMaskR != opt.writeColor.r || colorMaskG != opt.writeColor.g ||
            colorMaskB != opt.writeColor.b || colorMaskA != opt.writeColor.a)
        {
            colorMaskR = opt.writeColor.r;
            colorMaskG = opt.writeColor.g;
            colorMaskB = opt.writeColor.b;
            colorMaskA = opt.writeColor.a;
            glColorMask(colorMaskR, colorMaskG, colorMaskB, colorMaskA);
        }

        initialized = true;
    }
};

// 轻量级全局状态缓存：所有 ShaderPass 共享，避免每个 pass 重复设置同样的状态
GLStateCache g_glStateCache;
} // namespace

ShaderPass::ShaderPass(const PassCode& code, PassOption option, const unsigned int index)
{
    if (code.GeometryShader.empty())
        LoadFromCode(code.VertexShader.c_str(), code.FragmentShader.c_str());
    else
        LoadFromCode(code.VertexShader.c_str(), code.FragmentShader.c_str(), code.GeometryShader.c_str());
    m_name = code.passName;
    this->m_options = option;
    if (m_IsReady)
        Log(MODULE, LogLevel::INFO, "ShaderPass '{}' ready (program id={})", m_name, m_id);
    else
        Log(MODULE, LogLevel::ERROR, "ShaderPass '{}' failed to compile/link", m_name);

    // set init uniform value
    m_passIndex = index;
}

ShaderPass::~ShaderPass()
{
    if (m_id != 0)
        glDeleteProgram(m_id);
}

void ShaderPass::LoadFromCode(const char* vs, const char* fs)
{
    unsigned int vid, fid;
    vid = glCreateShader(GL_VERTEX_SHADER);
    fid = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vid, 1, &vs, NULL);
    glShaderSource(fid, 1, &fs, NULL);

    glCompileShader(vid);
    if (!CheckShaderCompileState(vid, VERTEX))
    {
        m_IsReady = false;
        return;
    }

    glCompileShader(fid);
    if (!CheckShaderCompileState(fid, FRAGMENT))
    {
        m_IsReady = false;
        return;
    }
    m_id = glCreateProgram();
    glAttachShader(m_id, vid);
    glAttachShader(m_id, fid);
    glLinkProgram(m_id);
    if (!CheckShaderCompileState(m_id, PROGRAM))
    {
        m_IsReady = false;
        return;
    }
    glDeleteShader(vid);
    glDeleteShader(fid);

    m_IsReady = true;
}

void ShaderPass::LoadFromCode(const char* vs, const char* fs, const char* gs)
{
    unsigned int vid, fid, gid;
    vid = glCreateShader(GL_VERTEX_SHADER);
    fid = glCreateShader(GL_FRAGMENT_SHADER);
    gid = glCreateShader(GL_GEOMETRY_SHADER);

    glShaderSource(vid, 1, &vs, NULL);
    glShaderSource(fid, 1, &fs, NULL);
    glShaderSource(gid, 1, &gs, NULL);

    glCompileShader(vid);
    if (!CheckShaderCompileState(vid, VERTEX))
    {
        m_IsReady = false;
        return;
    }
    glCompileShader(fid);
    if (!CheckShaderCompileState(fid, FRAGMENT))
    {
        m_IsReady = false;
        return;
    }
    glCompileShader(gid);
    if (!CheckShaderCompileState(gid, GEOMETRY))
    {
        m_IsReady = false;
        return;
    }

    m_id = glCreateProgram();
    glAttachShader(m_id, vid);
    glAttachShader(m_id, fid);
    glAttachShader(m_id, gid);
    glLinkProgram(m_id);
    if (!CheckShaderCompileState(m_id, PROGRAM))
    {
        m_IsReady = false;
        return;
    }

    glDeleteShader(vid);
    glDeleteShader(fid);
    glDeleteShader(gid);
    m_IsReady = true;
}

void ShaderPass::use()
{
    glUseProgram(m_id);
}
void ShaderPass::SetValue(const std::string& name, const glm::vec3& value) const
{
    glUniform3f(glGetUniformLocation(m_id, name.c_str()), value.x, value.y, value.z);
}
void ShaderPass::SetValue(const std::string& name, const glm::vec2& value) const
{
    glUniform2f(glGetUniformLocation(m_id, name.c_str()), value.x, value.y);
}
void ShaderPass::SetValue(const std::string& name, float x, float y, float z) const
{
    glUniform3f(glGetUniformLocation(m_id, name.c_str()), x, y, z);
}
void ShaderPass::SetValue(const std::string& name, float value) const
{
    glUniform1f(glGetUniformLocation(m_id, name.c_str()), value);
}
void ShaderPass::SetValue(const std::string& name, const glm::vec4& value) const
{
    glUniform4f(glGetUniformLocation(m_id, name.c_str()), value.x, value.y, value.z, value.w);
}
void ShaderPass::SetValue(const std::string& name, float x, float y, float z, float w) const
{
    glUniform4f(glGetUniformLocation(m_id, name.c_str()), x, y, z, w);
}
void ShaderPass::SetValue(const std::string& name, int value) const
{
    glUniform1i(glGetUniformLocation(m_id, name.c_str()), value);
}
void ShaderPass::SetValue(const std::string& name, const glm::mat4& value) const
{
    glUniformMatrix4fv(glGetUniformLocation(m_id, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}
void ShaderPass::SetValue(const std::string& name, const glm::mat3& value) const
{
    glUniformMatrix3fv(glGetUniformLocation(m_id, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}
PassOption& ShaderPass::GetOptions()
{
    return m_options;
}
const PassOption& ShaderPass::GetOptions() const
{
    return m_options;
}
void ShaderPass::SetOptions(PassOption options)
{
    m_options = options;
}
void ShaderPass::SetName(const std::string& name)
{
    m_name = name;
}
const std::string& ShaderPass::GetName()
{
    return m_name;
}
void ShaderPass::SetState() const
{
    g_glStateCache.Apply(m_options);
}

bool ShaderPass::IsReady() const
{
    return m_IsReady;
}

void ShaderPass::SetGlobalColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    glColorMask(r, g, b, a);

    // 如果缓存已初始化，保持缓存与真实状态一致；未初始化则不强行置 initialized，
    // 让后续第一个 pass 仍能完整写入所有状态。
    g_glStateCache.colorMaskR = r;
    g_glStateCache.colorMaskG = g;
    g_glStateCache.colorMaskB = b;
    g_glStateCache.colorMaskA = a;
}

void ShaderPass::InvalidateStateCache()
{
    g_glStateCache.initialized = false;
}

bool CheckShaderCompileState(unsigned int ID, SHADERTYPE type)
{
    int success;
    char infoLog[512];

    if (type == PROGRAM)
    {
        glGetProgramiv(ID, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(ID, 512, NULL, infoLog);
            Log(MODULE, LogLevel::ERROR, "ERROR:SHADER::PROGRAM::LINK_FAIED\n{}", infoLog);
        }
    }
    else
    {
        glGetShaderiv(ID, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(ID, 512, NULL, infoLog);
            switch (type)
            {
            case VERTEX:
                Log(MODULE, LogLevel::ERROR, "ERROR::SHADER::VERTEX::COMPIATION_FAILED\n{}", infoLog);
                break;
            case FRAGMENT:
                Log(MODULE, LogLevel::ERROR, "ERROR::SHADER::FRAGMENT::COMPIATION_FAILED\n{}", infoLog);
                break;
            case GEOMETRY:
                Log(MODULE, LogLevel::ERROR, "ERROR::SHADER::GEOMETRY::COMPIATION_FAILED\n{}", infoLog);
                break;
            case COMPUTE:
                Log(MODULE, LogLevel::ERROR, "ERROR::SHADER::COMPUTE::COMPIATION_FAILED\n{}", infoLog);
                break;
            default:
                break;
            }
        }
    }
    return success;
}

#undef MODULE