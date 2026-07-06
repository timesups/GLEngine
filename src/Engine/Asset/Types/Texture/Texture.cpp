#include "Texture.h"
#include "../../../Core/Log.h"
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

#define MODULE "Texture"

int CalcMaxMipLevels(int width, int height)
{
    const int dim = std::max(width, height);
    if (dim <= 0)
        return 1;
    int levels = 1;
    for (int size = dim; size > 1; size >>= 1)
        ++levels;
    return levels;
}

int ResolveMipLevels(const TextureDesc& desc)
{
    if (desc.mipLevels > 0)
        return desc.mipLevels;
    return CalcMaxMipLevels(desc.width, desc.height);
}

void FinishTextureMipSetup(GLenum target, int mipLevels, bool generateMips, GLenum minFilter)
{
    glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, std::max(0, mipLevels - 1));
    if (!generateMips || mipLevels <= 1)
        return;

    glGenerateMipmap(target);
    if (minFilter == GL_NEAREST)
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    else if (minFilter == GL_LINEAR)
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
}

void ApplySampler(GLenum target, const TextureDesc& desc)
{
    const TextureSamplerDesc& s = desc.sampler;

    glTexParameteri(target, GL_TEXTURE_WRAP_S, s.wrapS);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, s.wrapT);
    if (s.wrapR == GL_CLAMP_TO_BORDER or s.wrapS == GL_CLAMP_TO_BORDER)
    {
        glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(s.borderColor));
    }

    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, s.minFilter);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, s.magFilter);
    if (s.compareMode != GL_NONE)
    {
        glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, s.compareMode);
        glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, s.compareFunc);
    }
    else
        glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    if (desc.type == TextureType::TEXTURECUBE || desc.type == TextureType::TEXTURECUBEARRAY)
    {
        glTexParameteri(target, GL_TEXTURE_WRAP_R, s.wrapR);
#ifndef GL_TEXTURE_CUBE_MAP_SEAMLESS
#define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F
#endif
        glTexParameteri(target, GL_TEXTURE_CUBE_MAP_SEAMLESS, GL_TRUE);
    }
}

TexFormatType ChannelsToFormat(int channels, bool SRGB)
{
    TexFormatType formatType{};
    switch (channels)
    {
    case 1:
        formatType.Type = GL_RED;
        formatType.Format = GL_R8;
        break;
    case 2:
        formatType.Type = GL_RG;
        formatType.Format = GL_RG8;
        break;
    case 3:
        formatType.Type = GL_RGB;
        formatType.Format = SRGB ? GL_SRGB8 : GL_RGB8;
        break;
    case 4:
    default:
        formatType.Type = GL_RGBA;
        formatType.Format = SRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        if (channels != 4)
            LogA(LogLevel::WARNING, "ChannelsToFormat: unexpected channel count {}, using RGBA", channels);
        break;
    }
    return formatType;
}

int ChannelsFromPixelFormat(GLenum format)
{
    switch (format)
    {
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
    case GL_DEPTH_COMPONENT:
        return 1;
    case GL_RG:
        return 2;
    case GL_RGB:
    case GL_BGR:
        return 3;
    case GL_RGBA:
    case GL_BGRA:
        return 4;
    default:
        return 0;
    }
}

