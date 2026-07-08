#include "FramebufferBinding.h"

#include "../Asset/Types/Texture/TextureCube.h"
#include "../Core/Log.h"
#include <algorithm>
#include <glad/glad.h>

#define MODULE "Framebuffer"

namespace
{
bool IsDepthAttachmentPoint(GLenum point)
{
    return point == GL_DEPTH_ATTACHMENT || point == GL_DEPTH_STENCIL_ATTACHMENT;
}

bool IsColorAttachmentPoint(GLenum point)
{
    return point >= GL_COLOR_ATTACHMENT0 && point <= GL_COLOR_ATTACHMENT15;
}

bool IsStencilAttachmentPoint(GLenum point)
{
    return point == GL_STENCIL_ATTACHMENT || point == GL_DEPTH_STENCIL_ATTACHMENT;
}

const char* FramebufferStatusName(GLenum status)
{
    switch (status)
    {
    case GL_FRAMEBUFFER_COMPLETE:
        return "COMPLETE";
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        return "INCOMPLETE_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        return "INCOMPLETE_MISSING_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        return "INCOMPLETE_DRAW_BUFFER";
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        return "INCOMPLETE_READ_BUFFER";
    case GL_FRAMEBUFFER_UNSUPPORTED:
        return "UNSUPPORTED";
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        return "INCOMPLETE_MULTISAMPLE";
    case 0x8DA8:
        return "INCOMPLETE_LAYER_TARGETS";
    case 0x8DA9:
        return "INCOMPLETE_LAYER_COUNT";
    default:
        return "UNKNOWN";
    }
}
} // namespace

FramebufferBlitMask operator|(FramebufferBlitMask a, FramebufferBlitMask b)
{
    return static_cast<FramebufferBlitMask>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

FramebufferBlitMask operator&(FramebufferBlitMask a, FramebufferBlitMask b)
{
    return static_cast<FramebufferBlitMask>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}

bool HasBlitFlag(FramebufferBlitMask mask, FramebufferBlitMask flag)
{
    return (static_cast<unsigned int>(mask) & static_cast<unsigned int>(flag)) != 0;
}

Framebuffer::Framebuffer()
{
    glGenFramebuffers(1, &m_fbo);
}

Framebuffer::~Framebuffer()
{
    if (m_fbo != 0)
        glDeleteFramebuffers(1, &m_fbo);
}

void Framebuffer::SetName(const std::string& name)
{
    m_name = name;
}

void Framebuffer::SetSize(int width, int height)
{
    m_width = width;
    m_height = height;
}

void Framebuffer::SetDrawBuffers(const std::vector<GLenum>& drawBuffers)
{
    m_drawBuffers = drawBuffers;
    m_autoDrawBuffers = drawBuffers.empty();
}

bool Framebuffer::AttachTexture(const FramebufferAttachmentBinding& binding)
{
    if (!binding.texture || binding.texture->GetId() == 0)
    {
        Log(MODULE, LogLevel::ERROR, "Framebuffer '{}': invalid texture for attachment 0x{:X}", m_name,
            static_cast<unsigned>(binding.point));
        return false;
    }

    const GLuint texId = binding.texture->GetId();
    while (glGetError() != GL_NO_ERROR)
    {
    }

    switch (binding.textureTarget)
    {
    case GL_TEXTURE_2D:
        glFramebufferTexture2D(GL_FRAMEBUFFER, binding.point, GL_TEXTURE_2D, texId, binding.mipLevel);
        break;
    case GL_TEXTURE_CUBE_MAP:
        if (binding.cubeFace >= 0)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, binding.point,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + binding.cubeFace, texId, binding.mipLevel);
        }
        else
        {
            glFramebufferTexture(GL_FRAMEBUFFER, binding.point, texId, binding.mipLevel);
        }
        break;
    case GL_TEXTURE_CUBE_MAP_ARRAY:
        // 点光立方体阴影：几何着色器通过 gl_Layer 写入多面，须绑定整张贴图而非单层
        glFramebufferTexture(GL_FRAMEBUFFER, binding.point, texId, binding.mipLevel);
        break;
    case GL_TEXTURE_2D_ARRAY:
        glFramebufferTextureLayer(GL_FRAMEBUFFER, binding.point, texId, binding.mipLevel, binding.layer);
        break;
    default:
        Log(MODULE, LogLevel::ERROR, "Framebuffer '{}': unsupported texture target 0x{:X}", m_name,
            static_cast<unsigned>(binding.textureTarget));
        return false;
    }

    const GLenum fbErr = glGetError();
    if (fbErr != GL_NO_ERROR)
    {
        Log(MODULE, LogLevel::ERROR, "Framebuffer '{}': texture attach failed glError=0x{:X}", m_name,
            static_cast<unsigned>(fbErr));
        return false;
    }
    return true;
}

