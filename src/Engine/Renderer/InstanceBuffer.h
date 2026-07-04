#pragma once
#include <cstddef>
#include <glad/glad.h>
#include <vector>

#include "DrawBatch.h"

class InstanceBuffer
{
  public:
    static InstanceBuffer& Get();

    // 每帧开始或首次使用前调用
    void BeginFrame();

    // 追加instance,返回该batch在ssbo中的起始index
    int Allocate(const std::vector<GPUInstanceData>& instances);

    // 在DrawInstaned 前绑定到3
    void Bind() const;

    int GetBatchOffset(int allocationId) const
    {
        return m_offsets[allocationId];
    }
    int GetBatchCount(int allocationId) const
    {
        return m_counts[allocationId];
    }

  private:
    InstanceBuffer() = default;
    mutable unsigned int m_ssbo = 0;
    mutable size_t m_capacity = 0;

    std::vector<GPUInstanceData> m_frameData;
    std::vector<int> m_offsets;
    std::vector<int> m_counts;
};