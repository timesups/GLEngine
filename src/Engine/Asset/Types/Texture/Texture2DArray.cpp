#include "Texture2DArray.h"
#include "../../../Core/Log.h"
#include <glad/glad.h>

#define MODULE "Texture2DArray"

bool Texture2DArray::Create(const TextureDesc& desc, const void* pixelData)
{
    if (desc.width <= 0 || desc.height <= 0 || desc.channels <= 0 || desc.layerCount <= 0)
    {
        LogA(LogLevel::ERROR, "Texture2DArray::Create: invalid size {}x{} ch={} layers={}", desc.width, desc.height,
             desc.channels, desc.layerCount);
        return false;
    }

    m_LayerCount = desc.layerCount;

    GLenum internalFmt = desc.internalFormat;
    GLenum pixelFmt = desc.pixelFormat;
    GLenum pixelType = desc.pixelType;

    if (desc.internalFormat == 0)
    {
        const TexFormatType ft = ChannelsToFormat(desc.channels, desc.srgb);
        internalFmt = ft.Format;
        pixelFmt = ft.Type;
        pixelType = GL_UNSIGNED_BYTE;
        m_autoFormat = true;
    }
    else
    {
        if (desc.pixelFormat == 0 || desc.pixelType == 0)
        {
            LogA(LogLevel::ERROR, "Texture2DArray::Create: custom internalFormat requires pixelFormat and pixelType");
            return false;
        }
        internalFmt = desc.internalFormat;
        pixelFmt = desc.pixelFormat;
        pixelType = desc.pixelType;
        m_autoFormat = false;
    }

    m_width = desc.width;
    m_height = desc.height;
    m_channels = desc.channels;
    m_srgb = desc.srgb;
    m_internalFormat = internalFmt;
    m_pixelFormat = pixelFmt;
    m_pixelType = pixelType;

    m_glTarget = GL_TEXTURE_2D_ARRAY;
    m_mipLevels = ResolveMipLevels(desc);
    m_generateMips = desc.generateMips;
    m_minFilter = desc.sampler.minFilter;

    TextureDesc samplerDesc = desc;
    samplerDesc.type = TextureType::TEXTURE2DARRAY;

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_id);
    ApplySampler(GL_TEXTURE_2D_ARRAY, samplerDesc);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFmt, desc.width, desc.height, desc.layerCount, 0, pixelFmt,
                 pixelType, pixelData);
    const GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        LogA(LogLevel::ERROR, "Texture2DArray::Create: glTexImage3D failed glError=0x{:X}", static_cast<unsigned>(err));
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        return false;
    }

    FinishTextureMipSetup(GL_TEXTURE_2D_ARRAY, m_mipLevels, m_generateMips, m_minFilter);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    return true;
}

bool Texture2DArray::Resize(int width, int height, int channels)
{
    const int ch = channels > 0 ? channels : m_channels;
    if (width <= 0 || height <= 0 || ch <= 0 || m_LayerCount <= 0)
    {
        LogA(LogLevel::ERROR, "Texture2DArray::Resize: invalid size {}x{} ch={} layers={}", width, height, ch,
             m_LayerCount);
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

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_id);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFmt, width, height, m_LayerCount, 0, pixelFmt, pixelType, nullptr);
    const GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        LogA(LogLevel::ERROR, "Texture2DArray::Resize: glTexImage3D failed glError=0x{:X}", static_cast<unsigned>(err));
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        return false;
    }

    m_width = width;
    m_height = height;
    m_channels = ch;
    m_mipLevels = CalcMaxMipLevels(width, height);
    m_internalFormat = internalFmt;
    m_pixelFormat = pixelFmt;
    m_pixelType = pixelType;
    FinishTextureMipSetup(GL_TEXTURE_2D_ARRAY, m_mipLevels, m_generateMips, m_minFilter);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    return true;
}

void Texture2DArray::Bind(unsigned int uint)
{
    if (!m_id)
    {
        LogA(LogLevel::ERROR, "Try to bind a invalid texture");
        return;
    }
    if (m_isBound && m_lastBoundUnit == uint)
        return;

    glActiveTexture(GL_TEXTURE0 + uint);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_id);

    m_lastBoundUnit = uint;
    m_isBound = true;
}

void Texture2DArray::UnBind()
{
    if (!m_isBound)
        return;
    glActiveTexture(GL_TEXTURE0 + m_lastBoundUnit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    m_isBound = false;
}

#undef MODULE
