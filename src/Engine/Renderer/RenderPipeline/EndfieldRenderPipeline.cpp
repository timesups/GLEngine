#include "EndfieldRenderPipeline.h"
#include "../../Asset/AssetManager.h"
#include "../../Asset/Types/Mesh.h"
#include "../../Asset/Types/Shader.h"
#include "../../Core/util.h"
#include "../../Entity/EntityManager.h"
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
    LoadAsset();

    return true;
}

void EndfieldRenderPipeline::Render(RenderContext& context)
{
    // Draw Gbuffer
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Draw G Buffer");
    m_GBuffer.Bind(false, context.sceneViewportWidth, context.sceneViewportHeight);
    Util::ClearScreen();
    EntityManager::Get().DrawRenderQueue(0, RenderQueue::OpaqueUpperBound);
    m_GBuffer.UnBind();

    glPopDebugGroup();
    // Defrred Light
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Defrred Light");
    m_GBuffer.BindAttachments();
    m_defrredLightShader->Use();
    AssetManager::Get().GetScreenMesh()->Draw();
    m_GBuffer.UnBindAttachments();
    glPopDebugGroup();
}

void EndfieldRenderPipeline::LoadAsset()
{
    m_defrredLightShader = AssetManager::Get().LoadShader("project://Endfield/Shader/EndfieldDefrredLight.glsl");
}

REGISTER_RENDER_PIPELINE("Endfield", EndfieldRenderPipeline);

#undef MODULE
