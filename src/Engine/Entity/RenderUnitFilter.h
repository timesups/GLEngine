#pragma once

#include <functional>
#include <limits>
#include <string>

struct RenderUnit;

/// 按 RenderQueue 范围与 MeshRender / Shader 等条件筛选 RenderUnit。
class RenderUnitFilter
{
  public:
    RenderUnitFilter() = default;
    explicit RenderUnitFilter(std::function<bool(const RenderUnit&)> predicate);

    bool Accepts(const RenderUnit& unit) const;
    bool AllowsInstancing() const;

    static RenderUnitFilter None();
    static RenderUnitFilter Queue(int minQueueInclusive, int maxQueueExclusive);
    static RenderUnitFilter Opaque();
    static RenderUnitFilter Transparent();
    static RenderUnitFilter CastShadow();
    static RenderUnitFilter DrawCustomDepth();
    static RenderUnitFilter PerObjectRender();
    static RenderUnitFilter HasLightModePass(const std::string& lightMode);
    static RenderUnitFilter LacksLightModePass(const std::string& lightMode);
    static RenderUnitFilter HasRenderPipelinePass(const std::string& renderPipeline);
    static RenderUnitFilter LacksRenderPipelinePass(const std::string& renderPipeline);
    static RenderUnitFilter And(RenderUnitFilter lhs, RenderUnitFilter rhs);

  private:
    bool IsIdentity() const;
    bool AcceptsQueue(const RenderUnit& unit) const;
    bool AcceptsPredicate(const RenderUnit& unit) const;

    int m_minQueueInclusive = 0;
    int m_maxQueueExclusive = std::numeric_limits<int>::max();
    std::function<bool(const RenderUnit&)> m_predicate;
};
