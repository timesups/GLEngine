#include "FramebufferDesc.h"

#include <algorithm>

namespace
{
TextureDesc WithSize(int width, int height, const TextureDesc& desc)
{
    TextureDesc out = desc;
    out.width = width;
    out.height = height;
    return out;
}

void RemoveAttachmentPoint(std::vector<FramebufferAttachmentDesc>& attachments, GLenum point)
{
    attachments.erase(
        std::remove_if(attachments.begin(), attachments.end(),
                       [point](const FramebufferAttachmentDesc& a) { return a.attachmentPoint == point; }),
        attachments.end());
}

void RemoveDepthAttachments(std::vector<FramebufferAttachmentDesc>& attachments)
{
    RemoveAttachmentPoint(attachments, GL_DEPTH_ATTACHMENT);
    RemoveAttachmentPoint(attachments, GL_DEPTH_STENCIL_ATTACHMENT);
}

TextureDesc LinearClampBase(GLenum internalFmt, GLenum format, GLenum type)
{
    TextureDesc desc = TextureDesc::MakeExplicit(0, 0, internalFmt, format, type);
    desc.sampler.minFilter = GL_LINEAR;
    desc.sampler.magFilter = GL_LINEAR;
    desc.sampler.wrapS = GL_CLAMP_TO_EDGE;
    desc.sampler.wrapT = GL_CLAMP_TO_EDGE;
    return desc;
}

TextureDesc NearestClampBase(GLenum internalFmt, GLenum format, GLenum type)
{
    TextureDesc desc = TextureDesc::MakeExplicit(0, 0, internalFmt, format, type);
    desc.sampler.minFilter = GL_NEAREST;
    desc.sampler.magFilter = GL_NEAREST;
    desc.sampler.wrapS = GL_CLAMP_TO_EDGE;
    desc.sampler.wrapT = GL_CLAMP_TO_EDGE;
    desc.sampler.wrapR = GL_CLAMP_TO_EDGE;
    return desc;
}
} // namespace

GLenum FramebufferDesc::AddColorAttachment(const TextureDesc& texture)
{
    const int colorIndex = ColorAttachmentCount();
    const GLenum point = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(colorIndex);

    FramebufferAttachmentDesc attachment;
    attachment.attachmentPoint = point;
    attachment.storage = AttachmentStorage::Texture;
    attachment.texture = WithSize(width, height, texture);
    attachments.push_back(attachment);
    drawBuffers.push_back(point);
    return point;
}

void FramebufferDesc::SetDepthAttachment(const TextureDesc& depthTexture)
{
    RemoveDepthAttachments(attachments);

    FramebufferAttachmentDesc depth;
    depth.attachmentPoint = GL_DEPTH_ATTACHMENT;
    depth.storage = AttachmentStorage::Texture;
    depth.texture = WithSize(width, height, depthTexture);
    attachments.push_back(depth);
}

void FramebufferDesc::SetDepthStencilAttachment(const RenderBufferDesc& renderBuffer)
{
    RemoveDepthAttachments(attachments);

    FramebufferAttachmentDesc depthStencil;
    depthStencil.attachmentPoint = GL_DEPTH_STENCIL_ATTACHMENT;
    depthStencil.storage = AttachmentStorage::RenderBuffer;
    depthStencil.renderBuffer = renderBuffer;
    attachments.push_back(depthStencil);
}

namespace RenderTargetFormats
{
TextureDesc SceneColor()
{
    return LinearClampBase(GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT);
}

TextureDesc ShadowDepth()
{
    return NearestClampBase(GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
}

TextureDesc WaterBack()
{
    return NearestClampBase(GL_RGBA32F, GL_RGBA, GL_FLOAT);
}

TextureDesc BloomHalf()
{
    return SceneColor();
}

FramebufferDesc SceneColorDepth(const std::string& name, int width, int height, const TextureDesc& colorDesc)
{
    FramebufferDesc desc;
    desc.name = name;
    desc.width = width;
    desc.height = height;
    desc.AddColorAttachment(colorDesc);
    desc.SetDepthAttachment(ShadowDepth());
    return desc;
}

FramebufferDesc ColorOnly(const std::string& name, int width, int height, const TextureDesc& colorDesc)
{
    FramebufferDesc desc;
    desc.name = name;
    desc.width = width;
    desc.height = height;
    desc.AddColorAttachment(colorDesc);
    return desc;
}

FramebufferDesc DepthOnly(const std::string& name, int width, int height, const TextureDesc& depthDesc)
{
    FramebufferDesc desc;
    desc.name = name;
    desc.width = width;
    desc.height = height;
    desc.SetDepthAttachment(depthDesc);
    desc.drawBuffers = {};
    return desc;
}

FramebufferDesc GBuffer(const std::string& name, int width, int height)
{
    FramebufferDesc desc;
    desc.name = name;
    desc.width = width;
    desc.height = height;

    desc.AddColorAttachment(LinearClampBase(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE));
    desc.AddColorAttachment(LinearClampBase(GL_RGBA32F, GL_RGBA, GL_FLOAT));
    desc.AddColorAttachment(LinearClampBase(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE));
    desc.AddColorAttachment(LinearClampBase(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE));
    desc.SetDepthAttachment(ShadowDepth());
    return desc;
}

RenderBufferDesc DepthRBO(int width, int height)
{
    RenderBufferDesc desc;
    desc.width = width;
    desc.height = height;
    desc.internalFormat = GL_DEPTH_COMPONENT32F;
    return desc;
}

RenderBufferDesc DepthStencilRBO(int width, int height)
{
    RenderBufferDesc desc;
    desc.width = width;
    desc.height = height;
    desc.internalFormat = GL_DEPTH24_STENCIL8;
    return desc;
}

RenderBufferDesc StencilRBO(int width, int height)
{
    RenderBufferDesc desc;
    desc.width = width;
    desc.height = height;
    desc.internalFormat = GL_STENCIL_INDEX8;
    return desc;
}

FramebufferDesc SceneColorDepthStencil(const std::string& name, int width, int height, const TextureDesc& colorDesc)
{
    FramebufferDesc desc;
    desc.name = name;
    desc.width = width;
    desc.height = height;
    desc.AddColorAttachment(colorDesc);
    desc.SetDepthStencilAttachment(DepthStencilRBO(width, height));
    return desc;
}
} // namespace RenderTargetFormats
