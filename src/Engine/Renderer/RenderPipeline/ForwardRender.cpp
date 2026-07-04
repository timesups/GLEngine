#include "ForwardRender.h"
#include "RenderPipelineRegistry.h"

#include "../../Asset/Types/RenderQueue.h"
#include "../../Core/util.h"
#include "../../Entity/EntityManager.h"
#include "../RenderContext.h"

#define MODULE "Forward Render"

void ForwardRender::Render(RenderContext& context)
{
    DrawShadowMap(context);
    DrawCustomDepth(context);

    // 开始向前渲染管线
    m_bufOpaqueLight.Bind(true, context.sceneViewportWidth, context.sceneViewportHeight);
    Util::ClearScreen();

    BindLightShadowMap();
    BindIBLTextures();
    EntityManager::Get().DrawRenderQueue(0, RenderQueue::OpaqueUpperBound);
    EntityManager::Get().DrawRenderQueue(RenderQueue::Skybox, RenderQueue::Transparent);
    UnbinLightShadowMap();
    m_bufOpaqueLight.UnBind();

    DrawTransparent(context);

    PostProcessing(context);
}

REGISTER_RENDER_PIPELINE("Forward", ForwardRender);

#undef MODULE
