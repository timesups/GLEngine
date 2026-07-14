#include "RenderUnitFilter.h"

#include "../Asset/Types/Material.h"
#include "../Asset/Types/RenderQueue.h"
#include "../Asset/Types/ShaderTags.h"
#include "Components/MeshRender.h"
#include "EntityManager.h"

RenderUnitFilter::RenderUnitFilter(std::function<bool(const RenderUnit&)> predicate)
    : m_predicate(std::move(predicate))
{
}

bool RenderUnitFilter::IsIdentity() const
{
    return !m_predicate && m_minQueueInclusive == 0 &&
           m_maxQueueExclusive == std::numeric_limits<int>::max();
}

bool RenderUnitFilter::AcceptsQueue(const RenderUnit& unit) const
{
    return unit.renderQueue >= m_minQueueInclusive && unit.renderQueue < m_maxQueueExclusive;
}

bool RenderUnitFilter::AcceptsPredicate(const RenderUnit& unit) const
{
    return !m_predicate || m_predicate(unit);
}

bool RenderUnitFilter::Accepts(const RenderUnit& unit) const
{
    return AcceptsQueue(unit) && AcceptsPredicate(unit);
}

bool RenderUnitFilter::AllowsInstancing() const
{
    return m_minQueueInclusive < RenderQueue::Transparent;
}

RenderUnitFilter RenderUnitFilter::None()
{
    return {};
}

RenderUnitFilter RenderUnitFilter::Queue(int minQueueInclusive, int maxQueueExclusive)
{
    RenderUnitFilter filter;
    filter.m_minQueueInclusive = minQueueInclusive;
    filter.m_maxQueueExclusive = maxQueueExclusive;
    return filter;
}

RenderUnitFilter RenderUnitFilter::Opaque()
{
    return Queue(0, RenderQueue::OpaqueUpperBound);
}

RenderUnitFilter RenderUnitFilter::Transparent()
{
    return Queue(RenderQueue::Transparent, std::numeric_limits<int>::max());
}

RenderUnitFilter RenderUnitFilter::CastShadow()
{
    return RenderUnitFilter([](const RenderUnit& unit)
                            { return unit.meshRender && unit.meshRender->GetCastShadow(); });
}

RenderUnitFilter RenderUnitFilter::DrawCustomDepth()
{
    return RenderUnitFilter([](const RenderUnit& unit)
                            { return unit.meshRender && unit.meshRender->GetDrawCustomDepth(); });
}

RenderUnitFilter RenderUnitFilter::DrawOutline()
{
    return RenderUnitFilter([](const RenderUnit& unit)
                            { return unit.meshRender && unit.meshRender->GetDrawOutline(); });
}

RenderUnitFilter RenderUnitFilter::PerObjectRender()
{
    return RenderUnitFilter([](const RenderUnit& unit)
                            { return unit.meshRender && unit.meshRender->GetPerObjectRender(); });
}

RenderUnitFilter RenderUnitFilter::HasShaderTag(const std::string& tag)
{
    return RenderUnitFilter([tag](const RenderUnit& unit)
                            {
                                if (!unit.meshRender)
                                    return false;
                                Material* mat =
                                    unit.meshRender->GetMaterial(static_cast<int>(unit.sectionIndex)).get();
                                if (!mat)
                                    return false;
                                const std::shared_ptr<Shader> shader = mat->GetShader();
                                return shader && ShaderHasShaderTag(*shader, tag);
                            });
}

RenderUnitFilter RenderUnitFilter::LacksShaderTag(const std::string& tag)
{
    return RenderUnitFilter([tag](const RenderUnit& unit)
                            {
                                if (!unit.meshRender)
                                    return false;
                                Material* mat =
                                    unit.meshRender->GetMaterial(static_cast<int>(unit.sectionIndex)).get();
                                if (!mat)
                                    return true;
                                const std::shared_ptr<Shader> shader = mat->GetShader();
                                return !shader || !ShaderHasShaderTag(*shader, tag);
                            });
}

RenderUnitFilter RenderUnitFilter::And(RenderUnitFilter lhs, RenderUnitFilter rhs)
{
    if (lhs.IsIdentity())
        return rhs;
    if (rhs.IsIdentity())
        return lhs;

    RenderUnitFilter result;
    result.m_minQueueInclusive = std::max(lhs.m_minQueueInclusive, rhs.m_minQueueInclusive);
    result.m_maxQueueExclusive = std::min(lhs.m_maxQueueExclusive, rhs.m_maxQueueExclusive);
    result.m_predicate = [l = std::move(lhs), r = std::move(rhs)](const RenderUnit& unit)
    { return l.AcceptsPredicate(unit) && r.AcceptsPredicate(unit); };
    return result;
}
