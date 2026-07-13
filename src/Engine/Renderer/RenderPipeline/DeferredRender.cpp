#include "DeferredRender.h"
#include "../../Asset/AssetManager.h"
#include "../../Asset/Types/Mesh.h"
#include "../../Asset/Types/Shader.h"
#include "../../Asset/Types/Texture/Texture2D.h"
#include "../../Core/Log.h"
#include "../../Core/util.h"
#include "../../Entity/DrawSetting.h"
#include "../../Entity/EntityManager.h"
#include "../../Entity/RenderUnitFilter.h"
#include "../FramebufferDesc.h"
#include "../RenderContext.h"
#include "RenderPipelineRegistry.h"
#include <glad/glad.h>
#include <random>

#define MODULE "Deferred Pipeline"

DeferredRender::~DeferredRender() = default;

DeferredRender::DeferredRender() = default;

bool DeferredRender::OnInit(const int width, const int height)
{
    m_Gbuffer.CreateGBuffer("Draw GBuffer", width, height);
    m_shaderDeferredLighting = AssetManager::Get().GetAsset<Shader>("engine://shaders/DeferredLight.glsl");
    if (!m_shaderDeferredLighting || m_shaderDeferredLighting->m_passes.empty())
    {
        LogA(LogLevel::ERROR, "DrawScene: DeferredLight shader missing or has no passes");
        return false;
    }
    if (!InitSsao(width, height))
        LogA(LogLevel::WARNING, "Deferred pipeline: SSAO init failed, AO pass unavailable");
    return true;
}

bool DeferredRender::InitSsao(const int width, const int height)
{
    m_shaderSsao = AssetManager::Get().GetAsset<Shader>("engine://shaders/SSAO.glsl");
    m_shaderSsaoBlur = AssetManager::Get().GetAsset<Shader>("engine://shaders/SSAO_Blur.glsl");
    if (!m_shaderSsao || m_shaderSsao->m_passes.empty() || !m_shaderSsaoBlur || m_shaderSsaoBlur->m_passes.empty())
    {
        LogA(LogLevel::ERROR, "InitSsao: SSAO or SSAO_Blur shader missing or has no passes");
        return false;
    }

    TextureDesc descAo = TextureDesc::MakeExplicit(width, height, GL_R32F, GL_RED, GL_FLOAT);
    descAo.sampler.minFilter = GL_NEAREST;
    descAo.sampler.magFilter = GL_NEAREST;
    m_buf_ao.CreateColorOnly("AO", width, height, descAo);
    m_buf_aoBlur.CreateColorOnly("AO_blur", width, height, descAo);

    std::uniform_real_distribution randomFloats(0.0, 1.0);
    std::default_random_engine generator;
    m_ssaoKernel.clear();
    m_ssaoKernel.reserve(64);
    for (size_t i = 0; i < 64; i++)
    {
        float scale = (float)i / 64.0;
        scale = Util::lerp(0.1f, 1.0f, scale * scale);

        glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0,
                         randomFloats(generator));
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        sample *= scale;
        m_ssaoKernel.push_back(sample);
    }

    std::vector<glm::vec3> ssaoNoise;
    for (size_t i = 0; i < 16; i++)
    {
        glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0f);
        ssaoNoise.push_back(noise);
    }
    TextureDesc desc_noise = TextureDesc::MakeExplicit(4, 4, GL_RGB16F, GL_RGB, GL_FLOAT);
    desc_noise.sampler.minFilter = GL_NEAREST;
    desc_noise.sampler.magFilter = GL_NEAREST;
    desc_noise.sampler.wrapS = GL_REPEAT;
    desc_noise.sampler.wrapT = GL_REPEAT;
    m_ssao_noise = std::make_unique<Texture2D>();
    if (!m_ssao_noise->Create(desc_noise, &ssaoNoise[0]))
    {
        LogA(LogLevel::ERROR, "InitSsao: failed to create noise texture");
        return false;
    }

    m_ssaoKernelUploaded = false;
    return true;
}

void DeferredRender::UploadSsaoKernel(const Shader& shader) const
{
    for (int i = 0; i < static_cast<int>(m_ssaoKernel.size()); i++)
    {
        const std::string valueName = "_samples[" + std::to_string(i) + "]";
        shader.SetValue(valueName, m_ssaoKernel[i]);
    }
}

bool DeferredRender::HasAoResources() const
{
    return m_shaderSsao && m_shaderSsaoBlur && m_ssao_noise && m_buf_aoBlur.Width() > 0;
}

void DeferredRender::Render(RenderContext& context)
{
    DrawShadowMap(context);
    DrawCustomDepth(context);
    DrawGbuffer(context);
    DrawAmbientOcclusion(context);
    DrawLighting(context);
    DrawTransparent(context);
    PostProcessing(context);
}

void DeferredRender::DrawGbuffer(RenderContext& context)
{
    m_Gbuffer.Bind(true, context.sceneViewportWidth, context.sceneViewportHeight);
    m_Gbuffer.ApplyGeometryDrawBuffers();
    Util::ClearScreen();
    EntityManager::Get().DrawRenderQueue(DrawSetting{}.WithFilter(RenderUnitFilter::Opaque()));
    m_Gbuffer.UnBind();
}

