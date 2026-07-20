#include "CubemapBaker.h"
#include "../Asset/AssetManager.h"
#include "../Asset/Types/Mesh.h"
#include "../Asset/Types/Shader.h"
#include "../Asset/Types/ShaderPass.h"
#include "../Asset/Types/Texture/Texture2D.h"
#include "../Asset/Types/Texture/TextureCube.h"
#include "../Core/Log.h"
#include "FramebufferBinding.h"
#include "FramebufferDesc.h"
#include "RenderBuffer.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>

#define MODULE "CubemapBaker"

namespace
{
constexpr int kMinFaceSize = 64;
constexpr int kMaxFaceSize = 4096;
constexpr int kDefaultIrradianceFaceSize = 32;
constexpr int kMinIrradianceFaceSize = 8;
constexpr int kMaxIrradianceFaceSize = 256;
constexpr int kBrdfLutSize = 512;
int FloorPowerOfTwo(int value)
{
    if (value <= 1)
        return 1;
    int pot = 1;
    while (pot * 2 <= value)
        pot *= 2;
    return pot;
}

TextureDesc MakeCubemapDesc(int faceSize, bool srgb)
{
    TextureDesc cubeDesc = TextureDesc::MakeExplicit(faceSize, faceSize, GL_RGB16F, GL_RGB, GL_FLOAT, srgb);
    cubeDesc.type = TextureType::TEXTURECUBE;
    cubeDesc.sampler.wrapS = GL_CLAMP_TO_EDGE;
    cubeDesc.sampler.wrapT = GL_CLAMP_TO_EDGE;
    cubeDesc.sampler.wrapR = GL_CLAMP_TO_EDGE;
    cubeDesc.sampler.minFilter = GL_LINEAR;
    cubeDesc.sampler.magFilter = GL_LINEAR;
    return cubeDesc;
}

Mesh& GetBakeCubeMesh()
{
    static std::unique_ptr<Mesh> mesh;
    if (mesh)
        return *mesh;
    static const glm::vec3 corners[8] = {
        {-1.0f, 1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f},   {1.0f, -1.0f, 1.0f},  {1.0f, 1.0f, 1.0f},
    };
    static const unsigned int indices[36] = {
        0, 1, 2, 2, 3, 0, 4, 1, 0, 0, 5, 4, 2, 6, 7, 7, 3, 2, 4, 5, 7, 7, 6, 4, 0, 3, 7, 7, 5, 0, 1, 4, 2, 2, 4, 6,
    };
    std::vector<Vertex> vertices;
    vertices.reserve(8);
    for (const glm::vec3& p : corners)
    {
        Vertex v{};
        v.Position = p;
        vertices.push_back(v);
    }
    mesh = std::make_unique<Mesh>();
    mesh->Create(vertices, std::vector<unsigned int>(std::begin(indices), std::end(indices)),
                 Bounds(glm::vec3(1.0f), glm::vec3(-1.0f)));
    return *mesh;
}

Mesh& GetBrdfBakeScreenMesh()
{
    static std::unique_ptr<Mesh> mesh;
    if (mesh)
        return *mesh;
    static const glm::vec3 corners[4] = {
        {1.0f, 1.0f, 0.0f},
        {1.0f, -1.0f, 0.0f},
        {-1.0f, -1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f},
    };
    static const unsigned int indices[6] = {0, 1, 2, 0, 2, 3};
    std::vector<Vertex> vertices;
    vertices.reserve(4);
    for (const glm::vec3& p : corners)
    {
        Vertex v{};
        v.Position = p;
        vertices.push_back(v);
    }
    mesh = std::make_unique<Mesh>();
    mesh->Create(vertices, std::vector<unsigned int>(std::begin(indices), std::end(indices)),
                 Bounds(glm::vec3(1.0f), glm::vec3(-1.0f)));
    return *mesh;
}

struct BrdfBakeContext
{
    Framebuffer framebuffer;
    std::shared_ptr<RenderBuffer> depthBuffer;
};

BrdfBakeContext& GetBrdfBakeContext()
{
    static BrdfBakeContext context;
    return context;
}

bool EnsureBrdfBakeContext(int size)
{
    BrdfBakeContext& context = GetBrdfBakeContext();
    if (context.depthBuffer && context.depthBuffer->GetId() != 0 && context.framebuffer.Width() == size)
        return true;
    context.framebuffer.SetName("BrdfLutBake");
    context.framebuffer.SetSize(size, size);
    if (!context.depthBuffer)
        context.depthBuffer = std::make_shared<RenderBuffer>();
    RenderBufferDesc depthDesc = RenderTargetFormats::DepthRBO();
    depthDesc.width = size;
    depthDesc.height = size;
    if (!context.depthBuffer->Create(depthDesc))
        return false;
    context.framebuffer.ClearAttachments();
    FramebufferAttachmentBinding depthBinding;
    depthBinding.point = GL_DEPTH_ATTACHMENT;
    depthBinding.source = AttachmentSource::RenderBuffer;
    depthBinding.renderBuffer = context.depthBuffer;
    if (!context.framebuffer.Attach(depthBinding))
        return false;
    context.framebuffer.SetDrawBuffers({GL_COLOR_ATTACHMENT0});
    return true;
}

std::shared_ptr<Texture2D>& GetBrdfLutStorage()
{
    static std::shared_ptr<Texture2D> lut;
    return lut;
}

struct CubemapBakeContext
{
    Framebuffer framebuffer;
    std::shared_ptr<RenderBuffer> depthBuffer;
    int faceSize = 0;
};

CubemapBakeContext& GetBakeContext()
{
    static CubemapBakeContext context;
    return context;
}

bool EnsureBakeContext(int faceSize)
{
    CubemapBakeContext& context = GetBakeContext();
    if (context.faceSize == faceSize && context.depthBuffer && context.depthBuffer->GetId() != 0)
        return true;
    context.framebuffer.SetName("CubemapBake");
    context.framebuffer.SetSize(faceSize, faceSize);
    context.faceSize = faceSize;
    if (!context.depthBuffer)
        context.depthBuffer = std::make_shared<RenderBuffer>();
    RenderBufferDesc depthDesc = RenderTargetFormats::DepthRBO();
    depthDesc.width = faceSize;
    depthDesc.height = faceSize;
    if (!context.depthBuffer->Create(depthDesc))
        return false;
    context.framebuffer.ClearAttachments();
    FramebufferAttachmentBinding depthBinding;
    depthBinding.point = GL_DEPTH_ATTACHMENT;
    depthBinding.source = AttachmentSource::RenderBuffer;
    depthBinding.renderBuffer = context.depthBuffer;
    if (!context.framebuffer.Attach(depthBinding))
        return false;
    context.framebuffer.SetDrawBuffers({GL_COLOR_ATTACHMENT0});
    return true;
}

} // namespace
int CubemapBaker::InferFaceSizeFromEquirect(int equirectWidth, int equirectHeight)
{
    if (equirectWidth <= 0 || equirectHeight <= 0)
        return 512;
    const int fromWidth = equirectWidth / 4;
    const int fromHeight = equirectHeight / 2;
    int faceSize = std::min(fromWidth, fromHeight);
    faceSize = FloorPowerOfTwo(faceSize);
    faceSize = std::clamp(faceSize, kMinFaceSize, kMaxFaceSize);
    return faceSize;
}