bool Framebuffer::AttachRenderBuffer(const FramebufferAttachmentBinding& binding)
{
    if (!binding.renderBuffer || binding.renderBuffer->GetId() == 0)
    {
        Log(MODULE, LogLevel::ERROR, "Framebuffer '{}': invalid renderbuffer for attachment 0x{:X}", m_name,
            static_cast<unsigned>(binding.point));
        return false;
    }

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, binding.point, GL_RENDERBUFFER, binding.renderBuffer->GetId());

    const GLenum fbErr = glGetError();
    if (fbErr != GL_NO_ERROR)
    {
        Log(MODULE, LogLevel::ERROR, "Framebuffer '{}': renderbuffer attach failed glError=0x{:X}", m_name,
            static_cast<unsigned>(fbErr));
        return false;
    }
    return true;
}

bool Framebuffer::Attach(const FramebufferAttachmentBinding& binding)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    bool ok = false;
    if (binding.source == AttachmentSource::Texture)
        ok = AttachTexture(binding);
    else
        ok = AttachRenderBuffer(binding);

    if (!ok)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    auto existing = std::find_if(m_attachments.begin(), m_attachments.end(),
                                 [&](const FramebufferAttachmentBinding& item) { return item.point == binding.point; });
    if (existing != m_attachments.end())
        *existing = binding;
    else
        m_attachments.push_back(binding);

    if (m_autoDrawBuffers)
        RebuildDrawBuffersFromAttachments();

    ApplyDrawBuffers();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool Framebuffer::Detach(GLenum point)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, point, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_attachments.erase(
        std::remove_if(m_attachments.begin(), m_attachments.end(),
                       [point](const FramebufferAttachmentBinding& item) { return item.point == point; }),
        m_attachments.end());

    if (m_autoDrawBuffers)
        RebuildDrawBuffersFromAttachments();
    return true;
}

void Framebuffer::ClearAttachments()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    for (const FramebufferAttachmentBinding& attachment : m_attachments)
    {
        if (attachment.source == AttachmentSource::RenderBuffer)
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment.point, GL_RENDERBUFFER, 0);
        else
            glFramebufferTexture(GL_FRAMEBUFFER, attachment.point, 0, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_attachments.clear();
}

void Framebuffer::RebuildDrawBuffersFromAttachments()
{
    m_drawBuffers.clear();
    for (const FramebufferAttachmentBinding& attachment : m_attachments)
    {
        if (IsColorAttachmentPoint(attachment.point))
            m_drawBuffers.push_back(attachment.point);
    }
}

void Framebuffer::ApplyDrawBuffers()
{
    if (m_drawBuffers.empty())
    {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    else
    {
        glDrawBuffers(static_cast<GLsizei>(m_drawBuffers.size()), m_drawBuffers.data());
    }
}

bool Framebuffer::BindCubemapFace(const std::shared_ptr<TextureCube>& tex, int face, GLenum point, int mip)
{
    if (!tex || face < 0 || face > 5)
        return false;

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    const bool ok = AttachCubemapFace(point, tex->GetId(), face, mip);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return ok;
}

bool Framebuffer::AttachCubemapFace(GLenum point, unsigned int textureId, int face, int mip)
{
    if (face < 0 || face > 5 || textureId == 0)
        return false;

    glFramebufferTexture2D(GL_FRAMEBUFFER, point, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, textureId, mip);
    return CheckComplete();
}

bool Framebuffer::BindTextureLayer(GLenum point, const std::shared_ptr<Texture>& texture, GLenum target, int layer,
                                   int mip)
{
    if (!texture)
        return false;

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, point, texture->GetId(), mip, layer);
    const bool ok = CheckComplete();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return ok;
}

void Framebuffer::Bind(bool debug)
{
    if (!m_savedBinding.active)
    {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_savedBinding.framebuffer);
        glGetIntegerv(GL_VIEWPORT, m_savedBinding.viewport);
        glGetIntegerv(GL_DRAW_BUFFER, &m_savedBinding.drawBuffer);
        m_savedBinding.active = true;
    }

    m_debug = debug;
    if (debug)
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, m_name.c_str());

    glViewport(0, 0, m_width, m_height);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    ApplyDrawBuffers();
}

