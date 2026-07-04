#pragma once

#include "../Asset/Types/Texture/Texture.h"
#include "RenderBuffer.h"
#include <glad/glad.h>
#include <string>
#include <vector>

enum GBufferTarget : uint8_t
{
    AlbdeoAO = 0, // rgb: Albedo, a: ao
    NormalXY = 1, // rg: normal xy
    MRSC = 2,     // r: Metallic, g: Roughness, b: specular, a: custom
    Flag = 3,
    Depth = 4,
    Count,
};

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
    /// 非 GBuffer 附件保持为 GBufferTarget::Count
    GBufferTarget gbufferTag = GBufferTarget::Count;
};

struct FramebufferDesc
{
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<FramebufferAttachmentDesc> attachments;
    /// MRT 绘制目标；为空时由 Build 根据颜色附件自动推导
    std::vector<GLenum> drawBuffers;
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
