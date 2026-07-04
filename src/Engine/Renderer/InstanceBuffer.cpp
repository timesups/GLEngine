#include "InstanceBuffer.h"

InstanceBuffer& InstanceBuffer::Get()
{
    static InstanceBuffer instance;
    return instance;
}

void InstanceBuffer::BeginFrame()
{
    m_frameData.clear();
    m_offsets.clear();
    m_counts.clear();
}

int InstanceBuffer::Allocate(const std::vector<GPUInstanceData>& instances)
{
    const int allocationId = static_cast<int>(m_offsets.size());
    const int offset = static_cast<int>(m_frameData.size());
    m_offsets.push_back(offset);
    m_counts.push_back(static_cast<int>(instances.size()));
    m_frameData.insert(m_frameData.end(), instances.begin(), instances.end());
    return allocationId;
}

void InstanceBuffer::Bind() const
{
    if (m_frameData.empty())
        return;

    const size_t byteSize = m_frameData.size() * sizeof(GPUInstanceData);
    if (!m_ssbo)
        glGenBuffers(1, &m_ssbo);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
    if (byteSize > m_capacity)
    {
        m_capacity = byteSize;
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_capacity, m_frameData.data(), GL_DYNAMIC_DRAW);
    }
    else
    {
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, byteSize, m_frameData.data());
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_ssbo);
}
