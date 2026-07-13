#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

class RenderPipeline;

/// 渲染管线工厂注册表；各管线 .cpp 通过 REGISTER_RENDER_PIPELINE 静态注册。
class RenderPipelineRegistry
{
  public:
    using Factory = std::function<std::unique_ptr<RenderPipeline>()>;

    static bool Register(const char* name, Factory factory);

    static bool Exists(const std::string& name);
    static std::vector<std::string> GetRegisteredNames();
    static std::string GetDefaultName();
    /// 名称有效则原样返回，否则回退到默认管线并写日志。
    static std::string ResolveName(const std::string& requested);
    static std::unique_ptr<RenderPipeline> Create(const std::string& name);
};

#define GLE_GLUE_IMPL(x, y) x##y
#define GLE_GLUE(x, y) GLE_GLUE_IMPL(x, y)

#define REGISTER_RENDER_PIPELINE(PipelineName, PipelineType) \
    namespace \
    { \
    const bool GLE_GLUE(_gle_pipeline_reg_, __LINE__) = \
        (RenderPipelineRegistry::Register(PipelineName, \
                                          []() { return std::make_unique<PipelineType>(); }), \
         true); \
    }
