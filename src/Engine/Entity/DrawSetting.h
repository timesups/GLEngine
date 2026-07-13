#pragma once

#include "RenderUnitFilter.h"

#include <memory>
#include <string>

class Material;

/// 绘制前对 RenderUnit 的排序方式。
enum class DrawSortMode
{
    /// 保持当前顺序（Gather 后的原始顺序）。
    None,
    /// 仅按 RenderQueue 升序。
    RenderQueue,
    /// 先按 RenderQueue，再对 Transparent 队列按深度从远到近。
    RenderQueueThenBackToFront,
};

/// Shader Pass 绘制标签（LightMode / RenderPipeline）。
struct ShaderPassDrawTags
{
    std::string lightMode;
    /// 为空时使用 Config::renderPipeline。
    std::string renderPipeline;

    static ShaderPassDrawTags Default();
    static ShaderPassDrawTags WithLightMode(const std::string& lightMode);
};

/// 一次 DrawRenderQueue 调用的绘制参数。
class DrawSetting
{
  public:
    std::shared_ptr<Material> materialOverride;
    ShaderPassDrawTags passTags;
    DrawSortMode sortMode = DrawSortMode::RenderQueueThenBackToFront;
    RenderUnitFilter filter = RenderUnitFilter::None();

    static DrawSetting Default();

    DrawSetting& WithMaterialOverride(std::shared_ptr<Material> material);
    DrawSetting& WithFilter(RenderUnitFilter unitFilter);
    DrawSetting& WithPassTags(ShaderPassDrawTags tags);
    DrawSetting& WithLightMode(const std::string& lightMode);
    DrawSetting& WithRenderPipeline(const std::string& renderPipeline);
    DrawSetting& WithSortMode(DrawSortMode mode);

    Material* GetMaterialOverride() const;
    std::string ResolveRenderPipeline() const;
};
