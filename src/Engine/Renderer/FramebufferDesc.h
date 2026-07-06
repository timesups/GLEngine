#pragma once

#include "../Asset/Types/Texture/Texture.h"
#include "RenderBuffer.h"
#include <glad/glad.h>
#include <string>
#include <vector>

enum class AttachmentStorage
{
    Texture,
    RenderBuffer,
};

struct FramebufferAttachmentDesc
{
    GLenum attachmentPoint = GL_COLOR_ATTACHMENT0;
    AttachmentStorage storage = AttachmentStorage::Texture;
    TextureDesc texture;
    RenderBufferDesc renderBuffer;
};

struct FramebufferDesc
{
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<FramebufferAttachmentDesc> attachments;
    /// MRT 绘制目标；为空时由 Build 根据颜色附件自动推导
    std::vector<GLenum> drawBuffers;

    /// 追加颜色附件，返回 GL_COLOR_ATTACHMENT0 + index
    GLenum AddColorAttachment(const TextureDesc& texture);
    /// 设置深度纹理附件（替换已有 depth / depth-stencil）
    void SetDepthAttachment(const TextureDesc& depthTexture);
    /// 设置 depth-stencil RenderBuffer 附件（替换已有 depth / depth-stencil）
    void SetDepthStencilAttachment(const RenderBufferDesc& renderBuffer);
    int ColorAttachmentCount() const { return static_cast<int>(drawBuffers.size()); }
};

namespace RenderTargetFormats
{
TextureDesc SceneColor();
TextureDesc ShadowDepth();
TextureDesc WaterBack();
TextureDesc BloomHalf();

RenderBufferDesc DepthRBO(int width, int height);
RenderBufferDesc DepthStencilRBO(int width, int height);
RenderBufferDesc StencilRBO(int width, int height);

FramebufferDesc SceneColorDepth(const std::string& name, int width, int height, const TextureDesc& colorDesc);
FramebufferDesc SceneColorDepthStencil(const std::string& name, int width, int height, const TextureDesc& colorDesc);
FramebufferDesc ColorOnly(const std::string& name, int width, int height, const TextureDesc& colorDesc);
FramebufferDesc DepthOnly(const std::string& name, int width, int height, const TextureDesc& depthDesc);
FramebufferDesc GBuffer(const std::string& name, int width, int height);
} // namespace RenderTargetFormats
