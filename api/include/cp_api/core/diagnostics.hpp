#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <limits>
#include <chrono>
#include <mutex>
#include "cp_api/core/debug.hpp"

namespace cp_api {

    class HighResolutionTimer {
    public:
        HighResolutionTimer() : m_start(0), m_end(0) {}

        void Start() { m_start = Now(); }
        void End()   { m_end = Now(); }

        [[nodiscard]] double GetElapsedSeconds() const {
            return static_cast<double>(m_end - m_start) * 1e-6; // micros → seconds
        }

    private:
        static uint64_t Now() {
            using namespace std::chrono;
            return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
        }

        uint64_t m_start;
        uint64_t m_end;
    };

    // ---------------------------
    // TimerSampler: coleta e processa samples
    // ---------------------------
    class TimerSampler {
    public:
        void AddSample(double milliseconds) {
            m_samples.push_back(milliseconds);
            m_sampleCount++;

            if (milliseconds < m_min) m_min = milliseconds;
            if (milliseconds > m_max) m_max = milliseconds;

            // Atualiza média incrementalmente
            m_average = ((m_average * (m_sampleCount - 1)) + milliseconds) / m_sampleCount;
        }

        double GetAverage() const { return m_average; }
        double GetMin() const { return m_min; }
        double GetMax() const { return m_max; }
        size_t GetSampleCount() const { return m_sampleCount; }

        // opcional: histórico completo
        const std::vector<double>& GetSamples() const { return m_samples; }

    private:
        std::vector<double> m_samples;
        double m_average = 0.0;
        double m_min = std::numeric_limits<double>::max();
        double m_max = 0.0;
        size_t m_sampleCount = 0;
    };

    // ---------------------------
    // Estrutura para armazenar dados de frame
    // ---------------------------
    struct FrameData {
        uint64_t totalFrames = 0;

        struct TimeInfo {
            double deltaTime = 0.0;
        } timeInfo;

        struct FpsInfo {
            uint32_t current = 0;
            uint32_t average = 0;
            uint32_t min = std::numeric_limits<uint32_t>::max();
            uint32_t max = 0;
        } fpsInfo;
    };

    // ---------------------------
    // FrameCounter: mede FPS
    // ---------------------------
    class FrameCounter {
    public:
        explicit FrameCounter(size_t warmupFrames = 10)
            : m_frameCount(0), m_warmupFrames(warmupFrames), m_started(false) {}

        void StartFrame() {
            if (m_started) return;
            m_started = true;
            m_lastTime = Now();
        }

        void EndFrame() {
            if (!m_started) return;
            const uint64_t now = Now();
            const double delta = static_cast<double>(now - m_lastTime) * 1e-6; // ms -> s
            m_frameCount++;

            if (m_frameCount > m_warmupFrames) {
                m_frameData.timeInfo.deltaTime = delta;
                m_frameData.totalFrames++;

                uint32_t fps = static_cast<uint32_t>(1.0 / delta);
                m_frameData.fpsInfo.current = fps;

                if (m_frameData.totalFrames == 1) {
                    m_frameData.fpsInfo.average = fps;
                    m_frameData.fpsInfo.min = fps;
                    m_frameData.fpsInfo.max = fps;
                } else {
                    m_frameData.fpsInfo.average =
                        static_cast<uint32_t>((m_frameData.fpsInfo.average * (m_frameData.totalFrames - 1) + fps)
                                              / m_frameData.totalFrames);
                    m_frameData.fpsInfo.min = std::min(m_frameData.fpsInfo.min, fps);
                    m_frameData.fpsInfo.max = std::max(m_frameData.fpsInfo.max, fps);
                }
            }

            m_started = false;
        }

        const FrameData& GetFrameData() const { return m_frameData; }

    private:
        static uint64_t Now() {
            using namespace std::chrono;
            return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
        }

        FrameData m_frameData;
        uint64_t m_lastTime;
        bool m_started;
        size_t m_frameCount;
        size_t m_warmupFrames;
    };

    // ---------------------------
    // DiagnosticsManager: gerencia timers e frame counter
    // ---------------------------
    class DiagnosticsManager {
    public:
        explicit DiagnosticsManager(size_t warmupFrames = 10)
            : m_frameCounter(warmupFrames) {}

        void BeginFrame() { m_frameCounter.StartFrame(); }
        void EndFrame() { m_frameCounter.EndFrame(); }

        void StartTimer(const std::string& name) {
            m_timerStartTimes[name] = Now();
        }

        void StopTimer(const std::string& name) {
            auto it = m_timerStartTimes.find(name);
            if (it == m_timerStartTimes.end()) return; // timer não iniciado

            double elapsedMs = static_cast<double>(Now() - it->second) * 1e-3;
            m_timerSamplers[name].AddSample(elapsedMs);
            m_timerStartTimes.erase(it);
        }

        const FrameData& GetFrameData() const { return m_frameCounter.GetFrameData(); }

        // acesso ao TimerSampler de forma segura
        const TimerSampler& GetTimerSampler(const std::string& name) const {
            static TimerSampler dummy;
            auto it = m_timerSamplers.find(name);
            return (it != m_timerSamplers.end()) ? it->second : dummy;
        }

        // resumo em string
        std::string Summary() const {
            std::string out;
            const auto& fd = m_frameCounter.GetFrameData();
            out += "Frame " + std::to_string(fd.totalFrames) + " | FPS " +
                   std::to_string(fd.fpsInfo.current) +
                   " (avg " + std::to_string(fd.fpsInfo.average) +
                   ", min " + std::to_string(fd.fpsInfo.min) +
                   ", max " + std::to_string(fd.fpsInfo.max) + ")\n";

            for (const auto& [name, sampler] : m_timerSamplers) {
                out += "  " + name + " : " +
                       std::to_string(sampler.GetAverage()) + " ms" +
                       " (min " + std::to_string(sampler.GetMin()) +
                       ", max " + std::to_string(sampler.GetMax()) + ")\n";
            }
            return out;
        }

    private:
        static uint64_t Now() {
            using namespace std::chrono;
            return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
        }

        FrameCounter m_frameCounter;
        std::unordered_map<std::string, uint64_t> m_timerStartTimes;
        std::unordered_map<std::string, TimerSampler> m_timerSamplers;
    };

} // namespace cp_api
