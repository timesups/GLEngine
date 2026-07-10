#pragma once
#include <string>

class ComputeShader
{
  public:
    ComputeShader();
    ComputeShader(const char* code);
    ~ComputeShader();

    void Use();
    bool LoadFromCode(const char* code);
    bool IsReady() const
    {
        return m_isReady;
    }
    unsigned int GetProgramId() const
    {
        return m_id;
    }
    std::string m_name;
    std::string m_path;

  private:
    unsigned int m_id = 0;
    bool m_isReady = false;
};
