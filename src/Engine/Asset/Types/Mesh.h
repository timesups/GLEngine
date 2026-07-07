#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <limits>
#include <memory>
#include <string>
#include <vector>

struct Bounds
{
    Bounds();
    Bounds(const glm::vec3& max, const glm::vec3& min);
    void FromMaxMinPoints(const glm::vec3& max, const glm::vec3& min);
    void ApplyMatrix(const glm::mat4& mat);
    const glm::vec3& GetMaxPoint();
    const glm::vec3& GetMinPoint();
    const glm::vec3& GetCenterPoint();

  private:
    std::vector<glm::vec3> m_initial_cornerPoints;
    glm::vec3 m_max = glm::vec3(std::numeric_limits<float>::min());
    glm::vec3 m_min = glm::vec3(std::numeric_limits<float>::max());
};

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec3 Color;
    glm::vec4 Tangent;
    glm::vec2 Texcoord0;
    glm::vec2 Texcoord1;
};

class Mesh;

struct MeshSection
{
    std::shared_ptr<Mesh> mesh;
    int materialIndex;
    std::string name;
};

class Mesh
{
  public:
    Mesh();
    ~Mesh();
    void Create(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const Bounds& bounds);
    void Draw(GLenum mode = GL_TRIANGLES) const;
    void DrawInstanced(GLenum mode, GLsizei instanceCount) const;
    size_t GetVertexCount() const;
    size_t GetIndexCount() const;

  public:
    std::string m_name;

  private:
    friend class Model;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    unsigned int m_ebo = 0;
    size_t m_vertexCount = 0;
    size_t m_indexCount = 0;
    Bounds m_bounds;
};
