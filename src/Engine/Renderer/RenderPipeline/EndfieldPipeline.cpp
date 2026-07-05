#include "EndfieldPipeline.h"
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

#define MODULE "EndfieldPipeline"

EndfieldPipeline::~EndfieldPipeline() = default;

EndfieldPipeline::EndfieldPipeline() = default;

bool EndfieldPipeline::OnInit(const int width, const int height)
{
    //init G buffer
    FramebufferDesc desc;
    desc.name = "Draw GBuffer";
    desc.width = width;
    desc.height = height;




    //m_Gbuffer.Build(const FramebufferDesc &desc)
    return true;
}

void EndfieldPipeline::Render(RenderContext& context)
{
    DrawShadowMap(context);
}


REGISTER_RENDER_PIPELINE("EndfieldPipeline", EndfieldPipeline);

#undef MODULE