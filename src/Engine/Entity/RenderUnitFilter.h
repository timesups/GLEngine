#pragma once

#include <functional>
#include <limits>
#include <string>

struct RenderUnit;

/// 从 RenderUnit 列表中筛选本次要绘制的对象（队列范围、投射阴影、Pass 能力等）。
/// 与 DrawSetting 独立：本类只负责“画谁”，不负责 LightMode 等绘制设置。
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
    static RenderUnitFilter DrawOutline();
    static RenderUnitFilter PerObjectRender();
    /// 是否有匹配指定纯文本 Shader Tag 的 Pass（如 "LightMode:ShadowCaster"，大小写不敏感）。
    static RenderUnitFilter HasShaderTag(const std::string& tag);
    static RenderUnitFilter LacksShaderTag(const std::string& tag);
    static RenderUnitFilter And(RenderUnitFilter lhs, RenderUnitFilter rhs);

  private:
    bool IsIdentity() const;
    bool AcceptsQueue(const RenderUnit& unit) const;
    bool AcceptsPredicate(const RenderUnit& unit) const;

    int m_minQueueInclusive = 0;
    int m_maxQueueExclusive = std::numeric_limits<int>::max();
    std::function<bool(const RenderUnit&)> m_predicate;
};
