#include "EndfieldRenderPipeline.h"
#include "../../Asset/AssetManager.h"
#include "../../Asset/Types/Mesh.h"
#include "../../Asset/Types/Shader.h"
#include "../../Core/util.h"
#include "../../Entity/DrawSetting.h"
#include "../../Entity/EntityManager.h"
#include "../../Entity/RenderUnitFilter.h"
#include "../RenderContext.h"
#include "RenderPipelineRegistry.h"
#define MODULE "Endfield Pipeline"

bool EndfieldRenderPipeline::OnInit(const int width, const int height)
{
    FramebufferDesc bufferDesc;
    bufferDesc.width = width;
    bufferDesc.height = height;
    bufferDesc.name = "Gbuffer";

    TextureDesc texDesc = TextureDesc::MakeExplicit(width, height, GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT);
    bufferDesc.AddColorAttachment(texDesc);
    texDesc = TextureDesc::MakeExplicit(width, height, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2);
    bufferDesc.AddColorAttachment(texDesc);
    bufferDesc.AddColorAttachment(texDesc);
    bufferDesc.AddColorAttachment(texDesc);
    texDesc.internalFormat = GL_RGBA8;
    texDesc.pixelFormat = GL_RGBA;
    texDesc.pixelType = GL_UNSIGNED_BYTE;
    bufferDesc.AddColorAttachment(texDesc);
    bufferDesc.SetDepthStencilAttachment(RenderTargetFormats::DepthStencilTexture());
    m_GBuffer.Build(bufferDesc);

    FramebufferDesc defrredLightDesc;
    defrredLightDesc.width = width;
    defrredLightDesc.height = height;
    defrredLightDesc.name = "Defrred Light";
    defrredLightDesc.SetDepthStencilAttachment(RenderTargetFormats::DepthStencilRBO());
    m_defrredLight.Build(defrredLightDesc);
    m_defrredLight.AddColorAttachment(m_GBuffer.GetColorAttachmentTexture(0));

    FramebufferDesc shadowMapDesc;
    shadowMapDesc.width = width;
    shadowMapDesc.height = height;
    shadowMapDesc.name = "Shadow Map";
    shadowMapDesc.AddColorAttachment(texDesc);

    LoadAsset();
    return true;
}

void EndfieldRenderPipeline::Render(RenderContext& context)
{
    // Draw Main Light Shadow Depth
    DrawShadowMap(context);
    // Draw Gbuffer
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Draw G Buffer");
    m_GBuffer.Bind(false, context.sceneViewportWidth, context.sceneViewportHeight);
    Util::ClearScreen();
    DrawSetting gbufferSetting;
    gbufferSetting.shaderTags = {"LightMode:Deferred"};
    EntityManager::Get().DrawRenderQueue(gbufferSetting, RenderUnitFilter::Opaque());
    gbufferSetting.shaderTags[0] = "LightMode:outline";
    EntityManager::Get().DrawRenderQueue(
        gbufferSetting, RenderUnitFilter::And(RenderUnitFilter::Opaque(), RenderUnitFilter::DrawOutline()));
    m_GBuffer.UnBind();

    glPopDebugGroup();

    // Deferred light：渲染到场景视口尺寸的 RT，避免 UI 开启时全屏绘制导致比例错位
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Defrred Light");
    glViewport(context.sceneViewportX, context.sceneViewportY, context.sceneViewportWidth, context.sceneViewportHeight);
    Util::ClearScreen(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    unsigned int nextIndex = m_GBuffer.BindAttachments();
    m_GBuffer.DepthAttachment().Bind(nextIndex);
    m_defrredLightShader->Use();
    AssetManager::Get().GetScreenMesh()->Draw();
    m_GBuffer.UnBindAttachments();
    m_GBuffer.DepthAttachment().UnBind();
    glPopDebugGroup();

    // PostProcessing(context);
}

void EndfieldRenderPipeline::LoadAsset()
{
    m_defrredLightShader = AssetManager::Get().LoadShader("project://Endfield/Shader/EndfieldDefrredLight.glsl");
}

REGISTER_RENDER_PIPELINE("Endfield", EndfieldRenderPipeline);

#undef MODULE
