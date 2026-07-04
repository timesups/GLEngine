#pragma once
#include <memory>

class Entity;

class RenderContext
{
  public:
    RenderContext();
    void Init();

  public:
    bool ShowUI = false;
    float deltaTime = 0.f;
    float engineTime = 0.f;
    int width = 0;
    int height = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    int sceneViewportX = 0;
    int sceneViewportY = 0;
    int sceneViewportWidth = 0;
    int sceneViewportHeight = 0;
    std::shared_ptr<Entity> currentCamera;

    bool enable_blooom;
};
