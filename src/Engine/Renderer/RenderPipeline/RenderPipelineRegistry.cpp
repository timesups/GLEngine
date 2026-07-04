#include "RenderPipelineRegistry.h"

#include "../../Core/LightingConvention.h"
#include "../../Core/Log.h"
#include "RenderPipeline.h"

#include <cctype>
#include <unordered_map>

#define MODULE "RenderPipelineRegistry"

namespace
{
struct RenderPipelineEntry
{
    RenderPipelineRegistry::Factory factory;
};

std::unordered_map<std::string, RenderPipelineEntry>& Entries()
{
    static std::unordered_map<std::string, RenderPipelineEntry> entries;
    return entries;
}

std::vector<std::string>& RegistrationOrder()
{
    static std::vector<std::string> order;
    return order;
}

std::string SanitizePipelineMacroName(const std::string& name)
{
    std::string sanitized;
    sanitized.reserve(name.size());
    for (char c : name)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
            sanitized.push_back(c);
    }
    if (sanitized.empty())
        sanitized = "Unknown";
    return sanitized;
}
} // namespace

bool RenderPipelineRegistry::Register(const char* name, Factory factory)
{
    if (!name || !name[0] || !factory)
        return false;

    const std::string key(name);
    auto& entries = Entries();
    if (!entries.contains(key))
        RegistrationOrder().push_back(key);

    entries[key] = RenderPipelineEntry{std::move(factory)};
    return true;
}

bool RenderPipelineRegistry::Exists(const std::string& name)
{
    return Entries().contains(name);
}

std::vector<std::string> RenderPipelineRegistry::GetRegisteredNames()
{
    return RegistrationOrder();
}

std::string RenderPipelineRegistry::GetDefaultName()
{
    const std::string defaultName = LightingConvention::DirectionalCalibrationBaseline::kDefaultRenderPipeline;
    if (Exists(defaultName))
        return defaultName;

    const std::vector<std::string>& order = RegistrationOrder();
    if (!order.empty())
        return order.front();

    return defaultName;
}

std::string RenderPipelineRegistry::ResolveName(const std::string& requested)
{
    if (!requested.empty() && Exists(requested))
        return requested;

    const std::string fallback = GetDefaultName();
    if (!requested.empty() && requested != fallback)
    {
        LogA(LogLevel::WARNING, "Render pipeline '{}' is not registered, fallback to '{}'", requested, fallback);
    }
    return fallback;
}

std::string RenderPipelineRegistry::BuildShaderDefines(const std::string& pipelineName)
{
    const std::string resolved = ResolveName(pipelineName);
    std::string defines = "#define GLE_PIPELINE_" + SanitizePipelineMacroName(resolved) + "\n";

    // 兼容现有 shader 中的 FORWARDRENDER / DEFERREDRENDER 分支（仅内置两条管线）
    if (resolved == "Forward")
        defines += "#define FORWARDRENDER\n";
    else if (resolved == "Deferred")
        defines += "#define DEFERREDRENDER\n";

    return defines;
}

std::unique_ptr<RenderPipeline> RenderPipelineRegistry::Create(const std::string& name)
{
    const std::string resolved = ResolveName(name);
    const auto it = Entries().find(resolved);
    if (it == Entries().end() || !it->second.factory)
        return nullptr;
    return it->second.factory();
}

#undef MODULE