void CubemapBaker::SetupCubemapMipFiltering(TextureCube& cube)
{
    if (cube.GetId() == 0 || cube.GetMipLevels() <= 1)
        return;
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube.GetId());
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

bool CubemapBaker::BakeIBLFromEquirect(Texture2D& equirect, TextureCube& outPrefiltered, TextureCube& outIrradiance,
                                       const EquirectToCubemapParams& params)
{
    int faceSize =
        params.faceSize > 0 ? params.faceSize
                            : std::max(params.minPrefilterFaceSize > 0 ? params.minPrefilterFaceSize : 0,
                                       InferFaceSizeFromEquirect(equirect.GetWidth(), equirect.GetHeight()));
    faceSize = FloorPowerOfTwo(faceSize);
    faceSize = std::clamp(faceSize, kMinFaceSize, kMaxFaceSize);
    if (params.faceSize <= 0)
    {
        Log(MODULE, LogLevel::INFO, "CubemapBaker: inferred prefiltered mip0 size {} from equirect {}x{}", faceSize,
            equirect.GetWidth(), equirect.GetHeight());
    }
    int irradianceFaceSize = params.irradianceFaceSize > 0 ? params.irradianceFaceSize : kDefaultIrradianceFaceSize;
    irradianceFaceSize = FloorPowerOfTwo(irradianceFaceSize);
    irradianceFaceSize = std::clamp(irradianceFaceSize, kMinIrradianceFaceSize, kMaxIrradianceFaceSize);
    const int maxMipLevels = params.generateMips ? CalcMaxMipLevels(faceSize, faceSize) : 1;
    TextureDesc prefilterDesc = MakeCubemapDesc(faceSize, params.srgb);
    prefilterDesc.mipLevels = maxMipLevels;
    prefilterDesc.generateMips = false;
    if (!outPrefiltered.Create(prefilterDesc, nullptr))
    {
        Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to create prefiltered cubemap {}x{}", faceSize, faceSize);
        return false;
    }
    TextureDesc irradianceDesc = MakeCubemapDesc(irradianceFaceSize, false);
    irradianceDesc.mipLevels = 1;
    if (!outIrradiance.Create(irradianceDesc, nullptr))
    {
        Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to create irradiance cubemap {}x{}", irradianceFaceSize,
            irradianceFaceSize);
        return false;
    }
    auto equirectShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/Internal/EquirectMap.glsl");
    if (!equirectShader)
    {
        Log(MODULE, LogLevel::ERROR, "CubemapBaker: missing EquirectMap shader");
        return false;
    }
    auto irradianceShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/Internal/irradiance.glsl");
    if (!irradianceShader)
    {
        Log(MODULE, LogLevel::ERROR, "CubemapBaker: missing irradiance shader");
        return false;
    }
    if (maxMipLevels > 1)
    {
        TextureDesc envDesc = MakeCubemapDesc(faceSize, params.srgb);
        envDesc.mipLevels = maxMipLevels;
        envDesc.generateMips = false;
        TextureCube envSource;
        if (!envSource.Create(envDesc, nullptr))
        {
            Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to create env source cubemap {}x{}", faceSize, faceSize);
            return false;
        }
        if (!BakeCubemapFaces(&equirect, &envSource, equirectShader, "_EquirectMap", 0))
        {
            Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to bake equirect to env source");
            return false;
        }
        if (!envSource.GenerateMipmaps())
        {
            Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to generate env source mipmaps");
            return false;
        }
        if (!BakeCubemapFaces(&envSource, &outIrradiance, irradianceShader, "_cubeMap", 0, params.irradiancePhiSamples,
                              params.irradianceThetaSamples))
        {
            Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to bake irradiance convolution");
            return false;
        }
        auto prefilterShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/Internal/prefilter.glsl");
        if (!prefilterShader)
        {
            Log(MODULE, LogLevel::ERROR, "CubemapBaker: missing prefilter shader");
            return false;
        }
        for (int mip = 0; mip < maxMipLevels; ++mip)
        {
            const float roughness = static_cast<float>(mip) / static_cast<float>(std::max(1, maxMipLevels - 1));
            if (!BakeCubemapFaces(&envSource, &outPrefiltered, prefilterShader, "_cubeMap", mip, 0, 0, roughness,
                                  faceSize))
            {
                Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to bake prefilter mip {}", mip);
                return false;
            }
        }
        SetupCubemapMipFiltering(outPrefiltered);
        Log(MODULE, LogLevel::INFO, "CubemapBaker: baked prefilter mips {} (roughness 0..1)", maxMipLevels);
    }
    else
    {
        if (!BakeCubemapFaces(&equirect, &outPrefiltered, equirectShader, "_EquirectMap", 0))
        {
            Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to bake equirect to prefiltered mip0");
            return false;
        }
        if (!BakeCubemapFaces(&outPrefiltered, &outIrradiance, irradianceShader, "_cubeMap", 0, params.irradiancePhiSamples,
                              params.irradianceThetaSamples))
        {
            Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to bake irradiance convolution");
            return false;
        }
    }
    glFinish();
    Log(MODULE, LogLevel::INFO, "CubemapBaker: baked irradiance {}x{} (phi={}, theta={}) from prefiltered mip0 {}x{}",
        irradianceFaceSize, irradianceFaceSize, params.irradiancePhiSamples, params.irradianceThetaSamples, faceSize,
        faceSize);
    return true;
}

