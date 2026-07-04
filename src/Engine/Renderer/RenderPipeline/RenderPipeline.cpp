#include "RenderPipeline.h"

#include "../../Asset/AssetManager.h"
#include "../../Asset/Types/Material.h"
#include "../../Asset/Types/Mesh.h"
#include "../../Asset/Types/Shader.h"
#include "../../Asset/Types/Texture/IBLImage.h"
#include "../../Asset/Types/Texture/Texture2D.h"
#include "../../Asset/Types/Texture/TextureCube.h"
#include "../../Core/Log.h"
#include "../../Core/util.h"
#include "../../Entity/Components/Light.h"
#include "../../Entity/Components/SkyBox.h"
#include "../../Entity/EntityManager.h"
#include "../../Entity/RenderUnitFilter.h"
#include "../CubemapBaker.h"
#include "../FramebufferDesc.h"
#include "../RenderContext.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>

#define MODULE "Render Pipeline"

bool RenderPipeline::Init(const int width, const int height)
{
    if (!OnInit(width, height))
        return false;

    // 创建各种framebuffer
    m_bufTransparent.Create("Draw Transparent", width, height, RenderTargetFormats::SceneColor());
    m_bufOpaqueLight.Create("Draw Opaque Light", width, height, RenderTargetFormats::SceneColor());
    m_buf_CustomDepth.Create("Draw CustomDepth", width, height, RenderTargetFormats::WaterBack());

    const TextureDesc bloomColor = RenderTargetFormats::BloomHalf();

    // 初始化Bloom所使用的FrameBuffer
    for (size_t i = 0; i < 5; i++)
    {
        const int divisor = static_cast<int>(glm::pow(2, static_cast<int>(i) + 1));
        const int w = std::max(1, width / divisor);
        const int h = std::max(1, height / divisor);

        auto& bufferDownSample = m_DwonSampleChain[i];
        std::string name = "DownSample 1/" + std::to_string(static_cast<int>(glm::pow(2, i + 2)));
        bufferDownSample.CreateColorOnly(name, w, h, bloomColor);

        const int bloomDivisor = static_cast<int>(glm::pow(2, 5 - static_cast<int>(i)));
        const int bloomW = std::max(1, width / bloomDivisor);
        const int bloomH = std::max(1, height / bloomDivisor);

        auto& bufferBloomX = m_BloomChainX[i];
        name = "BloomX 1/" + std::to_string(bloomDivisor);
        bufferBloomX.CreateColorOnly(name, bloomW, bloomH, bloomColor);

        auto& bufferBloomY = m_BloomChainY[i];
        name = "BloomY 1/" + std::to_string(bloomDivisor);
        bufferBloomY.CreateColorOnly(name, bloomW, bloomH, bloomColor);
    }

    m_buf_shadow.CreateDepth("Draw Direction Light Shadow", 512, 512);

    auto desc = RenderTargetFormats::ShadowDepth();
    desc.type = TextureType::TEXTURECUBEARRAY;
    desc.layerCount = Config::MaxLocalLight;
    if (!m_bufLocShadow.CreateDepth("Local Shadow Depth", 1024, 1024, desc))
    {
        LogA(LogLevel::ERROR, "Failed to create local shadow framebuffer (see FrameBuffer log above)");
        return false;
    }

    // 初始化所需要的材质
    m_shadowMaterialGloabl = AssetManager::Get().GetAsset<Material>("engine://materials/ShadowGlobal");
    m_shadowMaterialLocal = AssetManager::Get().GetAsset<Material>("engine://materials/ShadowLocal");

    if (!m_shadowMaterialGloabl || !m_shadowMaterialLocal)
    {
        LogA(LogLevel::ERROR, "Shadow pass: Shadow material not found");
        return false;
    }

    return true;
}
void RenderPipeline::StartRender(RenderContext& context)
{
    // 进入渲染管线
    Render(context);

    // 渲染其他屏幕元素
    if (context.ShowUI)
    {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Draw Icons");
        EntityManager::Get().DrawIcons();
        glPopDebugGroup();
    }
}

void RenderPipeline::BindLightShadowMap()
{
    m_buf_shadow.DepthAttachment().Bind(14);
    m_bufLocShadow.DepthAttachment().Bind(13);
}

void RenderPipeline::UnbinLightShadowMap()
{
    m_buf_shadow.DepthAttachment().UnBind();
    m_bufLocShadow.DepthAttachment().UnBind();
}

