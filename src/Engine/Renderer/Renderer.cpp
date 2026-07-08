#include "Renderer.h"

#include "../Asset/Types/ShaderPass.h"
#include "../Core/Config.h"
#include "../Core/Log.h"
#include "../Core/util.h"
#include "../Entity/Components/Camera.h"
#include "AmbientOcclusion.h"
#include "../Entity/EntityManager.h"
#include "glm/ext/vector_float3.hpp"

#include "../Core/LightingConvention.h"

#include "../Entity/Components/light/DirectionalLight.h"
#include "../Entity/Components/light/Light.h"
#include "../Entity/Components/light/LocalLight.h"
#include "../Entity/Components/light/SkyBox.h"

#include "../Asset/Types/Texture/IBLImage.h"
#include "../Asset/Types/Texture/TextureCube.h"

#include "RenderPipeline/RenderPipelineRegistry.h"
#include "RenderPipeline/RenderPipeline.h"

#include "../Asset/LoaderManager.h"
#include "InstanceBuffer.h"

#define MODULE "Renderer"

static_assert(sizeof(PostProcessSetting) == 64, "PostProcessSetting must match post_processing_buffer std140 layout (64 bytes)");

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

void Renderer::Init(const int initialWidth, const int initialHeight)
{
    // 绑定Uniform缓冲对象
    cameraBuffer.BindBase(0);
    LogA(LogLevel::INFO, "Uniform buffer bound to binding 0 (camera_buffer)");

    lightBuffer.BindBase(1);
    LogA(LogLevel::INFO, "Uniform buffer bound to binding 1 (Light_buffer)");

    PostProcessBuffer.BindBase(2);
    LogA(LogLevel::INFO, "Uniform buffer bound to binding 2 (post processing buffer)");

    // 启用各种测试
    glEnable(GL_DEPTH_TEST);

    // 初始化渲染管线
    const std::string pipelineName = RenderPipelineRegistry::ResolveName(Config::Get().renderPipeline);
    Config::Get().renderPipeline = pipelineName;
    renderPipeline = RenderPipelineRegistry::Create(pipelineName);
    if (!renderPipeline)
    {
        LogA(LogLevel::ERROR, "Failed to create render pipeline '{}'", pipelineName);
        return;
    }
    if (!renderPipeline->Init(initialWidth, initialHeight))
    {
        LogA(LogLevel::ERROR, "Failed to initialize render pipeline '{}'", pipelineName);
        renderPipeline.reset();
        return;
    }

    LogA(LogLevel::INFO, "Render pipeline: {}", renderPipeline->GetName());
    LogA(LogLevel::INFO, "Renderer initialized (scene {}x{}, shadow 2048x2048)", initialWidth, initialHeight);
}
// 主渲染循环
void Renderer::Render(RenderContext& context)
{
    if (!renderPipeline)
        return;

    // 更新着色器
    LoaderManager::Get().UpdateAssetFromDisk();
    // 每帧开始同步一次：防止外部直接改 GL 状态导致缓存失真
    ShaderPass::InvalidateStateCache();
    InstanceBuffer::Get().BeginFrame();
    // 更新实体
    EntityManager::Get().Update(context.deltaTime);
    // 收集场景的渲染队列
    EntityManager::Get().GatherSceneRenderUnit(context);
    // 相机更新
    UpdateCameraData(context);
    // 传递灯光信息
    UploadLightData();

    // 首先刷新整个屏幕,包括UI
    glViewport(0, 0, context.framebufferWidth, context.framebufferHeight);
    Util::ClearScreen();

    // 进入渲染管线
    renderPipeline->StartRender(context);
}