bool CubemapBaker::BakeCubemapFaces(Texture* texSource, TextureCube* texDes, const std::shared_ptr<Shader>& shader,
                                    const char* sourceSamplerUniform, int destMipLevel, int irradiancePhiSamples,
                                    int irradianceThetaSamples, float roughness, int cubeMapResolution)
{
    if (!texSource || !texDes || !shader || !sourceSamplerUniform || sourceSamplerUniform[0] == '\0')
    {
        Log(MODULE, LogLevel::ERROR, "BakeCubemapFaces: invalid arguments");
        return false;
    }
    if (texDes->GetId() == 0)
    {
        Log(MODULE, LogLevel::ERROR, "BakeCubemapFaces: destination cubemap is not created");
        return false;
    }
    if (destMipLevel < 0 || destMipLevel >= texDes->GetMipLevels())
    {
        Log(MODULE, LogLevel::ERROR, "BakeCubemapFaces: invalid destination mip level {}", destMipLevel);
        return false;
    }
    if (shader->m_passes.empty() || !shader->m_passes[0] || !shader->m_passes[0]->IsReady())
    {
        Log(MODULE, LogLevel::ERROR, "BakeCubemapFaces: shader is not ready");
        return false;
    }
    const int faceSize = std::max(1, texDes->GetWidth() >> destMipLevel);
    if (!EnsureBakeContext(faceSize))
    {
        Log(MODULE, LogLevel::ERROR, "BakeCubemapFaces: failed to initialize bake framebuffer {}x{}", faceSize,
            faceSize);
        return false;
    }
    Mesh& bakeCube = GetBakeCubeMesh();
    const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.01f, 10.0f);
    const glm::mat4 views[6] = {
        glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
    };
    CubemapBakeContext& context = GetBakeContext();
    constexpr GLenum kColorPoint = GL_COLOR_ATTACHMENT0;
    context.framebuffer.Bind(false);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    shader->Use();
    texSource->Bind(0);
    shader->SetValue(sourceSamplerUniform, 0);
    if (irradiancePhiSamples > 0 || irradianceThetaSamples > 0)
    {
        shader->SetValue("_PhiSamples", std::max(1, irradiancePhiSamples));
        shader->SetValue("_ThetaSamples", std::max(1, irradianceThetaSamples));
    }
    if (roughness >= 0.0f)
    {
        shader->SetValue("_Roughness", roughness);
        shader->SetValue("_CubeMapResolution", static_cast<float>(std::max(1, cubeMapResolution)));
    }
    shader->SetValue("_GL_MATRIX_P", proj);
    bool ok = true;
    for (int i = 0; i < 6; ++i)
    {
        if (!context.framebuffer.AttachCubemapFace(kColorPoint, texDes->GetId(), i, destMipLevel))
        {
            Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to bind cubemap face {} mip {}", i, destMipLevel);
            ok = false;
            break;
        }
        shader->SetValue("_GL_MATRIX_V", views[i]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        bakeCube.Draw();
    }
    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    texSource->UnBind();
    context.framebuffer.Unbind();
    ShaderPass::InvalidateStateCache();
    return ok;
}

