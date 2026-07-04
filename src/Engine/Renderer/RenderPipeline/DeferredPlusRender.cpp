#include "DeferredPlusRender.h"
#include "RenderPipelineRegistry.h"

#define MODULE "DeferredPlus"

REGISTER_RENDER_PIPELINE("DeferredPlus", DeferredPlusRender);
#undef MODULE
