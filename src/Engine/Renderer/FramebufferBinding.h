#pragma once

#include "FramebufferDesc.h"
#include <memory>
#include <string>
#include <vector>

class Texture;
class TextureCube;
class RenderBuffer;

enum class FramebufferBlitMask : unsigned int
{
    None = 0,
    Color = 1u << 0,
    Depth = 1u << 1,
    Stencil = 1u << 2,
};

FramebufferBlitMask operator|(FramebufferBlitMask a, FramebufferBlitMask b);
FramebufferBlitMask operator&(FramebufferBlitMask a, FramebufferBlitMask b);
bool HasBlitFlag(FramebufferBlitMask mask, FramebufferBlitMask flag);

enum class AttachmentSource
{
    Texture,
    RenderBuffer,
};

struct FramebufferAttachmentBinding
{
    GLenum point = GL_COLOR_ATTACHMENT0;
    AttachmentSource source = AttachmentSource::Texture;
    std::shared_ptr<Texture> texture;
    std::shared_ptr<RenderBuffer> renderBuffer;
    GLenum textureTarget = GL_TEXTURE_2D;
    int cubeFace = -1;
    int layer = 0;
    int mipLevel = 0;
    GBufferTarget gbufferTag = GBufferTarget::Count;
};

class Framebuffer
{
  public:
    Framebuffer();
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void SetName(const std::string& name);
    void SetSize(int width, int height);
    void SetDrawBuffers(const std::vector<GLenum>& drawBuffers);

    bool Attach(const FramebufferAttachmentBinding& binding);
    bool Detach(GLenum point);
    void ClearAttachments();

    bool BindCubemapFace(const std::shared_ptr<TextureCube>& tex, int face,
                         GLenum point = GL_COLOR_ATTACHMENT0, int mip = 0);
    /// 要求 FBO 已绑定；用于连续渲染多个 cubemap face
    bool AttachCubemapFace(GLenum point, unsigned int textureId, int face, int mip = 0);
    bool BindTextureLayer(GLenum point, const std::shared_ptr<Texture>& texture, GLenum target, int layer, int mip = 0);

    void Bind(bool debug = false);
    void Unbind();

    /// 绑定后根据 m_drawBuffers 设置 GL draw buffer（Bind 内自动调用）
    void ApplyDrawBuffers();

    void BlitTo(const Framebuffer& dst, FramebufferBlitMask mask) const;
    bool CheckComplete() const;

    bool HasColorAttachment(int index = 0) const;
    bool HasDepthAttachment() const;
    bool HasStencilAttachment() const;

    const FramebufferAttachmentBinding* FindColorAttachment(int index) const;
    const FramebufferAttachmentBinding* FindDepthAttachment() const;
    const FramebufferAttachmentBinding* FindAttachment(GLenum point) const;

    GLuint GetId() const;
    int Width() const;
    int Height() const;
    const std::string& Name() const;
    const std::vector<FramebufferAttachmentBinding>& Attachments() const;

  private:
    bool AttachTexture(const FramebufferAttachmentBinding& binding);
    bool AttachRenderBuffer(const FramebufferAttachmentBinding& binding);
    void RebuildDrawBuffersFromAttachments();

    std::vector<FramebufferAttachmentBinding> m_attachments;
    std::vector<GLenum> m_drawBuffers;
    bool m_autoDrawBuffers = true;

    unsigned int m_fbo = 0;
    int m_width = 0;
    int m_height = 0;
    std::string m_name;
    bool m_debug = false;

    struct SavedBindingState
    {
        GLint framebuffer = 0;
        GLint viewport[4] = {0, 0, 0, 0};
        GLint drawBuffer = GL_BACK;
        bool active = false;
    };
    SavedBindingState m_savedBinding;
};
