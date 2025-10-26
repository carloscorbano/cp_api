#include "cp_api/core/debug.hpp"

#include <chrono>
#include <iostream>
#include <fstream>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

namespace cp_api {
    namespace {
        static const char* GetColor(LogLevel level, bool colorEnabled) {
            if (!colorEnabled) return "";

            switch (level) {
            case LogLevel::Info:    return "\033[1;37m"; // Branco
            case LogLevel::Success: return "\033[1;32m"; // Verde
            case LogLevel::Warn:    return "\033[1;33m"; // Amarelo
            case LogLevel::Error:   return "\033[1;31m"; // Vermelho
            case LogLevel::Debug:   return "\033[1;36m"; // Ciano
            default:                return "\033[0m";
            }
        }

        static const char* GetLevelName(LogLevel level) {
            switch (level) {
            case LogLevel::Info:    return "INFO";
            case LogLevel::Success: return "SUCCESS";
            case LogLevel::Warn:    return "WARN";
            case LogLevel::Error:   return "ERROR";
            case LogLevel::Debug:   return "DEBUG";
            default:                return "LOG";
            }
        }

        static std::string GetTimestamp() {
            using namespace std::chrono;
            auto now = system_clock::now();
            auto time = system_clock::to_time_t(now);
            std::tm localTime{};
        #ifdef _WIN32
            localtime_s(&localTime, &time);
        #else
            localtime_r(&time, &localTime);
        #endif
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "[%02d:%02d:%02d]",
                          localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
            return buffer;
        }

        static std::unique_ptr<std::ofstream> fileStream;

        static void EnableVirtualTerminalOnWindows() {
        #ifdef _WIN32
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut == INVALID_HANDLE_VALUE) return;
            DWORD mode = 0;
            if (!GetConsoleMode(hOut, &mode)) return;
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        #endif
        }
    }

    void Debug::Print(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(g_mutex);

        const char* color = GetColor(level, g_colorEnabled);
        const char* reset = g_colorEnabled ? "\033[0m" : "";
        const char* levelStr = GetLevelName(level);
        std::string timestamp = GetTimestamp();

        auto& out = g_outputStream ? *g_outputStream : std::cout;
        out << color << timestamp << " [" << levelStr << "] " << message << reset << "\n";

        if (g_autoFlush)
            out.flush();
    }

    void Debug::SetColorEnabled(bool enabled) {
        g_colorEnabled = enabled;
        if (enabled)
            EnableVirtualTerminalOnWindows();
    }

    void Debug::SetMinimumLevel(LogLevel level) {
        g_minLevel = level;
    }

    void Debug::SetAutoFlush(bool enabled) {
        g_autoFlush = enabled;
    }

    void Debug::SetLogFile(const std::string& filepath) {
        fileStream = std::make_unique<std::ofstream>(filepath, std::ios::app);
        if (fileStream && fileStream->is_open()) {
            g_outputStream = fileStream.get();
        } else {
            g_outputStream = nullptr;
        }
    }

    void Debug::ResetOutputToConsole() {
        g_outputStream = nullptr;
        fileStream.reset();
    }
} // namespace CPFramework
