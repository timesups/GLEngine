#include "Texture2D.h"

#include "../../../Core/Log.h"
#include <glad/glad.h>

#define MODULE "Texture2D"

bool Texture2D::Create(const TextureDesc& desc, const void* pixelData)
{
    if (desc.width <= 0 || desc.height <= 0)
    {
        LogA(LogLevel::ERROR, "Create: invalid size {}x{}", desc.width, desc.height);
        return false;
    }

    ResolvedTextureLayout layout;
    if (!ResolveTextureLayout(desc, layout))
    {
        LogA(LogLevel::ERROR, "Create: failed to resolve texture layout (internal=0x{:X})", desc.internalFormat);
        return false;
    }

    m_srgb = desc.srgb;
    m_width = desc.width;
    m_height = desc.height;
    m_channels = layout.channels;
    m_autoFormat = layout.autoFormat;

    const GLenum internalFmt = layout.internalFormat;
    const GLenum pixelFmt = layout.pixelFormat;
    const GLenum pixelType = layout.pixelType;

    m_internalFormat = internalFmt;
    m_pixelFormat = pixelFmt;
    m_pixelType = pixelType;

    m_glTarget = GL_TEXTURE_2D;
    m_mipLevels = ResolveMipLevels(desc);
    m_generateMips = desc.generateMips;
    m_minFilter = desc.sampler.minFilter;

    glBindTexture(GL_TEXTURE_2D, m_id);
    ApplySampler(GL_TEXTURE_2D, desc);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, desc.width, desc.height, 0, pixelFmt, pixelType, pixelData);
    FinishTextureMipSetup(GL_TEXTURE_2D, m_mipLevels, m_generateMips, m_minFilter);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}
void Texture2D::Bind(unsigned int uint)
{
    if (!m_id)
    {
        LogA(LogLevel::ERROR, "Try to bind a invalid texture");
        return;
    }
    if (m_isBound && m_lastBoundUnit == uint)
        return;

    glActiveTexture(GL_TEXTURE0 + uint);
    glBindTexture(GL_TEXTURE_2D, m_id);

    m_lastBoundUnit = uint;
    m_isBound = true;
    glActiveTexture(GL_TEXTURE0);
}

void Texture2D::UnBind()
{
    if (!m_isBound)
        return;
    glActiveTexture(GL_TEXTURE0 + m_lastBoundUnit);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_isBound = false;
}

bool Texture2D::Resize(int width, int height, int channels)
{
    const int ch = channels > 0 ? channels : m_channels;
    if (width <= 0 || height <= 0 || ch <= 0)
    {
        LogA(LogLevel::ERROR, "Resize: invalid size {}x{} ch={} (stored ch={})", width, height, channels, m_channels);
        return false;
    }

    GLenum internalFmt = m_internalFormat;
    GLenum pixelFmt = m_pixelFormat;
    GLenum pixelType = m_pixelType;

    if (m_autoFormat)
    {
        const TexFormatType ft = ChannelsToFormat(ch, m_srgb);
        internalFmt = ft.Format;
        pixelFmt = ft.Type;
        pixelType = GL_UNSIGNED_BYTE;
    }

    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, width, height, 0, pixelFmt, pixelType, nullptr);
    m_width = width;
    m_height = height;
    m_channels = ch;
    m_mipLevels = CalcMaxMipLevels(width, height);
    FinishTextureMipSetup(GL_TEXTURE_2D, m_mipLevels, m_generateMips, m_minFilter);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_internalFormat = internalFmt;
    m_pixelFormat = pixelFmt;
    m_pixelType = pixelType;
    return true;
}

#undef MODULE