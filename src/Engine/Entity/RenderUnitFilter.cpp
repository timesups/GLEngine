#include "RenderUnitFilter.h"

#include "Components/MeshRender.h"
#include "EntityManager.h"

RenderUnitFilter::RenderUnitFilter(std::function<bool(const RenderUnit&)> predicate)
    : m_predicate(std::move(predicate))
{
}

bool RenderUnitFilter::Accepts(const RenderUnit& unit) const
{
    return !m_predicate || m_predicate(unit);
}

RenderUnitFilter RenderUnitFilter::None()
{
    return {};
}

RenderUnitFilter RenderUnitFilter::CastShadow()
{
    return RenderUnitFilter([](const RenderUnit& unit)
                            { return unit.meshRenser && unit.meshRenser->GetCastShadow(); });
}

RenderUnitFilter RenderUnitFilter::DrawCustomDepth()
{
    return RenderUnitFilter([](const RenderUnit& unit)
                            { return unit.meshRenser && unit.meshRenser->GetDrawCustomDepth(); });
}

RenderUnitFilter RenderUnitFilter::PerObjectRender()
{
    return RenderUnitFilter([](const RenderUnit& unit)
                            { return unit.meshRenser && unit.meshRenser->GetPerObjectRender(); });
}

RenderUnitFilter RenderUnitFilter::And(RenderUnitFilter lhs, RenderUnitFilter rhs)
{
    if (!lhs)
        return rhs;
    if (!rhs)
        return lhs;

    return RenderUnitFilter([l = std::move(lhs), r = std::move(rhs)](const RenderUnit& unit)
                            { return l.Accepts(unit) && r.Accepts(unit); });
}
