#include "RenderUnitFilter.h"

#include "../Asset/Types/Material.h"
#include "../Asset/Types/ShaderTags.h"
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

RenderUnitFilter RenderUnitFilter::HasLightModePass(const std::string& lightMode)
{
    return RenderUnitFilter([lightMode](const RenderUnit& unit)
                            {
                                if (!unit.meshRenser)
                                    return false;
                                Material* mat =
                                    unit.meshRenser->GetMaterial(static_cast<int>(unit.sectionIndex)).get();
                                if (!mat)
                                    return false;
                                const std::shared_ptr<Shader> shader = mat->GetShader();
                                return shader && ShaderHasLightModePass(*shader, lightMode);
                            });
}

RenderUnitFilter RenderUnitFilter::LacksLightModePass(const std::string& lightMode)
{
    return RenderUnitFilter([lightMode](const RenderUnit& unit)
                            {
                                if (!unit.meshRenser)
                                    return false;
                                Material* mat =
                                    unit.meshRenser->GetMaterial(static_cast<int>(unit.sectionIndex)).get();
                                if (!mat)
                                    return true;
                                const std::shared_ptr<Shader> shader = mat->GetShader();
                                return !shader || !ShaderHasLightModePass(*shader, lightMode);
                            });
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
