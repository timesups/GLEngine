#include "DeferredRender.h"
#include "RenderPipelineRegistry.h"
#include "../../Asset/AssetManager.h"
#include "../../Asset/Types/Mesh.h"
#include "../../Asset/Types/Shader.h"
#include "../../Asset/Types/Texture/Texture2D.h"
#include "../../Core/Log.h"
#include "../../Core/util.h"
#include "../../Entity/EntityManager.h"
#include "../RenderContext.h"
#include <random>

#define MODULE "Deferred Pipeline"

DeferredRender::~DeferredRender() = default;

DeferredRender::DeferredRender() = default;

bool DeferredRender::OnInit(const int width, const int height)
{
    m_buf_geo.CreateGBuffer("Draw GBuffer", width, height);
    m_shaderDeferredLighting = AssetManager::Get().GetAsset<Shader>("engine://shaders/DeferredLight.glsl");
    if (!m_shaderDeferredLighting || m_shaderDeferredLighting->m_passes.empty())
    {
        LogA(LogLevel::ERROR, "DrawScene: DeferredLight shader missing or has no passes");
        return false;
    }
    InitSSAO(width, height);
    return true;
}

bool DeferredRender::InitSSAO(const int width, const int height)
{

    TextureDesc desc_ssao = TextureDesc::MakeExplicit(width, height, GL_R32F, GL_RED, GL_FLOAT);
    desc_ssao.sampler.minFilter = GL_NEAREST;
    desc_ssao.sampler.magFilter = GL_NEAREST;
    m_buf_ssao.CreateColorOnly("SSAO", width, height, desc_ssao);
    m_bufSSAOBlur.CreateColorOnly("SSAO_blur", width, height, desc_ssao);

    // init ssao kernel
    std::uniform_real_distribution randomFloats(0.0, 1.0);
    std::default_random_engine generator;
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
    // init ssao noise texture
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
    m_ssao_noise->Create(desc_noise, &ssaoNoise[0]);

    return true;
}

void DeferredRender::Render(RenderContext& context)
{
    DrawShadowMap(context);

    DrawCustomDepth(context);

    DrawGbuffer(context);

    DrawSSAO(context);

    DrawLighting(context);

    DrawTransparent(context);

    PostProcessing(context);
}

void DeferredRender::DrawGbuffer(RenderContext& context)
{
    m_buf_geo.Bind(true, context.sceneViewportWidth, context.sceneViewportHeight);
    Util::ClearScreen();
    EntityManager::Get().DrawRenderQueue(0, RenderQueue::OpaqueUpperBound);
    m_buf_geo.UnBind();
}

void DeferredRender::DrawSSAO(RenderContext& context)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "SSAO");

    // Draw SSAO
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Draw SSAO");
    auto shader = AssetManager::Get().GetAsset<Shader>("engine://shaders/SSAO.glsl");
    m_buf_ssao.Bind(false, context.sceneViewportWidth, context.sceneViewportHeight);
    shader->Use();
    shader->SetValue("_NormalXYTex", 0);
    shader->SetValue("_DepthTex", 1);
    shader->SetValue("_texNoise", 2);
    for (int i = 0; i < m_ssaoKernel.size(); i++)
    {
        std::string valueName = "_samples[";
        shader->SetValue(valueName + std::to_string(i) + "]", m_ssaoKernel[i]);
    }

    m_buf_geo.ColorAttachment(1).Bind(0);
    m_buf_geo.DepthAttachment().Bind(1);
    m_ssao_noise->Bind(2);

    AssetManager::Get().GetScreenMesh()->Draw();
    m_buf_geo.ColorAttachment(1).UnBind();
    m_buf_geo.DepthAttachment().UnBind();
    m_ssao_noise->UnBind();

    m_buf_ssao.UnBind();
    glPopDebugGroup();

    // SSAO blur
    shader = AssetManager::Get().GetAsset<Shader>("engine://shaders/SSAO_Blur.glsl");
    m_bufSSAOBlur.Resize(context.sceneViewportWidth, context.sceneViewportHeight);
    m_buf_ssao.DrawBufferTo(m_bufSSAOBlur, shader, "SSAO Blur");

    glPopDebugGroup();
}

void DeferredRender::DrawLighting(RenderContext& context)
{
    m_bufOpaqueLight.Bind(true, context.sceneViewportWidth, context.sceneViewportHeight);
    Util::ClearScreen(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    BindLightShadowMap();
    BindIBLTextures();
    int currentIndex = m_buf_geo.BindGBufferTexture();
    m_bufSSAOBlur.ColorAttachment().Bind(currentIndex);
    m_shaderDeferredLighting->Use();
    AssetManager::Get().GetScreenMesh()->Draw();
    m_buf_geo.UnbindGBufferTexture();
    UnbinLightShadowMap();
    m_bufSSAOBlur.ColorAttachment().UnBind();
    EntityManager::Get().DrawRenderQueue(RenderQueue::Skybox, RenderQueue::Transparent);
    m_bufOpaqueLight.UnBind();
}

REGISTER_RENDER_PIPELINE("Deferred", DeferredRender);

#undef MODULE