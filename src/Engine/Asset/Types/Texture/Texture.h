#pragma once
#include "glm/ext/vector_float4.hpp"
#include <glad/glad.h>
#include <string>

struct TexFormatType
{
    GLenum Format;
    GLenum Type;
};
enum class TextureType
{
    TEXTURE2D = 0,
    TEXTURECUBE = 1,
    TEXTURE2DARRAY,
    TEXTURECUBEARRAY,
};

struct TextureSamplerDesc
{
    GLenum wrapS = GL_REPEAT;
    GLenum wrapT = GL_REPEAT;
    GLenum wrapR = GL_REPEAT;
    GLenum minFilter = GL_NEAREST;
    GLenum magFilter = GL_LINEAR;
    GLenum compareMode = GL_NONE;
    GLenum compareFunc = GL_LEQUAL;
    glm::vec4 borderColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
};

struct TextureDesc
{
    int width = 0;
    int height = 0;
    int channels = 4;
    bool srgb = false;
    GLenum internalFormat = 0;
    GLenum pixelFormat = 0;
    GLenum pixelType = GL_UNSIGNED_BYTE;
    TextureSamplerDesc sampler{};
    TextureType type = TextureType::TEXTURE2D;
    unsigned int layerCount = 0;
    /// 0 表示按尺寸自动计算；1 表示仅 base level
    int mipLevels = 0;
    /// 上传 base level 后是否调用 glGenerateMipmap
    bool generateMips = false;

    static TextureDesc MakeAuto(int width, int height, int channels, bool srgb);
    /// 指定 internalFormat 与上传 format/type；channels 由 format 推导
    static TextureDesc MakeExplicit(int width, int height, GLenum internalFmt, GLenum format, GLenum type,
                                    bool srgb = false);
};

int CalcMaxMipLevels(int width, int height);
int ResolveMipLevels(const TextureDesc& desc);
void FinishTextureMipSetup(GLenum target, int mipLevels, bool generateMips, GLenum minFilter);

TexFormatType ChannelsToFormat(int channels, bool SRGB);
int ChannelsFromPixelFormat(GLenum format);
bool InferPixelLayoutFromInternal(GLenum internalFormat, GLenum& pixelFormat, GLenum& pixelType, int& channels);

struct ResolvedTextureLayout
{
    GLenum internalFormat = 0;
    GLenum pixelFormat = 0;
    GLenum pixelType = GL_UNSIGNED_BYTE;
    int channels = 0;
    bool autoFormat = true;
};

bool ResolveTextureLayout(const TextureDesc& desc, ResolvedTextureLayout& out);
void ApplySampler(GLenum target, const TextureDesc& desc);

class Texture
{
  public:
    Texture();
    virtual ~Texture();
    unsigned int GetId() const;
    int GetWidth() const;
    int GetHeight() const;
    int GetChannels() const;
    int GetMipLevels() const;
    bool GenerateMipmaps();

    /// pixelData: Texture2D 为单张图；TextureCube 为 6 个面连续排列 (+X,-X,+Y,-Y,+Z,-Z)，每面 width×height
    virtual bool Create(const TextureDesc& desc, const void* pixelData = nullptr) = 0;
    virtual bool Resize(int width, int height, int channels = 0) = 0;
    virtual void Bind(unsigned int uint = 0) = 0;
    virtual void UnBind() = 0;

  public:
    std::string m_path;
    std::string m_name;
    bool m_srgb = false;
    bool m_generateMips = false;
    bool m_showInUi = true;

  protected:
    unsigned int m_id = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
    bool m_isBound = false;
    unsigned int m_lastBoundUnit = 0;

    bool m_autoFormat = true;
    GLenum m_internalFormat = 0;
    GLenum m_pixelFormat = 0;
    GLenum m_pixelType = GL_UNSIGNED_BYTE;

    GLenum m_glTarget = GL_TEXTURE_2D;
    int m_mipLevels = 1;
    GLenum m_minFilter = GL_NEAREST;
};
