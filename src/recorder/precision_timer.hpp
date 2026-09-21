#pragma once

#include <windows.h>
#include <cstdint>

namespace Recoil {

/**
 * @brief High-precision timer wrapper using Windows QueryPerformanceCounter (QPC).
 * Sesuai Task 2.1: Menjamin resolusi sub-milidetik (< 1 microsecond) tanpa drift.
 */
class PrecisionTimer {
private:
    LARGE_INTEGER m_frequency{};
    LARGE_INTEGER m_startTime{};
    LARGE_INTEGER m_lastTime{};
    bool m_running{false};

public:
    PrecisionTimer() {
        ::QueryPerformanceFrequency(&m_frequency);
        m_startTime.QuadPart = 0;
        m_lastTime.QuadPart = 0;
    }

    // Memulai timer dari 0
    void Start() {
        ::QueryPerformanceCounter(&m_startTime);
        m_lastTime = m_startTime;
        m_running = true;
    }

    // Reset hitungan waktu ke titik sekarang
    void Reset() {
        if (m_running) {
            ::QueryPerformanceCounter(&m_startTime);
            m_lastTime = m_startTime;
        } else {
            m_startTime.QuadPart = 0;
            m_lastTime.QuadPart = 0;
        }
    }

    // Menghentikan timer
    void Stop() {
        m_running = false;
    }

    bool IsRunning() const {
        return m_running;
    }

    // Total waktu berlalu sejak Start() dalam milidetik (presisi pecahan desimal)
    double GetElapsedMilliseconds() const {
        if (!m_running || m_frequency.QuadPart == 0) return 0.0;
        LARGE_INTEGER current;
        ::QueryPerformanceCounter(&current);
        return static_cast<double>(current.QuadPart - m_startTime.QuadPart) * 1000.0 / static_cast<double>(m_frequency.QuadPart);
    }

    // Total waktu berlalu dalam milidetik integer
    int64_t GetElapsedMillisecondsInt() const {
        return static_cast<int64_t>(GetElapsedMilliseconds());
    }

    // Total waktu berlalu dalam mikrosekon
    double GetElapsedMicroseconds() const {
        if (!m_running || m_frequency.QuadPart == 0) return 0.0;
        LARGE_INTEGER current;
        ::QueryPerformanceCounter(&current);
        return static_cast<double>(current.QuadPart - m_startTime.QuadPart) * 1000000.0 / static_cast<double>(m_frequency.QuadPart);
    }

    // Waktu yang berlalu sejak pemanggilan GetDeltaMilliseconds() sebelumnya (berguna untuk interval per-shot)
    double GetDeltaMilliseconds() {
        if (!m_running || m_frequency.QuadPart == 0) return 0.0;
        LARGE_INTEGER current;
        ::QueryPerformanceCounter(&current);
        double delta = static_cast<double>(current.QuadPart - m_lastTime.QuadPart) * 1000.0 / static_cast<double>(m_frequency.QuadPart);
        m_lastTime = current;
        return delta;
    }

    // Frekuensi timer QPC hardware Windows (Hz)
    int64_t GetFrequencyHz() const {
        return m_frequency.QuadPart;
    }
};

} // namespace Recoil
