#include "TextureCubeArray.h"
#include "../../../Core/Log.h"
#include "Texture.h"

#define MODULE "TextureCubeArray"
bool TextureCubeArray::Create(const TextureDesc& desc, const void* pixelData)
{

    if (desc.width <= 0 || desc.height <= 0 || desc.channels <= 0 || desc.layerCount <= 0)
    {
        LogA(LogLevel::ERROR, "Create: invalid size {}x{} ch={} layers={}", desc.width, desc.height, desc.channels,
             desc.layerCount);
        return false;
    }
    if (desc.width != desc.height)
    {
        LogA(LogLevel::ERROR, "Create: cube map array requires square faces ({}x{})", desc.width, desc.height);
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
            LogA(LogLevel::ERROR, "Create: custom internalFormat requires non-zero pixelFormat and pixelType");
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

    m_glTarget = GL_TEXTURE_CUBE_MAP_ARRAY;
    m_mipLevels = ResolveMipLevels(desc);
    m_generateMips = desc.generateMips;
    m_minFilter = desc.sampler.minFilter;

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_id);
    ApplySampler(GL_TEXTURE_CUBE_MAP_ARRAY, desc);
    // glTexImage3D/TexStorage3D 的 depth = layer-face 总数（6 * cube 数）
    const unsigned int layerFaceCount = m_LayerCount * 6;
    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, internalFmt, m_width, m_height, layerFaceCount, 0, pixelFmt, pixelType,
                 pixelData);
    const GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        LogA(LogLevel::ERROR, "Create: glTexImage3D failed ({}x{} cubes={} layerFaces={}) glError=0x{:X}", m_width,
             m_height, m_LayerCount, layerFaceCount, static_cast<unsigned>(err));
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
        return false;
    }

    FinishTextureMipSetup(GL_TEXTURE_CUBE_MAP_ARRAY, m_mipLevels, m_generateMips, m_minFilter);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    return true;
}
bool TextureCubeArray::Resize(int width, int height, int channels)
{
    const int ch = channels > 0 ? channels : m_channels;
    if (width <= 0 || height <= 0 || ch <= 0 || m_LayerCount <= 0)
    {
        LogA(LogLevel::ERROR, "Resize: invalid size {}x{} ch={} layers={}", width, height, ch, m_LayerCount);
        return false;
    }

    GLenum internalFmt = m_internalFormat;
    if (m_autoFormat)
        internalFmt = ChannelsToFormat(ch, m_srgb).Format;

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_id);
    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, internalFmt, width, height, m_LayerCount * 6, 0, m_pixelFormat,
                 m_pixelType, nullptr);
    const GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        LogA(LogLevel::ERROR, "Resize: glTexImage3D failed glError=0x{:X}", static_cast<unsigned>(err));
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
        return false;
    }

    m_width = width;
    m_height = height;
    m_channels = ch;
    m_mipLevels = CalcMaxMipLevels(width, height);
    m_internalFormat = internalFmt;
    FinishTextureMipSetup(GL_TEXTURE_CUBE_MAP_ARRAY, m_mipLevels, m_generateMips, m_minFilter);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    return true;
}

unsigned int TextureCubeArray::GetLayerCount() const
{
    return m_LayerCount;
}

void TextureCubeArray::Bind(unsigned int uint)
{
    if (!m_id)
    {
        LogA(LogLevel::ERROR, "Try to bind a invalid texture");
        return;
    }
    if (m_isBound && m_lastBoundUnit == uint)
        return;

    glActiveTexture(GL_TEXTURE0 + uint);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_id);

    m_lastBoundUnit = uint;
    m_isBound = true;
}

void TextureCubeArray::UnBind()
{
    if (!m_isBound)
        return;
    glActiveTexture(GL_TEXTURE0 + m_lastBoundUnit);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
    m_isBound = false;
}

#undef MODULE