void RenderPipeline::BindIBLTextures()
{
    const std::shared_ptr<Entity>& skyBoxEntity = EntityManager::Get().skyBox;
    if (!skyBoxEntity)
        return;

    SkyBox* skyBox = skyBoxEntity->GetComponent<SkyBox>();
    if (!skyBox)
        return;

    std::shared_ptr<IBLImage>& ibl = skyBox->GetIBL();
    if (!ibl || !ibl->irradiance || !ibl->prefiltered)
        return;

    if (!skyBox->IsIBLLightingEnabled())
        return;

    ibl->irradiance->Bind(15);
    ibl->prefiltered->Bind(12);
    if (std::shared_ptr<Texture2D> brdfLut = CubemapBaker::EnsureBrdfLut())
        brdfLut->Bind(16);
}

void RenderPipeline::DrawShadowMap(RenderContext& context)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Draw Shadow Maps");
    int DirectionalLightCount = 0;

    for (int i = 0; i < EntityManager::Get().m_Lights.size(); i++)
    {
        auto light = EntityManager::Get().m_Lights[i]->GetComponent<Light>();

        if (!light)
            continue;

        if (light->type == LightType::Directional)
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
            // 绘制主光shadowmap
            m_buf_shadow.Bind(true, (int)light->m_ShadowRes, (int)light->m_ShadowRes);
            Util::ClearScreen(GL_DEPTH_BUFFER_BIT);
            EntityManager::Get().DrawRenderQueue(0, RenderQueue::OpaqueUpperBound, m_shadowMaterialGloabl,
                                                 RenderUnitFilter::CastShadow());
            m_buf_shadow.UnBind();
            DirectionalLightCount++;
        }
    }
    // 局部灯光
    m_bufLocShadow.Bind(true);
    Util::ClearScreen(GL_DEPTH_BUFFER_BIT);
    EntityManager::Get().DrawRenderQueue(0, RenderQueue::OpaqueUpperBound, m_shadowMaterialLocal,
                                         RenderUnitFilter::CastShadow());
    m_bufLocShadow.UnBind();
    glPopDebugGroup();
}

void RenderPipeline::DrawCustomDepth(RenderContext& context)
{
    m_buf_CustomDepth.Bind(true, context.sceneViewportWidth, context.sceneViewportHeight);
    Util::ClearScreen();
    auto mat = AssetManager::Get().GetAsset<Material>("engine://materials/CustomDepth");
    EntityManager::Get().DrawRenderQueue(0, RenderQueue::OpaqueUpperBound, mat, RenderUnitFilter::DrawCustomDepth());
    EntityManager::Get().DrawRenderQueue(RenderQueue::Transparent, std::numeric_limits<int>::max(), mat,
                                         RenderUnitFilter::DrawCustomDepth());
    m_buf_CustomDepth.UnBind();
}

