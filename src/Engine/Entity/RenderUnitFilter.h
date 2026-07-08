#pragma once

#include <functional>
#include <string>

struct RenderUnit;

/// 按 MeshRender 等组件设置筛选 RenderUnit，供 DrawRenderQueue / BuildDrawBatches 使用。
class RenderUnitFilter
{
  public:
    RenderUnitFilter() = default;
    explicit RenderUnitFilter(std::function<bool(const RenderUnit&)> predicate);

    bool Accepts(const RenderUnit& unit) const;
    explicit operator bool() const
    {
        return static_cast<bool>(m_predicate);
    }

    static RenderUnitFilter None();
    static RenderUnitFilter CastShadow();
    static RenderUnitFilter DrawCustomDepth();
    static RenderUnitFilter PerObjectRender();
    static RenderUnitFilter HasLightModePass(const std::string& lightMode);
    static RenderUnitFilter LacksLightModePass(const std::string& lightMode);
    static RenderUnitFilter And(RenderUnitFilter lhs, RenderUnitFilter rhs);

  private:
    std::function<bool(const RenderUnit&)> m_predicate;
};
