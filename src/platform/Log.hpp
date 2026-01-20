#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace coomer {

enum class LogLevel { Info, Warn, Error, Debug };

inline bool g_debug_enabled = false;
inline FILE* g_log_file = nullptr;

inline void setDebugLogging(bool enabled) {
    g_debug_enabled = enabled;
}

inline void initFileLogging() {
    const char* logPath = std::getenv("COOMER_LOG_FILE");
    if (logPath && logPath[0] != '\0') {
        g_log_file = std::fopen(logPath, "a");
        if (g_log_file) {
            std::fprintf(g_log_file, "\n=== coomer started ===\n");
            std::fflush(g_log_file);
        }
    }
}

inline void closeFileLogging() {
    if (g_log_file) {
        std::fclose(g_log_file);
        g_log_file = nullptr;
    }
}

inline void logMessage(LogLevel level, const char* fmt, ...) {
    if (level == LogLevel::Debug && !g_debug_enabled) {
        return;
    }
    const char* tag = "INFO";
    switch (level) {
        case LogLevel::Info:
            tag = "INFO";
            break;
        case LogLevel::Warn:
            tag = "WARN";
            break;
        case LogLevel::Error:
            tag = "ERROR";
            break;
        case LogLevel::Debug:
            tag = "DEBUG";
            break;
    }

    va_list args;
    va_start(args, fmt);

    // Log to stderr
    std::fprintf(stderr, "[%s] ", tag);
    va_list args_copy;
    va_copy(args_copy, args);
    std::vfprintf(stderr, fmt, args_copy);
    va_end(args_copy);
    std::fprintf(stderr, "\n");

    // Log to file if enabled
    if (g_log_file) {
        std::fprintf(g_log_file, "[%s] ", tag);
        va_copy(args_copy, args);
        std::vfprintf(g_log_file, fmt, args_copy);
        va_end(args_copy);
        std::fprintf(g_log_file, "\n");
        std::fflush(g_log_file);
    }

    va_end(args);
}

}  // namespace coomer

#define LOG_INFO(...) \
    ::coomer::logMessage(::coomer::LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...) \
    ::coomer::logMessage(::coomer::LogLevel::Warn, __VA_ARGS__)
#define LOG_ERROR(...) \
    ::coomer::logMessage(::coomer::LogLevel::Error, __VA_ARGS__)
#define LOG_DEBUG(...) \
    ::coomer::logMessage(::coomer::LogLevel::Debug, __VA_ARGS__)
