#include "RenderTarget.h"

#include "../Asset/AssetManager.h"
#include "../Asset/Types/Mesh.h"
#include "../Asset/Types/Shader.h"
#include "../Asset/Types/Texture/Texture2D.h"
#include "../Asset/Types/Texture/Texture2DArray.h"
#include "../Asset/Types/Texture/TextureCube.h"
#include "../Asset/Types/Texture/TextureCubeArray.h"
#include "../Core/Log.h"
#include "RenderBuffer.h"

#define MODULE "RenderTarget"

RenderTarget::RenderTarget() = default;

RenderTarget::~RenderTarget()
{
    Destroy();
}

void RenderTarget::Destroy()
{
    m_framebuffer.ClearAttachments();
    m_attachments.clear();
}

GLenum RenderTarget::TextureTargetFromType(TextureType type) const
{
    switch (type)
    {
    case TextureType::TEXTURE2D:
        return GL_TEXTURE_2D;
    case TextureType::TEXTURECUBE:
        return GL_TEXTURE_CUBE_MAP;
    case TextureType::TEXTURECUBEARRAY:
        return GL_TEXTURE_CUBE_MAP_ARRAY;
    case TextureType::TEXTURE2DARRAY:
        return GL_TEXTURE_2D_ARRAY;
    }
    return GL_TEXTURE_2D;
}

bool RenderTarget::CreateAttachments(const FramebufferDesc& desc)
{
    m_attachments.reserve(desc.attachments.size());

    for (const FramebufferAttachmentDesc& attachmentDesc : desc.attachments)
    {
        RenderTargetAttachment attachment;
        attachment.binding.point = attachmentDesc.attachmentPoint;

        if (attachmentDesc.storage == AttachmentStorage::RenderBuffer)
        {
            attachment.renderBuffer = std::make_shared<RenderBuffer>();
            if (!attachment.renderBuffer->Create(attachmentDesc.renderBuffer))
            {
                Log(MODULE, LogLevel::ERROR, "RenderTarget '{}': failed to create renderbuffer 0x{:X}", m_name,
                    static_cast<unsigned>(attachment.binding.point));
                Destroy();
                return false;
            }

            attachment.binding.source = AttachmentSource::RenderBuffer;
            attachment.binding.renderBuffer = attachment.renderBuffer;
        }
        else
        {
            switch (attachmentDesc.texture.type)
            {
            case TextureType::TEXTURE2D:
                attachment.texture = std::make_shared<Texture2D>();
                break;
            case TextureType::TEXTURECUBE:
                attachment.texture = std::make_shared<TextureCube>();
                break;
            case TextureType::TEXTURECUBEARRAY:
                attachment.texture = std::make_shared<TextureCubeArray>();
                break;
            case TextureType::TEXTURE2DARRAY:
                attachment.texture = std::make_shared<Texture2DArray>();
                break;
            }

            if (!attachment.texture->Create(attachmentDesc.texture, nullptr))
            {
                Log(MODULE, LogLevel::ERROR, "RenderTarget '{}': failed to create attachment 0x{:X}", m_name,
                    static_cast<unsigned>(attachment.binding.point));
                Destroy();
                return false;
            }

            attachment.binding.source = AttachmentSource::Texture;
            attachment.binding.texture = attachment.texture;
            attachment.binding.textureTarget = TextureTargetFromType(attachmentDesc.texture.type);
            attachment.binding.cubeFace = -1;
            attachment.binding.layer = 0;
            attachment.binding.mipLevel = 0;
        }

        if (!m_framebuffer.Attach(attachment.binding))
        {
            Destroy();
            return false;
        }

        m_attachments.push_back(std::move(attachment));
    }

    if (!desc.drawBuffers.empty())
        m_framebuffer.SetDrawBuffers(desc.drawBuffers);
    else
        m_framebuffer.SetDrawBuffers({});

    m_framebuffer.Bind(false);
    m_framebuffer.ApplyDrawBuffers();
    const bool ok = m_framebuffer.CheckComplete();
    m_framebuffer.Unbind();

    if (ok)
    {
        Log(MODULE, LogLevel::INFO, "RenderTarget '{}' built {}x{} ({} attachments)", m_name, m_width, m_height,
            m_attachments.size());
        return true;
    }

    Destroy();
    return false;
}

bool RenderTarget::ReattachAll()
{
    m_framebuffer.ClearAttachments();
    for (const RenderTargetAttachment& attachment : m_attachments)
    {
        if (!m_framebuffer.Attach(attachment.binding))
            return false;
    }
    m_framebuffer.Bind(false);
    m_framebuffer.ApplyDrawBuffers();
    const bool ok = m_framebuffer.CheckComplete();
    m_framebuffer.Unbind();
    return ok;
}

