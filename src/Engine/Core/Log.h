#pragma once
#include <deque>
#include <format>
#include <string>

enum class LogLevel
{
    INFO,
    WARNING,
    ERROR,
};

struct LogEntry
{
    LogLevel level = LogLevel::INFO;
    std::string text;
};

class LogManager
{
  public:
    static LogManager& Get();
    ~LogManager();
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    bool GetALog(LogEntry& logOut);
    void addLog(const std::string& module, LogLevel level, const std::string& text);

  private:
    LogManager();
    std::deque<LogEntry> logBuffer;
};

template <typename... Args> void Log(const char* module, LogLevel level, std::string_view format, Args&&... args)
{
    std::string text = std::vformat(format, std::make_format_args(args...));
    LogManager::Get().addLog(module, level, text);
}

#define LogA(level, format, ...) Log(MODULE, level, format, __VA_ARGS__)