void RenderPipeline::Bloom(RenderContext& context)
{
    const int sceneW = context.sceneViewportWidth;
    const int sceneH = context.sceneViewportHeight;
    auto bloomSetupShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/BloomSetup.glsl");
    auto downSampleShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/DownSample.glsl");
    auto blurXShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/BlurX.glsl");
    auto blurYShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/BlurY.glsl");
    auto upsampleShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/BloomUpsample.glsl");
    if (!bloomSetupShader || !downSampleShader || !blurXShader || !blurYShader || !upsampleShader)
    {
        static bool s_logged = false;
        if (!s_logged)
        {
            LogA(LogLevel::ERROR,
                 "PostProcess: missing builtin shader (BloomSetup/DownSample/BlurX/BlurY/BloomUpsample/Tonemap)");
            s_logged = true;
        }
        return;
    }
    auto mipSize = [&](int mipIndex) -> glm::ivec2
    {
        const int divisor = static_cast<int>(glm::pow(2, mipIndex + 1));
        return {std::max(1, sceneW / divisor), std::max(1, sceneH / divisor)};
    };

    // 亮部提取 + 下采样链
    const glm::ivec2 halfRes = mipSize(0);
    m_DwonSampleChain[0].Resize(halfRes.x, halfRes.y);

    m_bufTransparent.DrawBufferTo(m_DwonSampleChain[0], bloomSetupShader, "BloomSetup");

    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "DownSample Chain");
    for (int i = 1; i < 5; i++)
    {
        std::string name = "DownSample 1/" + std::to_string(static_cast<int>(glm::pow(2, i + 1)));
        const glm::ivec2 size = mipSize(i);
        m_DwonSampleChain[i].Resize(size.x, size.y);
        m_DwonSampleChain[i - 1].DrawBufferTo(m_DwonSampleChain[i], downSampleShader, name);
    }
    glPopDebugGroup();

    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Bloom");
    // 约定：每级双 pass 模糊结束后，完整结果在 BloomChainX；Y 仅作水平模糊临时缓冲。
    const glm::ivec2 smallestRes = mipSize(4);
    m_BloomChainY[0].Resize(smallestRes.x, smallestRes.y);
    m_DwonSampleChain[4].DrawBufferTo(m_BloomChainY[0], blurXShader, "BloomX 1/32");

    m_BloomChainX[0].Resize(smallestRes.x, smallestRes.y);
    m_BloomChainY[0].DrawBufferTo(m_BloomChainX[0], blurYShader, "BloomY 1/32");

    // 逐级上采样并叠加各 mip（dual-filter）
    for (int i = 1; i < 5; i++)
    {
        const int mipIndex = 4 - i;
        const glm::ivec2 size = mipSize(mipIndex);
        const int sizeDivide = static_cast<int>(glm::pow(2, mipIndex + 1));

        std::string upsampleName = "BloomUpsample 1/" + std::to_string(sizeDivide);
        m_BloomChainX[i].Resize(size.x, size.y);
        m_BloomChainX[i - 1].UpsampleBufferTo(m_DwonSampleChain[mipIndex], m_BloomChainX[i], upsampleShader,
                                              upsampleName);

        std::string blurXName = "BloomX 1/" + std::to_string(sizeDivide);
        m_BloomChainY[i].Resize(size.x, size.y);
        m_BloomChainX[i].DrawBufferTo(m_BloomChainY[i], blurXShader, blurXName);

        std::string blurYName = "BloomY 1/" + std::to_string(sizeDivide);
        m_BloomChainY[i].DrawBufferTo(m_BloomChainX[i], blurYShader, blurYName);
    }
    glPopDebugGroup();
}

void RenderPipeline::PostProcessing(RenderContext& context)
{
    // 后处理
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "PostProcess_Pass");
    // 设置后处理渲染通用的状态
    glDisable(GL_DEPTH_TEST);

    // 只有启用bloom才调用
    if (context.enable_blooom)
        Bloom(context);
    // Tonemap：线性空间合成后单次 tonemap
    auto tonemapShader = AssetManager::Get().GetAsset<Shader>("engine://shaders/Tonemap.glsl");
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Tone Map");
    glViewport(context.sceneViewportX, context.sceneViewportY, context.sceneViewportWidth, context.sceneViewportHeight);
    Util::ClearScreen();
    m_bufTransparent.ColorAttachment().Bind(0);
    tonemapShader->Use();
    tonemapShader->SetValue("_SceneColor", 0);
    if (context.enable_blooom)
    {
        m_BloomChainX[4].ColorAttachment().Bind(1);
        tonemapShader->SetValue("_BloomColor", 1);
    }
    AssetManager::Get().GetScreenMesh()->Draw();
    m_bufTransparent.ColorAttachment().UnBind();
    if (context.enable_blooom)
        m_BloomChainX[4].ColorAttachment().UnBind();
    glPopDebugGroup();

    glPopDebugGroup();
    // 复原状态
    glEnable(GL_DEPTH_TEST);
}

void RenderPipeline::DrawTransparent(RenderContext& context)
{
    m_bufTransparent.Bind(true, context.sceneViewportWidth, context.sceneViewportHeight);
    Util::ClearScreen(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_bufOpaqueLight.BlitTo(m_bufTransparent, FramebufferBlitMask::Color | FramebufferBlitMask::Depth);
    BindIBLTextures();
    m_buf_CustomDepth.ColorAttachment().Bind(10);
    m_bufOpaqueLight.ColorAttachment().Bind(11);
    EntityManager::Get().DrawRenderQueue(RenderQueue::Transparent, std::numeric_limits<int>::max());
    m_buf_CustomDepth.ColorAttachment().UnBind();
    m_bufOpaqueLight.ColorAttachment().UnBind();
    m_bufTransparent.UnBind();
}

#undef MODULE