#include "RenderBuffer.h"

#include "../Core/Log.h"

#define MODULE "RenderBuffer"

RenderBuffer::RenderBuffer() = default;

RenderBuffer::~RenderBuffer()
{
    Destroy();
}

void RenderBuffer::Destroy()
{
    if (m_id != 0)
    {
        glDeleteRenderbuffers(1, &m_id);
        m_id = 0;
    }
}

bool RenderBuffer::Create(const RenderBufferDesc& desc)
{
    Destroy();

    if (desc.width <= 0 || desc.height <= 0)
    {
        Log(MODULE, LogLevel::ERROR, "RenderBuffer::Create: invalid size {}x{}", desc.width, desc.height);
        return false;
    }

    m_width = desc.width;
    m_height = desc.height;
    m_internalFormat = desc.internalFormat;
    m_samples = desc.samples;

    glGenRenderbuffers(1, &m_id);
    glBindRenderbuffer(GL_RENDERBUFFER, m_id);
    if (m_samples > 0)
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, m_internalFormat, m_width, m_height);
    else
        glRenderbufferStorage(GL_RENDERBUFFER, m_internalFormat, m_width, m_height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    return true;
}

bool RenderBuffer::Resize(int width, int height)
{
    if (width == m_width && height == m_height)
        return true;

    RenderBufferDesc desc;
    desc.width = width;
    desc.height = height;
    desc.internalFormat = m_internalFormat;
    desc.samples = m_samples;
    return Create(desc);
}

GLuint RenderBuffer::GetId() const
{
    return m_id;
}

int RenderBuffer::Width() const
{
    return m_width;
}

int RenderBuffer::Height() const
{
    return m_height;
}

GLenum RenderBuffer::InternalFormat() const
{
    return m_internalFormat;
}

#undef MODULE
