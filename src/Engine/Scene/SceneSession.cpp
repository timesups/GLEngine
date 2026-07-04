#include "SceneSession.h"

SceneSession& SceneSession::Get()
{
    static SceneSession instance;
    return instance;
}

const std::string& SceneSession::GetCurrentPath() const
{
    return m_currentPath;
}

bool SceneSession::IsDirty() const
{
    return m_isDirty;
}

void SceneSession::SetCurrentPath(const std::string& path)
{
    m_currentPath = path;
}

void SceneSession::MarkDirty()
{
    m_isDirty = true;
}

void SceneSession::ClearDirty()
{
    m_isDirty = false;
}

void SceneSession::Reset()
{
    m_currentPath.clear();
    m_isDirty = false;
}