void DeferredRender::DrawAmbientOcclusion(const RenderContext& context)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Ambient Occlusion");

    switch (context.aoMode)
    {
    case AmbientOcclusionMode::SSAO:
        DrawSsao(context);
        break;
    case AmbientOcclusionMode::HBAO:
        DrawHbao(context);
        break;
    }

    glPopDebugGroup();
}

void DeferredRender::DrawSsao(const RenderContext& context)
{
    if (!HasAoResources())
    {
        static bool s_logged = false;
        if (!s_logged)
        {
            LogA(LogLevel::WARNING, "DrawSsao: AO resources missing");
            s_logged = true;
        }
        return;
    }

    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "SSAO");
    m_buf_ao.Bind(false, context.sceneViewportWidth, context.sceneViewportHeight);
    m_shaderSsao->Use();
    if (!m_ssaoKernelUploaded)
    {
        UploadSsaoKernel(*m_shaderSsao);
        m_ssaoKernelUploaded = true;
    }
    m_shaderSsao->SetValue("_NormalXYTex", 0);
    m_shaderSsao->SetValue("_DepthTex", 1);
    m_shaderSsao->SetValue("_FlagTex", 2);
    m_shaderSsao->SetValue("_texNoise", 3);

    m_Gbuffer.ColorAttachment(GBufferLayout::NormalXY).Bind(0);
    m_Gbuffer.DepthAttachment().Bind(1);
    m_Gbuffer.ColorAttachment(GBufferLayout::Flag).Bind(2);
    m_ssao_noise->Bind(3);

    AssetManager::Get().GetScreenMesh()->Draw();

    m_Gbuffer.ColorAttachment(GBufferLayout::NormalXY).UnBind();
    m_Gbuffer.DepthAttachment().UnBind();
    m_Gbuffer.ColorAttachment(GBufferLayout::Flag).UnBind();
    m_ssao_noise->UnBind();
    m_buf_ao.UnBind();
    glPopDebugGroup();

    m_buf_aoBlur.Resize(context.sceneViewportWidth, context.sceneViewportHeight);
    m_buf_ao.DrawBufferTo(m_buf_aoBlur, m_shaderSsaoBlur, "SSAO Blur");
}

void DeferredRender::DrawHbao(const RenderContext& context)
{
    (void)context;
    static bool s_logged = false;
    if (!s_logged)
    {
        LogA(LogLevel::WARNING, "DrawHbao: HBAO not implemented, falling back to SSAO");
        s_logged = true;
    }
    DrawSsao(context);
}

void DeferredRender::DrawLighting(RenderContext& context)
{
    m_bufOpaqueLight.Bind(true, context.sceneViewportWidth, context.sceneViewportHeight);
    Util::ClearScreen(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    BindLightShadowMap();
    BindIBLTextures();
    const int nextUnit = m_Gbuffer.BindGBufferMaterialTextures();
    if (HasAoResources())
        m_buf_aoBlur.ColorAttachment().Bind(nextUnit);
    else if (const auto white = AssetManager::Get().GetAsset<Texture2D>("engine://textures/white.png"))
        white->Bind(nextUnit);

    m_shaderDeferredLighting->Use();
    AssetManager::Get().GetScreenMesh()->Draw();

    m_Gbuffer.UnbindGBufferMaterialTextures();
    UnbinLightShadowMap();
    if (HasAoResources())
        m_buf_aoBlur.ColorAttachment().UnBind();
    else if (const auto white = AssetManager::Get().GetAsset<Texture2D>("engine://textures/white.png"))
        white->UnBind();

    m_Gbuffer.BlitTo(m_bufOpaqueLight, FramebufferBlitMask::Depth);

    EntityManager::Get().DrawSkyBox();
    m_bufOpaqueLight.UnBind();

    m_bufOpaqueLight.BlitColorAttachmentTo(m_Gbuffer, 0, GBufferLayout::Gbuffer0,
                                           FramebufferBlitMask::Color | FramebufferBlitMask::Depth);
}

void DeferredRender::DrawTransparent(RenderContext& context)
{
    m_bufTransparent.Bind(true, context.sceneViewportWidth, context.sceneViewportHeight);
    Util::ClearScreen(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_Gbuffer.BlitColorAttachmentTo(m_bufTransparent, GBufferLayout::Gbuffer0, 0,
                                    FramebufferBlitMask::Color | FramebufferBlitMask::Depth);
    BindIBLTextures();
    m_buf_CustomDepth.ColorAttachment().Bind(10);
    m_Gbuffer.ColorAttachment(GBufferLayout::Gbuffer0).Bind(11);
    EntityManager::Get().DrawRenderQueue(DrawSetting{}.WithFilter(RenderUnitFilter::Transparent()));
    m_buf_CustomDepth.ColorAttachment().UnBind();
    m_Gbuffer.ColorAttachment(GBufferLayout::Gbuffer0).UnBind();
    m_bufTransparent.UnBind();
}

REGISTER_RENDER_PIPELINE("Deferred", DeferredRender);

#undef MODULE