bool InferPixelLayoutFromInternal(GLenum internalFormat, GLenum& pixelFormat, GLenum& pixelType, int& channels)
{
    switch (internalFormat)
    {
    case GL_R8:
        pixelFormat = GL_RED;
        pixelType = GL_UNSIGNED_BYTE;
        channels = 1;
        return true;
    case GL_RG8:
        pixelFormat = GL_RG;
        pixelType = GL_UNSIGNED_BYTE;
        channels = 2;
        return true;
    case GL_RGB8:
        pixelFormat = GL_RGB;
        pixelType = GL_UNSIGNED_BYTE;
        channels = 3;
        return true;
    case GL_RGBA8:
    case GL_RGBA:
        pixelFormat = GL_RGBA;
        pixelType = GL_UNSIGNED_BYTE;
        channels = 4;
        return true;
    case GL_SRGB8:
        pixelFormat = GL_RGB;
        pixelType = GL_UNSIGNED_BYTE;
        channels = 3;
        return true;
    case GL_SRGB8_ALPHA8:
        pixelFormat = GL_RGBA;
        pixelType = GL_UNSIGNED_BYTE;
        channels = 4;
        return true;
    case GL_R16F:
    case GL_R32F:
    case GL_RED:
        pixelFormat = GL_RED;
        pixelType = GL_FLOAT;
        channels = 1;
        return true;
    case GL_RG16F:
        pixelFormat = GL_RG;
        pixelType = GL_FLOAT;
        channels = 2;
        return true;
    case GL_RG32F:
        pixelFormat = GL_RG;
        pixelType = GL_FLOAT;
        channels = 2;
        return true;
    case GL_RGB16F:
        pixelFormat = GL_RGB;
        pixelType = GL_FLOAT;
        channels = 3;
        return true;
    case GL_RGBA16F:
        pixelFormat = GL_RGBA;
        pixelType = GL_FLOAT;
        channels = 4;
        return true;
    case GL_RGBA32F:
        pixelFormat = GL_RGBA;
        pixelType = GL_FLOAT;
        channels = 4;
        return true;
    case GL_R11F_G11F_B10F:
        pixelFormat = GL_RGB;
        pixelType = GL_FLOAT;
        channels = 3;
        return true;
    case GL_DEPTH_COMPONENT16:
        pixelFormat = GL_DEPTH_COMPONENT;
        pixelType = GL_UNSIGNED_SHORT;
        channels = 1;
        return true;
    case GL_DEPTH_COMPONENT24:
        pixelFormat = GL_DEPTH_COMPONENT;
        pixelType = GL_UNSIGNED_INT;
        channels = 1;
        return true;
    case GL_DEPTH_COMPONENT32F:
        pixelFormat = GL_DEPTH_COMPONENT;
        pixelType = GL_FLOAT;
        channels = 1;
        return true;
    default:
        return false;
    }
}

bool ResolveTextureLayout(const TextureDesc& desc, ResolvedTextureLayout& out)
{
    if (desc.internalFormat == 0)
    {
        if (desc.channels <= 0)
            return false;

        const TexFormatType ft = ChannelsToFormat(desc.channels, desc.srgb);
        out.internalFormat = ft.Format;
        out.pixelFormat = ft.Type;
        out.pixelType = GL_UNSIGNED_BYTE;
        out.channels = desc.channels;
        out.autoFormat = true;
        return true;
    }

    out.internalFormat = desc.internalFormat;
    out.autoFormat = false;

    if (desc.pixelFormat != 0 && desc.pixelType != 0)
    {
        out.pixelFormat = desc.pixelFormat;
        out.pixelType = desc.pixelType;
        out.channels = desc.channels > 0 ? desc.channels : ChannelsFromPixelFormat(desc.pixelFormat);
        return out.channels > 0;
    }

    if (!InferPixelLayoutFromInternal(desc.internalFormat, out.pixelFormat, out.pixelType, out.channels))
        return false;
    return true;
}

Texture::Texture() : m_width(0), m_height(0), m_channels(0)
{
    glGenTextures(1, &m_id);
}

unsigned int Texture::GetId() const
{
    return m_id;
}

int Texture::GetWidth() const
{
    return m_width;
}

int Texture::GetHeight() const
{
    return m_height;
}

int Texture::GetChannels() const
{
    return m_channels;
}

int Texture::GetMipLevels() const
{
    return m_mipLevels;
}

bool Texture::GenerateMipmaps()
{
    if (!m_id || m_mipLevels <= 1)
        return true;

    glBindTexture(m_glTarget, m_id);
    FinishTextureMipSetup(m_glTarget, m_mipLevels, true, m_minFilter);
    glBindTexture(m_glTarget, 0);
    return glGetError() == GL_NO_ERROR;
}

TextureDesc TextureDesc::MakeAuto(int width, int height, int channels, bool srgb)
{
    TextureDesc d;
    d.width = width;
    d.height = height;
    d.channels = channels;
    d.srgb = srgb;
    return d;
}

TextureDesc TextureDesc::MakeExplicit(int width, int height, GLenum internalFmt, GLenum format, GLenum type, bool srgb)
{
    TextureDesc d;
    d.width = width;
    d.height = height;
    d.srgb = srgb;
    d.internalFormat = internalFmt;
    d.pixelFormat = format;
    d.pixelType = type;
    if (format != 0)
        d.channels = ChannelsFromPixelFormat(format);
    return d;
}

Texture::~Texture()
{
    if (m_id)
        glDeleteTextures(1, &m_id);
}

#undef MODULE
