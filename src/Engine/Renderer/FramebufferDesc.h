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
    /// 设置 depth-stencil 纹理附件（可采样，替换已有 depth / depth-stencil）
    void SetDepthStencilAttachment(const TextureDesc& depthStencilTexture);
    /// 设置独立 Stencil RenderBuffer 附件（可与深度纹理并存，替换已有 stencil）
    void SetStencilAttachment(const RenderBufferDesc& renderBuffer);
    int ColorAttachmentCount() const { return static_cast<int>(drawBuffers.size()); }
};

/// GBuffer FBO 颜色附件索引（与 RenderTargetFormats::GBuffer 创建顺序一致）
namespace GBufferLayout
{
constexpr int AlbedoAO = 0;
constexpr int NormalXY = 1;
constexpr int MRSC = 2;
constexpr int Flag = 3;
constexpr int Gbuffer0 = 4; // 光照结果；几何 Deferred 不写，由光照 pass Blit 填入
constexpr int MaterialColorStart = 0;
constexpr int MaterialColorCount = 4;
} // namespace GBufferLayout

namespace RenderTargetFormats
{
TextureDesc SceneColor();
TextureDesc ShadowDepth();
TextureDesc DepthStencilTexture();
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
