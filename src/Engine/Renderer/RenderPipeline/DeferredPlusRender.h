#pragma once

#include "DeferredRender.h"

/// 基于 DeferredRender 的扩展管线占位；当前行为与 Deferred 相同，后续在此 override Pass。
class DeferredPlusRender : public DeferredRender
{
  public:
    DeferredPlusRender() = default;
    ~DeferredPlusRender() override = default;

    const char* GetName() const override
    {
        return "DeferredPlus";
    }
};