void Framebuffer::Unbind()
{
    if (m_debug)
        glPopDebugGroup();

    if (m_savedBinding.active)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_savedBinding.framebuffer));
        glViewport(m_savedBinding.viewport[0], m_savedBinding.viewport[1], m_savedBinding.viewport[2],
                   m_savedBinding.viewport[3]);
        glDrawBuffer(static_cast<GLenum>(m_savedBinding.drawBuffer));
        m_savedBinding.active = false;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
}

bool Framebuffer::CheckComplete() const
{
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        Log(MODULE, LogLevel::ERROR, "Framebuffer incomplete ({}): {} (0x{:X})", m_name, FramebufferStatusName(status),
            static_cast<unsigned>(status));
        return false;
    }
    return true;
}

const FramebufferAttachmentBinding* Framebuffer::FindAttachment(GLenum point) const
{
    for (const FramebufferAttachmentBinding& attachment : m_attachments)
    {
        if (attachment.point == point)
            return &attachment;
    }
    return nullptr;
}

const FramebufferAttachmentBinding* Framebuffer::FindColorAttachment(int index) const
{
    int colorIndex = 0;
    for (const FramebufferAttachmentBinding& attachment : m_attachments)
    {
        if (!IsColorAttachmentPoint(attachment.point))
            continue;
        if (colorIndex == index)
            return &attachment;
        ++colorIndex;
    }
    return nullptr;
}

const FramebufferAttachmentBinding* Framebuffer::FindDepthAttachment() const
{
    for (const FramebufferAttachmentBinding& attachment : m_attachments)
    {
        if (IsDepthAttachmentPoint(attachment.point))
            return &attachment;
    }
    return nullptr;
}

bool Framebuffer::HasColorAttachment(int index) const
{
    return FindColorAttachment(index) != nullptr;
}

bool Framebuffer::HasDepthAttachment() const
{
    return FindDepthAttachment() != nullptr;
}

bool Framebuffer::HasStencilAttachment() const
{
    for (const FramebufferAttachmentBinding& attachment : m_attachments)
    {
        if (IsStencilAttachmentPoint(attachment.point))
            return true;
    }
    return false;
}

GLuint Framebuffer::GetId() const
{
    return m_fbo;
}

int Framebuffer::Width() const
{
    return m_width;
}

int Framebuffer::Height() const
{
    return m_height;
}

const std::string& Framebuffer::Name() const
{
    return m_name;
}

const std::vector<FramebufferAttachmentBinding>& Framebuffer::Attachments() const
{
    return m_attachments;
}

void Framebuffer::BlitTo(const Framebuffer& dst, FramebufferBlitMask mask) const
{
    if (mask == FramebufferBlitMask::None)
    {
        Log(MODULE, LogLevel::WARNING, "BlitTo skipped: empty mask ({} -> {})", m_name, dst.m_name);
        return;
    }
    if (m_fbo == 0 || dst.m_fbo == 0)
    {
        Log(MODULE, LogLevel::WARNING, "BlitTo skipped: missing FBO ({} -> {})", m_name, dst.m_name);
        return;
    }

    GLbitfield glMask = 0;
    if (HasBlitFlag(mask, FramebufferBlitMask::Color))
    {
        if (!HasColorAttachment())
        {
            Log(MODULE, LogLevel::WARNING, "BlitTo skipped: src has no color attachment ({})", m_name);
            return;
        }
        if (!dst.HasColorAttachment())
        {
            Log(MODULE, LogLevel::WARNING, "BlitTo skipped: dst has no color attachment ({})", dst.m_name);
            return;
        }
        glMask |= GL_COLOR_BUFFER_BIT;
    }
    if (HasBlitFlag(mask, FramebufferBlitMask::Depth))
    {
        if (!HasDepthAttachment())
        {
            Log(MODULE, LogLevel::WARNING, "BlitTo skipped: src has no depth attachment ({})", m_name);
            return;
        }
        if (!dst.HasDepthAttachment())
        {
            Log(MODULE, LogLevel::WARNING, "BlitTo skipped: dst has no depth attachment ({})", dst.m_name);
            return;
        }
        glMask |= GL_DEPTH_BUFFER_BIT;
    }
    if (HasBlitFlag(mask, FramebufferBlitMask::Stencil))
    {
        if (!HasStencilAttachment())
        {
            Log(MODULE, LogLevel::WARNING, "BlitTo skipped: src has no stencil attachment ({})", m_name);
            return;
        }
        if (!dst.HasStencilAttachment())
        {
            Log(MODULE, LogLevel::WARNING, "BlitTo skipped: dst has no stencil attachment ({})", dst.m_name);
            return;
        }
        glMask |= GL_STENCIL_BUFFER_BIT;
    }

    GLint prevRead = 0;
    GLint prevDraw = 0;
    GLint prevReadBuffer = GL_NONE;
    GLint prevDrawBuffer = GL_NONE;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);

    const bool blitColor = (glMask & GL_COLOR_BUFFER_BIT) != 0;
    if (blitColor)
    {
        glGetIntegerv(GL_READ_BUFFER, &prevReadBuffer);
        glGetIntegerv(GL_DRAW_BUFFER, &prevDrawBuffer);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.m_fbo);

    if (blitColor)
    {
        const GLenum srcColorPoint = FindColorAttachment(0)->point;
        const GLenum dstColorPoint = dst.FindColorAttachment(0)->point;
        glReadBuffer(srcColorPoint);
        glDrawBuffer(dstColorPoint);
    }

    const GLenum filter = (glMask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0 ? GL_NEAREST : GL_LINEAR;

    glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, dst.m_width, dst.m_height, glMask, filter);

    if (blitColor)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(prevRead));
        glReadBuffer(static_cast<GLenum>(prevReadBuffer));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(prevDraw));
        glDrawBuffer(static_cast<GLenum>(prevDrawBuffer));
    }
    else
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(prevRead));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(prevDraw));
    }
}

