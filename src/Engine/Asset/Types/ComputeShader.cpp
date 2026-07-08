#include "ComputeShader.h"
#include "../../Core/Config.h"
#include "../../Core/Log.h"
#include "ShaderPass.h"
#include <glad/glad.h>

#define MODULE "ComputeShader"

namespace
{
bool ShaderSourceHasVersionDirective(const std::string& code)
{
    size_t pos = 0;
    while (pos < code.size())
    {
        const char c = code[pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            ++pos;
            continue;
        }
        return code.compare(pos, 8, "#version") == 0;
    }
    return false;
}

std::string PrependGlslVersionIfNeeded(std::string code)
{
    if (ShaderSourceHasVersionDirective(code))
        return code;
    return Config::Get().GetGlslVersionString() + "\n" + code;
}
} // namespace

ComputeShader::ComputeShader() = default;

ComputeShader::ComputeShader(const char* code)
{
    LoadFromCode(code);
}

ComputeShader::~ComputeShader()
{
    if (m_id != 0)
        glDeleteProgram(m_id);
}

bool ComputeShader::LoadFromCode(const char* code)
{
    m_isReady = false;
    if (code == nullptr || code[0] == '\0')
    {
        LogA(LogLevel::ERROR, "Compute shader code is empty: {}", m_name);
        return false;
    }

    if (m_id != 0)
    {
        glDeleteProgram(m_id);
        m_id = 0;
    }

    m_id = glCreateProgram();
    const std::string source = PrependGlslVersionIfNeeded(code);
    const char* sourcePtr = source.c_str();
    unsigned int compute = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(compute, 1, &sourcePtr, NULL);
    glCompileShader(compute);
    if (!CheckShaderCompileState(compute, SHADERTYPE::COMPUTE))
    {
        LogA(LogLevel::ERROR, "Failed to compile compute shader: {}", m_name);
        glDeleteShader(compute);
        glDeleteProgram(m_id);
        m_id = 0;
        return false;
    }

    glAttachShader(m_id, compute);
    glLinkProgram(m_id);
    glDeleteShader(compute);
    if (!CheckShaderCompileState(m_id, SHADERTYPE::PROGRAM))
    {
        LogA(LogLevel::ERROR, "Failed to link compute shader: {}", m_name);
        glDeleteProgram(m_id);
        m_id = 0;
        return false;
    }

    m_isReady = true;
    return true;
}

void ComputeShader::Use()
{
    if (m_isReady)
        glUseProgram(m_id);
}

#undef MODULE
