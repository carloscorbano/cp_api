#pragma once

#include <string>
#include <fmt/core.h>
#include <fmt/std.h>
#include <ostream>
#include <mutex>

namespace cp_api {   
    /**
    * @enum LogLevel
    * @brief Representa os níveis de severidade de log.
    */
    enum class LogLevel {
        Info,
        Success,
        Warn,
        Error,
        Debug
    };

    /**
     * @class Debug
     * @brief Classe utilitária para logging com cores, níveis e saída configurável.
     *
     * - Em Debug: todos os níveis são exibidos por padrão.
     * - Em Release: todos os níveis funcionam, mas podem ser filtrados via SetMinimumLevel().
     * - Suporte a cores ANSI (opcional) e fallback seguro no Windows.
     * - Thread-safe para uso em aplicações multithread.
     */
    class Debug {
    public:
        /// Ativa ou desativa cores ANSI.
        static void SetColorEnabled(bool enabled);

        /// Define o nível mínimo exibido nos logs.
        static void SetMinimumLevel(LogLevel level);

        /// Define se deve dar flush após cada log.
        static void SetAutoFlush(bool enabled);

        /// Define saída para arquivo (substitui stdout).
        static void SetLogFile(const std::string& filepath);

        /// Reseta saída para console padrão.
        static void ResetOutputToConsole();

        /// Log genérico com formatação.
        template <typename... Args>
        static void Log(LogLevel level, fmt::format_string<Args...> format, Args&&... args) {
            if (level < g_minLevel) return;

            const std::string msg = fmt::format(format, std::forward<Args>(args)...);
            Print(level, msg);
        }

        /// Função interna para exibir mensagem com formatação final.
        static void Print(LogLevel level, const std::string& message);
        
    private:
        static inline bool g_colorEnabled = true;
        static inline bool g_autoFlush = true;
        static inline LogLevel g_minLevel =
    #ifndef NDEBUG
            LogLevel::Info;
    #else
            LogLevel::Warn;
    #endif

        static inline std::ostream* g_outputStream = nullptr;
        static inline std::mutex g_mutex;
    };
}

// ------------------- Macros para uso simplificado -------------------
#define CP_LOG_INFO(fmt_str, ...)    ::cp_api::Debug::Log(::cp_api::LogLevel::Info, fmt_str, ##__VA_ARGS__)
#define CP_LOG_SUCCESS(fmt_str, ...) ::cp_api::Debug::Log(::cp_api::LogLevel::Success, fmt_str, ##__VA_ARGS__)
#define CP_LOG_WARN(fmt_str, ...)    ::cp_api::Debug::Log(::cp_api::LogLevel::Warn, fmt_str, ##__VA_ARGS__)
#define CP_LOG_ERROR(fmt_str, ...)   ::cp_api::Debug::Log(::cp_api::LogLevel::Error, fmt_str, ##__VA_ARGS__)
#define CP_LOG_DEBUG(fmt_str, ...)   ::cp_api::Debug::Log(::cp_api::LogLevel::Debug, fmt_str, ##__VA_ARGS__)
