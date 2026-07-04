#pragma once

#include "RenderContext.h"
#include <memory>

#include "UniformBuffer.h"

#include "../Entity/Components/Camera.h"
#include "../Entity/Components/Light.h"

class RenderPipeline;

class Renderer
{
  public:
    Renderer();
    ~Renderer();
    void Render(RenderContext& context);
    void Init(int initialWidth, int initialHeight);

  private:
    void UploadLightData();
    void UpdateCameraData(RenderContext& context);

  private:
    UniformBuffer cameraBuffer{sizeof(CameraInputData)};
    UniformBuffer lightBuffer{sizeof(LightInputData)};
    UniformBuffer PostProcessBuffer{sizeof(PostProcessSetting)};
    std::unique_ptr<RenderPipeline> renderPipeline;
};