bool RenderTarget::Build(const FramebufferDesc& desc)
{
    Destroy();
    m_name = desc.name;
    m_width = desc.width;
    m_height = desc.height;

    if (m_width <= 0 || m_height <= 0)
    {
        Log(MODULE, LogLevel::ERROR, "RenderTarget '{}': invalid size {}x{}", m_name, m_width, m_height);
        return false;
    }

    m_framebuffer.SetName(m_name);
    m_framebuffer.SetSize(m_width, m_height);

    if (desc.attachments.empty())
    {
        Log(MODULE, LogLevel::INFO, "RenderTarget '{}': no attachments", m_name);
        return true;
    }

    return CreateAttachments(desc);
}

void RenderTarget::Create(const std::string& name, int width, int height)
{
    Build(RenderTargetFormats::SceneColorDepth(name, width, height, RenderTargetFormats::SceneColor()));
}

void RenderTarget::Create(const std::string& name, int width, int height, const TextureDesc& textureDesc)
{
    Build(RenderTargetFormats::SceneColorDepth(name, width, height, textureDesc));
}

void RenderTarget::CreateGBuffer(const std::string& name, int width, int height)
{
    Build(RenderTargetFormats::GBuffer(name, width, height));
}

void RenderTarget::CreateColorOnly(const std::string& name, int width, int height)
{
    Build(RenderTargetFormats::ColorOnly(name, width, height, RenderTargetFormats::SceneColor()));
}

void RenderTarget::CreateColorOnly(const std::string& name, int width, int height, const TextureDesc& textureDesc)
{
    Build(RenderTargetFormats::ColorOnly(name, width, height, textureDesc));
}

void RenderTarget::CreateDepth(const std::string& name, int width, int height)
{
    Build(RenderTargetFormats::DepthOnly(name, width, height, RenderTargetFormats::ShadowDepth()));
}

bool RenderTarget::CreateDepth(const std::string& name, int width, int height, const TextureDesc& textureDesc)
{
    return Build(RenderTargetFormats::DepthOnly(name, width, height, textureDesc));
}

bool RenderTarget::AddAttachment(RenderTargetAttachment& attachment, TextureType type)
{
    attachment.binding.textureTarget = TextureTargetFromType(type);
    attachment.binding.source = AttachmentSource::Texture;
    attachment.binding.texture = attachment.texture;

    if (!m_framebuffer.Attach(attachment.binding))
        return false;

    m_attachments.push_back(std::move(attachment));
    m_framebuffer.Bind(false);
    m_framebuffer.ApplyDrawBuffers();
    const bool ok = m_framebuffer.CheckComplete();
    m_framebuffer.Unbind();

    if (ok)
    {
        Log(MODULE, LogLevel::INFO, "RenderTarget '{}' built {}x{} ({} attachments)", m_name, m_width, m_height,
            m_attachments.size());
        return true;
    }
    return false;
}

RenderTargetAttachment* RenderTarget::FindColorAttachment(int index)
{
    int colorIndex = 0;
    for (RenderTargetAttachment& attachment : m_attachments)
    {
        if (attachment.binding.point < GL_COLOR_ATTACHMENT0 || attachment.binding.point > GL_COLOR_ATTACHMENT15)
            continue;
        if (colorIndex == index)
            return &attachment;
        ++colorIndex;
    }
    return nullptr;
}

const RenderTargetAttachment* RenderTarget::FindColorAttachment(int index) const
{
    return const_cast<RenderTarget*>(this)->FindColorAttachment(index);
}

RenderTargetAttachment* RenderTarget::FindDepthAttachment()
{
    for (RenderTargetAttachment& attachment : m_attachments)
    {
        if (attachment.binding.point == GL_DEPTH_ATTACHMENT ||
            attachment.binding.point == GL_DEPTH_STENCIL_ATTACHMENT)
            return &attachment;
    }
    return nullptr;
}

const RenderTargetAttachment* RenderTarget::FindDepthAttachment() const
{
    return const_cast<RenderTarget*>(this)->FindDepthAttachment();
}

int RenderTarget::ColorAttachmentCount() const
{
    int count = 0;
    for (const RenderTargetAttachment& attachment : m_attachments)
    {
        if (attachment.binding.point >= GL_COLOR_ATTACHMENT0 && attachment.binding.point <= GL_COLOR_ATTACHMENT15)
            ++count;
    }
    return count;
}

Texture& RenderTarget::ColorAttachment(int index)
{
    if (RenderTargetAttachment* attachment = FindColorAttachment(index))
        return *attachment->texture;
    static Texture2D fallback;
    Log(MODULE, LogLevel::ERROR, "RenderTarget '{}': color attachment {} not found", m_name, index);
    return fallback;
}