void Renderer::UploadLightData()
{
    // 灯光信息传递
    LightInputData lightData{};

    int localLightCount = 0;
    int DirectionalLightCount = 0;

    for (int i = 0; i < EntityManager::Get().m_Lights.size(); i++)
    {
        auto light = EntityManager::Get().m_Lights[i]->GetComponent<Light>();

        if (!light)
            continue;

        if (DirectionalLight* directional = dynamic_cast<DirectionalLight*>(light))
        {
            // 目前只接受一个定向光
            if (DirectionalLightCount != 0)
            {
                static bool s_loggedExtraDir = false;
                if (!s_loggedExtraDir)
                {
                    LogA(LogLevel::WARNING, "UploadLightData: only one directional light supported, extras ignored");
                    s_loggedExtraDir = true;
                }
                continue;
            }
            lightData._MainLightDir = glm::vec4(directional->GetDirection(), directional->m_bias);
            // _MainLightColor.rgb = color × DirectionalLuxToShaderLi(lux)；见 LightingConvention.h
            lightData._MainLightColor =
                glm::vec4(directional->GetColor() * directional->GetShaderRadianceScale(), directional->m_pcfSample);
            lightData._MainLightMatrix = directional->GetProjectMatrix() * directional->GetViewMatrix();
            lightData.param1.x = directional->m_nearPlane;
            lightData.param1.y = directional->m_farPlane;
            DirectionalLightCount++;
        }
        // 局部灯光
        else if (LocalLight* local = dynamic_cast<LocalLight*>(light))
        {
            if (localLightCount >= Config::MaxLocalLight)
                continue;

            LocalLightData data{};
            data.Position = local->GetPosition();
            // color.rgb = color × lumens；shader 衰减 1/(4π·dist²)，见 LightingConvention.h
            data.Color = glm::vec4(local->GetColor() * local->GetShaderRadianceScale(), 0.0);
            data.Direction = local->GetDirection();
            data.inCutOff = glm::cos(glm::radians(local->m_inCutoff));
            data.outCutOff = glm::cos(glm::radians(local->m_outCutoff));

            data.param1.x = local->m_nearPlane;
            data.param1.y = local->m_farPlane;
            data.param1.z = local->m_bias;
            data.param1.w = (float)local->m_pcfSample;

            for (int face = 0; face < 6; face++)
                data.matrix[face] = local->GetProjectMatrix() * local->GetViewMatrix(static_cast<Direction>(face));

            lightData._localLights[localLightCount] = data;
            localLightCount++;
        }
    }

    auto e_skybox = EntityManager::Get().skyBox;
    if (e_skybox)
    {
        if (SkyBox* c_skybox = e_skybox->GetComponent<SkyBox>())
            lightData._AmbientColor = c_skybox->GetColor() * c_skybox->GetIntensity();
    }

    lightData.LocalLightCount = localLightCount;

    if (EntityManager::Get().skyBox)
    {
        if (SkyBox* sky = EntityManager::Get().skyBox->GetComponent<SkyBox>())
        {
            lightData.param1.z = sky->IsIBLLightingEnabled() ? 1.0f : 0.0f;
            lightData.param1.w = sky->GetIBLLightingIntensity();
            if (std::shared_ptr<IBLImage>& ibl = sky->GetIBL(); ibl && ibl->prefiltered)
                lightData._MaxReflectionLOD =
                    static_cast<float>(std::max(0, ibl->prefiltered->GetMipLevels() - 1));
        }
    }

    lightBuffer.UploadStruct(lightData);
}

void Renderer::UpdateCameraData(RenderContext& context)
{
    static bool s_loggedNoCameraEntity = false;
    static bool s_loggedNoCameraComponent = false;
    if (!context.currentCamera)
    {
        if (!s_loggedNoCameraEntity)
        {
            LogA(LogLevel::WARNING, "Render skipped: no currentCamera in RenderContext");
            s_loggedNoCameraEntity = true;
        }
        return;
    }
    Camera* cam = context.currentCamera->GetComponent<Camera>();
    if (!cam)
    {
        if (!s_loggedNoCameraComponent)
        {
            LogA(LogLevel::ERROR, "currentCamera entity has no Camera component");
            s_loggedNoCameraComponent = true;
        }
        return;
    }
    cam->SetAspect((float)context.width / (context.height > 0 ? context.height : 1));
    cam->SyncExposureFromImaging();

    // 相机信息传递
    CameraInputData camData{};
    camData.GL_MATRIX_V_NO_MOVE = glm::mat4(glm::mat3(cam->GetViewMatrix()));
    camData.GL_MATRIX_P = cam->GetProjectionMatrix();
    camData.GL_I_MATRIX_P = glm::inverse(cam->GetProjectionMatrix());
    camData.GL_MATRIX_V = cam->GetViewMatrix();
    camData.GL_I_MATRIX_V = glm::inverse(cam->GetViewMatrix());
    camData._CameraPosition_zNear = glm::vec4(cam->GetPosition(), cam->GetNear());
    camData._zFar_pad.x = cam->GetFar();
    camData._zFar_pad.y = context.sceneViewportWidth;
    camData._zFar_pad.z = context.sceneViewportHeight;
    camData._time_pad.x = context.engineTime;
    camData._time_pad.y = cam->GetFov();
    cameraBuffer.UploadStruct(camData);
    // 后处理信息传递
    context.enable_blooom = cam->postSetting.bloom_setting.w > 0;
    context.aoMode = AmbientOcclusionModeFromFloat(cam->postSetting.sso_extra.x);
    PostProcessBuffer.UploadStruct(cam->postSetting);
}

#undef MODULE
