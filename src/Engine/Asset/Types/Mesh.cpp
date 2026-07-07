#include "Mesh.h"

#include <cstddef>
#include <limits>

Bounds::Bounds() = default;

Bounds::Bounds(const glm::vec3& max, const glm::vec3& min)
{
    FromMaxMinPoints(max, min);
}

void Bounds::FromMaxMinPoints(const glm::vec3& max, const glm::vec3& min)
{
    m_initial_cornerPoints.clear();
    m_initial_cornerPoints.push_back(max);
    m_initial_cornerPoints.push_back(min);
    m_initial_cornerPoints.push_back(glm::vec3(max.x, max.y, min.z));
    m_initial_cornerPoints.push_back(glm::vec3(max.x, min.y, max.z));
    m_initial_cornerPoints.push_back(glm::vec3(max.x, min.y, min.z));
    m_initial_cornerPoints.push_back(glm::vec3(min.x, max.y, max.z));
    m_initial_cornerPoints.push_back(glm::vec3(min.x, max.y, min.z));
    m_initial_cornerPoints.push_back(glm::vec3(min.x, min.y, max.z));

    m_max = max;
    m_min = min;
}

void Bounds::ApplyMatrix(const glm::mat4& mat)
{
    m_min = glm::vec3(std::numeric_limits<float>::max());
    m_max = glm::vec3(std::numeric_limits<float>::min());
    for (size_t i = 0; i < m_initial_cornerPoints.size(); i++)
    {
        glm::vec3 transformed_point = glm::vec3(mat * glm::vec4(m_initial_cornerPoints[i], 1.0));
        m_max = glm::max(transformed_point, m_max);
        m_min = glm::min(transformed_point, m_min);
    }
}

const glm::vec3& Bounds::GetMaxPoint()
{
    return m_max;
}

const glm::vec3& Bounds::GetMinPoint()
{
    return m_min;
}

const glm::vec3& Bounds::GetCenterPoint()
{
    glm::vec3 point = m_max + m_min;
    return glm::vec3(point.x / 2, point.y / 2, point.z / 2);
}

Mesh::Mesh() = default;

Mesh::~Mesh()
{
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_ebo)
        glDeleteBuffers(1, &m_ebo);
}

void Mesh::Create(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const Bounds& bounds)
{
    m_vertexCount = vertices.size();
    m_indexCount = indices.size();
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_vertexCount * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indexCount * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Color));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Texcoord0));

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Texcoord1));

    glBindVertexArray(0);

    m_bounds = bounds;
}

void Mesh::Draw(GLenum mode) const
{
    if (m_indexCount > 0)
    {
        glBindVertexArray(m_vao);
        glDrawElements(mode, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }
}

void Mesh::DrawInstanced(GLenum mode, GLsizei instanceCount) const
{
    if (m_indexCount == 0 || instanceCount <= 0)
        return;
    glBindVertexArray(m_vao);
    glDrawElementsInstanced(mode, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, nullptr, instanceCount);
    glBindVertexArray(0);
}

size_t Mesh::GetVertexCount() const
{
    return m_vertexCount;
}

size_t Mesh::GetIndexCount() const
{
    return m_indexCount;
}
