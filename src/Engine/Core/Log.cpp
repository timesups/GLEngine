#include "Log.h"

LogManager& LogManager::Get()
{
    static LogManager instance;
    return instance;
}

LogManager::LogManager() = default;

LogManager::~LogManager() = default;

bool LogManager::GetALog(LogEntry& logOut)
{
    if (logBuffer.empty())
        return false;

    logOut = std::move(logBuffer.front());
    logBuffer.pop_front();
    return true;
}

void LogManager::addLog(const std::string& module, LogLevel level, const std::string& text)
{
    const char* levelStr = "";
    switch (level)
    {
    case LogLevel::INFO:
        levelStr = "INFO";
        break;
    case LogLevel::WARNING:
        levelStr = "WARN";
        break;
    case LogLevel::ERROR:
        levelStr = "ERROR";
        break;
    }
    constexpr int kModuleW = 14;
    constexpr int kLevelW = 5;
    LogEntry entry;
    entry.level = level;
    entry.text = std::format("[{:<{}}] [{:<{}}] {}\n", module, kModuleW, levelStr, kLevelW, text);
    logBuffer.push_back(std::move(entry));
}
