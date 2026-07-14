#pragma once

#include <memory>
#include <string>
#include <vector>

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

/// 一次 DrawRenderQueue 调用的绘制设置（Shader Tag、材质覆盖、排序等）。
/// 对象筛选由独立的 RenderUnitFilter 负责，二者不要混用。
/// Shader Tag 为纯文本 "Key:Value"（如 "LightMode:Deferred"），大小写不敏感。
struct DrawSetting
{
    std::shared_ptr<Material> materialOverride;
    std::vector<std::string> shaderTags;
    DrawSortMode sortMode = DrawSortMode::RenderQueueThenBackToFront;
};
