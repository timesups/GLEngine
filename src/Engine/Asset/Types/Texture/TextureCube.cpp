#include "TextureCube.h"
#include "../../../Core/Log.h"
#include <algorithm>
#include <cstddef>
#include <glad/glad.h>

#define MODULE "TextureCube"

namespace
{
size_t CalcFaceByteSize(int width, int height, GLenum pixelFormat, GLenum pixelType)
{
    int comp = 4;
    switch (pixelFormat)
    {
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
        comp = 1;
        break;
    case GL_RG:
        comp = 2;
        break;
    case GL_RGB:
    case GL_BGR:
        comp = 3;
        break;
    default:
        comp = 4;
        break;
    }

    size_t elem = 1;
    switch (pixelType)
    {
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
        elem = 1;
        break;
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
    case GL_HALF_FLOAT:
        elem = 2;
        break;
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_FLOAT:
        elem = 4;
        break;
    default:
        elem = 1;
        break;
    }

    return static_cast<size_t>(width) * height * comp * elem;
}
} // namespace

bool TextureCube::Create(const TextureDesc& desc, const void* pixelData)
{
    if (desc.width <= 0 || desc.height <= 0 || desc.channels <= 0)
    {
        LogA(LogLevel::ERROR, "TextureCube::Create: invalid size {}x{} ch={}", desc.width, desc.height, desc.channels);
        return false;
    }

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
            LogA(LogLevel::ERROR,
                 "TextureCube::Create: custom internalFormat requires non-zero pixelFormat and pixelType");
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

    m_glTarget = GL_TEXTURE_CUBE_MAP;
    m_mipLevels = ResolveMipLevels(desc);
    m_generateMips = desc.generateMips;
    m_minFilter = desc.sampler.minFilter;

    const size_t faceBytes = CalcFaceByteSize(desc.width, desc.height, pixelFmt, pixelType);
    const auto* faceBase = static_cast<const unsigned char*>(pixelData);

    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);
    ApplySampler(GL_TEXTURE_CUBE_MAP, desc);
    for (int level = 0; level < m_mipLevels; ++level)
    {
        const int levelWidth = std::max(1, desc.width >> level);
        const int levelHeight = std::max(1, desc.height >> level);
        for (int i = 0; i < 6; ++i)
        {
            const void* faceData = nullptr;
            if (level == 0 && faceBase)
                faceData = faceBase + i * faceBytes;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, level, internalFmt, levelWidth, levelHeight, 0, pixelFmt,
                         pixelType, faceData);
        }
    }
    FinishTextureMipSetup(GL_TEXTURE_CUBE_MAP, m_mipLevels, m_generateMips, m_minFilter);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return true;
}

bool TextureCube::Resize(int width, int height, int channels)
{
    const int ch = channels > 0 ? channels : m_channels;
    if (width <= 0 || height <= 0 || ch <= 0)
    {
        LogA(LogLevel::ERROR, "TextureCube::Resize: invalid size {}x{} ch={} (stored ch={})", width, height, channels,
             m_channels);
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

    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);
    for (int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFmt, width, height, 0, pixelFmt, pixelType,
                     nullptr);
    }
    m_width = width;
    m_height = height;
    m_channels = ch;
    m_mipLevels = CalcMaxMipLevels(width, height);
    FinishTextureMipSetup(GL_TEXTURE_CUBE_MAP, m_mipLevels, m_generateMips, m_minFilter);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    m_internalFormat = internalFmt;
    m_pixelFormat = pixelFmt;
    m_pixelType = pixelType;
    return true;
}

void TextureCube::Bind(unsigned int uint)
{
    if (!m_id)
    {
        LogA(LogLevel::ERROR, "Try to bind a invalid texture");
        return;
    }
    if (m_isBound && m_lastBoundUnit == uint)
        return;

    glActiveTexture(GL_TEXTURE0 + uint);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);

    m_lastBoundUnit = uint;
    m_isBound = true;
}

void TextureCube::UnBind()
{
    if (!m_isBound)
        return;
    glActiveTexture(GL_TEXTURE0 + m_lastBoundUnit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    m_isBound = false;
}

#undef MODULE