void Framebuffer::BlitColorAttachmentTo(const Framebuffer& dst, int srcColorIndex, int dstColorIndex,
                                        FramebufferBlitMask mask) const
{
    if (mask == FramebufferBlitMask::None)
    {
        Log(MODULE, LogLevel::WARNING, "BlitColorAttachmentTo skipped: empty mask ({} -> {})", m_name, dst.m_name);
        return;
    }
    if (m_fbo == 0 || dst.m_fbo == 0)
    {
        Log(MODULE, LogLevel::WARNING, "BlitColorAttachmentTo skipped: missing FBO ({} -> {})", m_name, dst.m_name);
        return;
    }

    GLbitfield glMask = 0;
    const bool blitColor = HasBlitFlag(mask, FramebufferBlitMask::Color);
    if (blitColor)
    {
        if (!FindColorAttachment(srcColorIndex) || !dst.FindColorAttachment(dstColorIndex))
        {
            Log(MODULE, LogLevel::WARNING, "BlitColorAttachmentTo skipped: color index {} -> {} ({} -> {})", srcColorIndex,
                dstColorIndex, m_name, dst.m_name);
            return;
        }
        glMask |= GL_COLOR_BUFFER_BIT;
    }
    if (HasBlitFlag(mask, FramebufferBlitMask::Depth))
    {
        if (!HasDepthAttachment() || !dst.HasDepthAttachment())
        {
            Log(MODULE, LogLevel::WARNING, "BlitColorAttachmentTo skipped: depth missing ({} -> {})", m_name, dst.m_name);
            return;
        }
        glMask |= GL_DEPTH_BUFFER_BIT;
    }
    if (HasBlitFlag(mask, FramebufferBlitMask::Stencil))
        glMask |= GL_STENCIL_BUFFER_BIT;

    GLint prevRead = 0;
    GLint prevDraw = 0;
    GLint prevReadBuffer = GL_NONE;
    GLint prevDrawBuffer = GL_NONE;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);

    if (blitColor)
    {
        glGetIntegerv(GL_READ_BUFFER, &prevReadBuffer);
        glGetIntegerv(GL_DRAW_BUFFER, &prevDrawBuffer);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.m_fbo);

    if (blitColor)
    {
        glReadBuffer(FindColorAttachment(srcColorIndex)->point);
        glDrawBuffer(dst.FindColorAttachment(dstColorIndex)->point);
    }

    const GLenum filter = (glMask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0 ? GL_NEAREST : GL_LINEAR;
    glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, dst.m_width, dst.m_height, glMask, filter);

    if (blitColor)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(prevRead));
        glReadBuffer(static_cast<GLenum>(prevReadBuffer));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(prevDraw));
        glDrawBuffer(static_cast<GLenum>(prevDrawBuffer));
    }
    else
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(prevRead));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(prevDraw));
    }
}

#undef MODULE
