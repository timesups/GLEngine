#include "RenderContext.h"

#include <glm/glm.hpp>

#include "../Core/Log.h"
#include "../Entity/Components/Transform.h"
#include "../Entity/EntityManager.h"

RenderContext::RenderContext()
{
    Init();
}

void RenderContext::Init()
{
    EntityManager::Get().SetCurrentCameraRef(&currentCamera);
    currentCamera = EntityManager::Get().CreateCameraEntity("MainCamera");
    currentCamera->GetComponent<Transform>()->SetPosition(glm::vec3(0, 0, 5));
    Log("RenderContext", LogLevel::INFO, "RenderContext initialized with default camera");
}
