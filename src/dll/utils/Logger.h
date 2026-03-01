#pragma once
#include <memory>
#include <string>
#include <spdlog/spdlog.h>

class Logger
{
public:
    // Get the singleton instance
    static std::shared_ptr<spdlog::logger> Get();

    // Initialize the logger (called once at startup)
    // If userDir is provided, logs will be written there; otherwise falls back to Documents\SimCity 4
    // If logToFile is false, only the MSVC debug output sink is created
    static void Initialize(const std::string& logName = "UnknownDllMod", const std::string& userDir = "",
                           bool logToFile = true);

    // Set the log level (and flush level) at runtime
    static void SetLevel(spdlog::level::level_enum logLevel);

    // Shutdown the logger (called at exit)
    static void Shutdown();

private:
    static std::shared_ptr<spdlog::logger> s_logger;
    static std::string s_logName;
    static bool s_initialized;

    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

// Convenience macros for logging
#define LOG_TRACE(...) Logger::Get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::Get()->debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::Get()->info(__VA_ARGS__)
#define LOG_WARN(...) Logger::Get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::Get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::Get()->critical(__VA_ARGS__)