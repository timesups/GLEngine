#pragma once

#include "FramebufferBinding.h"
#include "FramebufferDesc.h"
#include <memory>
#include <string>
#include <vector>

class Shader;

struct RenderTargetAttachment
{
    FramebufferAttachmentBinding binding;
    std::shared_ptr<Texture> texture;
    std::shared_ptr<RenderBuffer> renderBuffer;
};

class RenderTarget
{
  public:
    RenderTarget();
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    bool Build(const FramebufferDesc& desc);

    void Create(const std::string& name, int width, int height);
    void Create(const std::string& name, int width, int height, const TextureDesc& textureDesc);
    void CreateGBuffer(const std::string& name, int width, int height);
    void CreateColorOnly(const std::string& name, int width, int height);
    void CreateColorOnly(const std::string& name, int width, int height, const TextureDesc& textureDesc);
    void CreateDepth(const std::string& name, int width, int height);
    bool CreateDepth(const std::string& name, int width, int height, const TextureDesc& textureDesc);

    bool AddAttachment(RenderTargetAttachment& attachment, TextureType type);

    void DrawBufferTo(RenderTarget& dec, std::shared_ptr<Shader> shader, const std::string& name);
    void UpsampleBufferTo(RenderTarget& mip, RenderTarget& dest, std::shared_ptr<Shader> shader,
                          const std::string& name);
    int BindGBufferTexture(int offset = 0);
    void UnbindGBufferTexture();

    void Resize(int width, int height);
    void Bind(bool debug, int width = 0, int height = 0);
    void UnBind();
    void BlitTo(RenderTarget& dst, FramebufferBlitMask mask) const;
    bool BindArrayTargetLayer(unsigned int layer, RenderTargetAttachment* attachment);

    Texture& ColorAttachment(int index = 0);
    const Texture& ColorAttachment(int index = 0) const;
    Texture& DepthAttachment();
    const Texture& DepthAttachment() const;
    Texture& GetGBufferTexture(GBufferTarget target);
    const Texture& GetGBufferTexture(GBufferTarget target) const;
    const RenderTargetAttachment* FindDepthAttachment() const;

    Framebuffer& GetFramebuffer();
    const Framebuffer& GetFramebuffer() const;

    int Width() const;
    int Height() const;
    const std::string& Name() const;

  private:
    void Destroy();
    bool CreateAttachments(const FramebufferDesc& desc);
    bool ReattachAll();
    GLenum TextureTargetFromType(TextureType type) const;

    RenderTargetAttachment* FindColorAttachment(int index);
    const RenderTargetAttachment* FindColorAttachment(int index) const;
    RenderTargetAttachment* FindDepthAttachment();
    RenderTargetAttachment* FindGBufferAttachment(GBufferTarget target);
    const RenderTargetAttachment* FindGBufferAttachment(GBufferTarget target) const;

    std::vector<RenderTargetAttachment> m_attachments;
    Framebuffer m_framebuffer;

    int m_width = 0;
    int m_height = 0;
    std::string m_name;
};
