#pragma once
#include <string>

class SceneSession
{
  public:
    static SceneSession& Get();

    const std::string& GetCurrentPath() const;
    bool IsDirty() const;

    void SetCurrentPath(const std::string& path);
    void MarkDirty();
    void ClearDirty();
    void Reset();

  private:
    SceneSession() = default;

    std::string m_currentPath;
    bool m_isDirty = false;
};