const Texture& RenderTarget::ColorAttachment(int index) const
{
    return const_cast<RenderTarget*>(this)->ColorAttachment(index);
}

Texture& RenderTarget::DepthAttachment()
{
    if (RenderTargetAttachment* attachment = FindDepthAttachment())
    {
        if (attachment->texture)
            return *attachment->texture;
    }
    static Texture2D fallback;
    Log(MODULE, LogLevel::ERROR, "RenderTarget '{}': depth attachment not found", m_name);
    return fallback;
}

const Texture& RenderTarget::DepthAttachment() const
{
    return const_cast<RenderTarget*>(this)->DepthAttachment();
}

void RenderTarget::DrawBufferTo(RenderTarget& dec, std::shared_ptr<Shader> shader, const std::string& name)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name.c_str());
    dec.Bind(false);
    shader->Use();
    ColorAttachment().Bind(0);
    shader->SetValue("_InvSrcSize", glm::vec4(1.0 / m_width, 1.0 / m_height, m_width, m_height));
    shader->SetValue("_SceneColor", 0);
    AssetManager::Get().GetScreenMesh()->Draw();
    ColorAttachment().UnBind();
    dec.UnBind();
    glPopDebugGroup();
}

void RenderTarget::UpsampleBufferTo(RenderTarget& mip, RenderTarget& dest, std::shared_ptr<Shader> shader,
                                   const std::string& name)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name.c_str());
    dest.Bind(false);
    shader->Use();
    ColorAttachment().Bind(0);
    shader->SetValue("_LowRes", 0);
    mip.ColorAttachment().Bind(1);
    shader->SetValue("_MipColor", 1);
    AssetManager::Get().GetScreenMesh()->Draw();
    ColorAttachment().UnBind();
    mip.ColorAttachment().UnBind();
    dest.UnBind();
    glPopDebugGroup();
}

int RenderTarget::BindGBufferTexture(int offset)
{
    const int colorCount = ColorAttachmentCount();
    for (int i = 0; i < colorCount; ++i)
        ColorAttachment(i).Bind(i + offset);

    int bound = colorCount;
    if (FindDepthAttachment())
    {
        DepthAttachment().Bind(offset + colorCount);
        ++bound;
    }
    return bound;
}

void RenderTarget::UnbindGBufferTexture()
{
    const int colorCount = ColorAttachmentCount();
    for (int i = 0; i < colorCount; ++i)
        ColorAttachment(i).UnBind();

    if (FindDepthAttachment())
        DepthAttachment().UnBind();
}

void RenderTarget::Resize(int width, int height)
{
    if (width == m_width && height == m_height)
        return;

    m_width = width;
    m_height = height;
    m_framebuffer.SetSize(m_width, m_height);

    for (RenderTargetAttachment& attachment : m_attachments)
    {
        if (attachment.texture)
            attachment.texture->Resize(m_width, m_height);
        else if (attachment.renderBuffer)
            attachment.renderBuffer->Resize(m_width, m_height);
    }

    if (!ReattachAll())
        Log(MODULE, LogLevel::ERROR, "RenderTarget '{}': failed to reattach after resize to {}x{}", m_name, m_width,
            m_height);
}

void RenderTarget::Bind(bool debug, int width, int height)
{
    if (width > 0 && height > 0)
        Resize(width, height);

    m_framebuffer.Bind(debug);
}

void RenderTarget::UnBind()
{
    m_framebuffer.Unbind();
}

void RenderTarget::BlitTo(RenderTarget& dst, FramebufferBlitMask mask) const
{
    m_framebuffer.BlitTo(dst.m_framebuffer, mask);
}

bool RenderTarget::BindArrayTargetLayer(unsigned int layer, RenderTargetAttachment* attachment)
{
    if (!attachment || !attachment->texture)
        return false;

    m_framebuffer.Bind(false);
    const bool ok =
        m_framebuffer.BindTextureLayer(attachment->binding.point, attachment->texture,
                                       attachment->binding.textureTarget, static_cast<int>(layer));
    m_framebuffer.Unbind();
    return ok;
}

Framebuffer& RenderTarget::GetFramebuffer()
{
    return m_framebuffer;
}

const Framebuffer& RenderTarget::GetFramebuffer() const
{
    return m_framebuffer;
}

int RenderTarget::Width() const
{
    return m_width;
}

int RenderTarget::Height() const
{
    return m_height;
}

const std::string& RenderTarget::Name() const
{
    return m_name;
}

#undef MODULE
