#pragma once

#include <cstddef>
#include <type_traits>

#include <glad/glad.h>

class UniformBuffer
{
  public:
    UniformBuffer();

    explicit UniformBuffer(size_t sizeBytes, GLenum usage = GL_STATIC_DRAW);
    ~UniformBuffer();

    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;

    UniformBuffer(UniformBuffer&& other) noexcept;
    UniformBuffer& operator=(UniformBuffer&& other) noexcept;

    void Create(size_t sizeBytes, GLenum usage = GL_DYNAMIC_DRAW);
    void Destroy();

    bool IsValid() const;
    GLuint GetId() const;
    size_t GetByteSize() const;

    void BindBase(GLuint uniformBlockBinding) const;
    void Upload(const void* data, size_t size, size_t offset = 0);

    template <typename T> void UploadStruct(const T& value, size_t offset = 0)
    {
        static_assert(std::is_trivially_copyable_v<T>, "UBO payload must be trivially copyable");
        Upload(&value, sizeof(T), offset);
    }

  private:
    GLuint m_id = 0;
    size_t m_size = 0;
    GLenum m_usage = GL_DYNAMIC_DRAW;
};
