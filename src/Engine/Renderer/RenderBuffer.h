#pragma once

#include <glad/glad.h>

struct RenderBufferDesc
{
    int width = 0;
    int height = 0;
    GLenum internalFormat = GL_DEPTH24_STENCIL8;
    int samples = 0;
};

class RenderBuffer
{
  public:
    RenderBuffer();
    ~RenderBuffer();

    RenderBuffer(const RenderBuffer&) = delete;
    RenderBuffer& operator=(const RenderBuffer&) = delete;

    bool Create(const RenderBufferDesc& desc);
    bool Resize(int width, int height);
    GLuint GetId() const;
    int Width() const;
    int Height() const;
    GLenum InternalFormat() const;

  private:
    void Destroy();

    GLuint m_id = 0;
    int m_width = 0;
    int m_height = 0;
    GLenum m_internalFormat = GL_DEPTH24_STENCIL8;
    int m_samples = 0;
};