bool CubemapBaker::BakeBrdfLUT(Texture2D& outLut)
{
    TextureDesc desc = TextureDesc::MakeExplicit(kBrdfLutSize, kBrdfLutSize, GL_RG16F, GL_RG, GL_FLOAT);
    desc.sampler.wrapS = GL_CLAMP_TO_EDGE;
    desc.sampler.wrapT = GL_CLAMP_TO_EDGE;
    desc.sampler.minFilter = GL_LINEAR;
    desc.sampler.magFilter = GL_LINEAR;
    desc.mipLevels = 1;
    desc.generateMips = false;
    if (!outLut.Create(desc, nullptr))
    {
        Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to create BRDF LUT {}x{}", kBrdfLutSize, kBrdfLutSize);
        return false;
    }
    auto brdfShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/Internal/brdf.glsl");
    if (!brdfShader || brdfShader->m_passes.empty() || !brdfShader->m_passes[0] || !brdfShader->m_passes[0]->IsReady())
    {
        Log(MODULE, LogLevel::ERROR, "CubemapBaker: BRDF shader is not ready");
        return false;
    }
    if (!EnsureBrdfBakeContext(kBrdfLutSize))
    {
        Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to initialize BRDF bake framebuffer");
        return false;
    }
    BrdfBakeContext& context = GetBrdfBakeContext();
    context.framebuffer.Bind(false);
    FramebufferAttachmentBinding colorBinding;
    colorBinding.point = GL_COLOR_ATTACHMENT0;
    colorBinding.source = AttachmentSource::Texture;
    colorBinding.texture = std::shared_ptr<Texture>(&outLut, [](Texture*) {});
    colorBinding.textureTarget = GL_TEXTURE_2D;
    if (!context.framebuffer.Attach(colorBinding))
    {
        Log(MODULE, LogLevel::ERROR, "CubemapBaker: failed to bind BRDF LUT to framebuffer");
        context.framebuffer.Unbind();
        return false;
    }
    // Attach() 内部会 glBindFramebuffer(0)，必须重新绑定 FBO 才能把像素写入 LUT
    context.framebuffer.Bind(false);
    if (!context.framebuffer.CheckComplete())
    {
        Log(MODULE, LogLevel::ERROR, "CubemapBaker: BRDF LUT framebuffer incomplete");
        context.framebuffer.Unbind();
        return false;
    }
    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glViewport(0, 0, kBrdfLutSize, kBrdfLutSize);
    brdfShader->Use();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GetBrdfBakeScreenMesh().Draw();
    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    if (blendWasEnabled)
        glEnable(GL_BLEND);
    context.framebuffer.Unbind();
    ShaderPass::InvalidateStateCache();
    outLut.m_path = "engine://textures/brdf_lut";
    outLut.m_name = "brdf_lut";
    Log(MODULE, LogLevel::INFO, "CubemapBaker: baked BRDF LUT {}x{}", kBrdfLutSize, kBrdfLutSize);
    return true;
}

std::shared_ptr<Texture2D> CubemapBaker::EnsureBrdfLut()
{
    std::shared_ptr<Texture2D>& lut = GetBrdfLutStorage();
    if (lut && lut->GetId() != 0)
        return lut;
    lut = std::make_shared<Texture2D>();
    if (!BakeBrdfLUT(*lut))
    {
        lut.reset();
        return nullptr;
    }
    return lut;
}

void CubemapBaker::InvalidateBrdfLut()
{
    GetBrdfLutStorage().reset();
}

#undef MODULE
