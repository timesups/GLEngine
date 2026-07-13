#include "DrawSetting.h"

#include "../Asset/Types/Material.h"
#include "../Core/Config.h"

ShaderPassDrawTags ShaderPassDrawTags::Default()
{
    return {};
}

ShaderPassDrawTags ShaderPassDrawTags::WithLightMode(const std::string& lightMode)
{
    ShaderPassDrawTags tags;
    tags.lightMode = lightMode;
    return tags;
}

DrawSetting DrawSetting::Default()
{
    return {};
}

DrawSetting& DrawSetting::WithMaterialOverride(std::shared_ptr<Material> material)
{
    materialOverride = std::move(material);
    return *this;
}

DrawSetting& DrawSetting::WithFilter(RenderUnitFilter unitFilter)
{
    filter = std::move(unitFilter);
    return *this;
}

DrawSetting& DrawSetting::WithPassTags(ShaderPassDrawTags tags)
{
    passTags = std::move(tags);
    return *this;
}

DrawSetting& DrawSetting::WithLightMode(const std::string& lightMode)
{
    passTags.lightMode = lightMode;
    return *this;
}

DrawSetting& DrawSetting::WithRenderPipeline(const std::string& renderPipeline)
{
    passTags.renderPipeline = renderPipeline;
    return *this;
}

DrawSetting& DrawSetting::WithSortMode(DrawSortMode mode)
{
    sortMode = mode;
    return *this;
}

Material* DrawSetting::GetMaterialOverride() const
{
    return materialOverride.get();
}

std::string DrawSetting::ResolveRenderPipeline() const
{
    if (!passTags.renderPipeline.empty())
        return passTags.renderPipeline;
    return Config::Get().renderPipeline;
}
