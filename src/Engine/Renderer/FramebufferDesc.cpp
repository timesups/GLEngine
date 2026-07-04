#include "FramebufferDesc.h"

namespace
{
TextureDesc WithSize(int width, int height, const TextureDesc& desc)
{
    TextureDesc out = desc;
    out.width = width;
    out.height = height;
    return out;
}

TextureDesc LinearClampColorBase(int channels, GLenum internalFmt, GLenum format, GLenum type)
{
    TextureDesc desc = TextureDesc::MakeExplicit(0, 0, channels, internalFmt, format, type, false);
    desc.sampler.minFilter = GL_LINEAR;
    desc.sampler.magFilter = GL_LINEAR;
    desc.sampler.wrapS = GL_CLAMP_TO_EDGE;
    desc.sampler.wrapT = GL_CLAMP_TO_EDGE;
    return desc;
}

TextureDesc NearestClampBase(int channels, GLenum internalFmt, GLenum format, GLenum type)
{
    TextureDesc desc = TextureDesc::MakeExplicit(0, 0, channels, internalFmt, format, type, false);
    desc.sampler.minFilter = GL_NEAREST;
    desc.sampler.magFilter = GL_NEAREST;
    desc.sampler.wrapS = GL_CLAMP_TO_EDGE;
    desc.sampler.wrapT = GL_CLAMP_TO_EDGE;
    desc.sampler.wrapR = GL_CLAMP_TO_EDGE;
    return desc;
}
} // namespace

namespace RenderTargetFormats
{
TextureDesc SceneColor()
{
    return LinearClampColorBase(3, GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT);
}

TextureDesc ShadowDepth()
{
    return NearestClampBase(1, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
}

TextureDesc WaterBack()
{
    return NearestClampBase(4, GL_RGBA32F, GL_RGBA, GL_FLOAT);
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

    FramebufferAttachmentDesc color;
    color.attachmentPoint = GL_COLOR_ATTACHMENT0;
    color.texture = WithSize(width, height, colorDesc);
    desc.attachments.push_back(color);

    FramebufferAttachmentDesc depth;
    depth.attachmentPoint = GL_DEPTH_ATTACHMENT;
    depth.texture = WithSize(width, height, ShadowDepth());
    desc.attachments.push_back(depth);

    desc.drawBuffers = {GL_COLOR_ATTACHMENT0};
    return desc;
}

FramebufferDesc ColorOnly(const std::string& name, int width, int height, const TextureDesc& colorDesc)
{
    FramebufferDesc desc;
    desc.name = name;
    desc.width = width;
    desc.height = height;

    FramebufferAttachmentDesc color;
    color.attachmentPoint = GL_COLOR_ATTACHMENT0;
    color.texture = WithSize(width, height, colorDesc);
    desc.attachments.push_back(color);

    desc.drawBuffers = {GL_COLOR_ATTACHMENT0};
    return desc;
}

FramebufferDesc DepthOnly(const std::string& name, int width, int height, const TextureDesc& depthDesc)
{
    FramebufferDesc desc;
    desc.name = name;
    desc.width = width;
    desc.height = height;

    FramebufferAttachmentDesc depth;
    depth.attachmentPoint = GL_DEPTH_ATTACHMENT;
    depth.texture = WithSize(width, height, depthDesc);
    desc.attachments.push_back(depth);

    desc.drawBuffers = {};
    return desc;
}

FramebufferDesc GBuffer(const std::string& name, int width, int height)
{
    FramebufferDesc desc;
    desc.name = name;
    desc.width = width;
    desc.height = height;

    auto addColor = [&](GBufferTarget tag, const TextureDesc& texDesc)
    {
        FramebufferAttachmentDesc attachment;
        attachment.attachmentPoint = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(tag);
        attachment.texture = WithSize(width, height, texDesc);
        attachment.gbufferTag = tag;
        desc.attachments.push_back(attachment);
        desc.drawBuffers.push_back(attachment.attachmentPoint);
    };

    TextureDesc albedo = LinearClampColorBase(4, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    TextureDesc normal = LinearClampColorBase(4, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    TextureDesc mrsc = LinearClampColorBase(4, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    TextureDesc flag = LinearClampColorBase(4, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);

    addColor(GBufferTarget::AlbdeoAO, albedo);
    addColor(GBufferTarget::NormalXY, normal);
    addColor(GBufferTarget::MRSC, mrsc);
    addColor(GBufferTarget::Flag, flag);

    FramebufferAttachmentDesc depth;
    depth.attachmentPoint = GL_DEPTH_ATTACHMENT;
    depth.texture = WithSize(width, height, ShadowDepth());
    depth.gbufferTag = GBufferTarget::Depth;
    desc.attachments.push_back(depth);

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

    FramebufferAttachmentDesc color;
    color.attachmentPoint = GL_COLOR_ATTACHMENT0;
    color.storage = AttachmentStorage::Texture;
    color.texture = WithSize(width, height, colorDesc);
    desc.attachments.push_back(color);

    FramebufferAttachmentDesc depthStencil;
    depthStencil.attachmentPoint = GL_DEPTH_STENCIL_ATTACHMENT;
    depthStencil.storage = AttachmentStorage::RenderBuffer;
    depthStencil.renderBuffer = DepthStencilRBO(width, height);
    desc.attachments.push_back(depthStencil);

    desc.drawBuffers = {GL_COLOR_ATTACHMENT0};
    return desc;
}
} // namespace RenderTargetFormats
