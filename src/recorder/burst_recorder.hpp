#pragma once

#include "../core/recoil_types.hpp"
#include "../core/recoil_math.hpp"
#include "precision_timer.hpp"
#include "telemetry_adapter.hpp"

#include <vector>
#include <mutex>
#include <atomic>
#include <string>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace Recoil {

// State Machine state sesuai Task 2.4.1
enum class RecorderState {
    Idle,           // Siap merekam
    Recording,      // Sedang aktif merekam tembakan burst
    BurstFinished   // Burst selesai (baik via manual stop atau auto-finalize)
};

inline std::string RecorderStateToString(RecorderState state) {
    switch (state) {
        case RecorderState::Idle: return "IDLE";
        case RecorderState::Recording: return "RECORDING";
        case RecorderState::BurstFinished: return "BURST_FINISHED";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Controller State Machine Burst Recorder (Task 2.4)
 * Menangani siklus hidup perekaman burst secara thread-safe, pencatatan waktu berpresisi tinggi,
 * live shot counter, dan auto-finalization timeout saat burst berhenti.
 */
class BurstRecorder {
private:
    mutable std::mutex m_mutex;
    std::atomic<RecorderState> m_state{RecorderState::Idle};

    PrecisionTimer m_timer;
    std::atomic<int> m_liveShotCounter{0};
    double m_lastShotTimeMs{0.0};

    // Auto-finalize settings (Task 2.4.2)
    bool m_autoFinalizeEnabled{true};
    int64_t m_autoFinalizeTimeoutMs{500}; // Selesai jika tidak ada tembakan selama > 500ms

    // Data sesi aktif
    std::string m_currentBurstId;
    std::string m_timestampIso;
    std::string m_sourceName{"raw"};
    std::vector<ShotData> m_shots;

public:
    BurstRecorder() = default;

    // Menghubungkan recorder langsung ke telemetry adapter
    void AttachAdapter(ITelemetryAdapter* adapter) {
        if (!adapter) return;
        adapter->SetCallback([this](float dx, float dy, int64_t timestamp_ms) {
            this->OnShotReceived(dx, dy, timestamp_ms);
        });
    }

    // Task 2.4.1: Mulai merekam burst baru
    void StartRecording(const std::string& custom_id = "", const std::string& source = "raw") {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!custom_id.empty()) {
            m_currentBurstId = custom_id;
        } else {
            static int counter = 1;
            std::ostringstream ss;
            ss << "burst_" << std::setw(3) << std::setfill('0') << counter++;
            m_currentBurstId = ss.str();
        }

        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        char time_buf[64];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c));
        m_timestampIso = time_buf;

        m_sourceName = source;
        m_shots.clear();
        m_liveShotCounter = 0;
        m_lastShotTimeMs = 0.0;

        m_timer.Start();
        m_state = RecorderState::Recording;
    }

    // Ingest shot event (Thread-safe, dapat dipanggil dari thread socket UDP)
    void OnShotReceived(float delta_x, float delta_y, int64_t timestamp_ms = -1) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_state != RecorderState::Recording) {
            return; // Abaikan shot di luar sesi recording aktif
        }

        // Hitung waktu jika tidak diberikan dari luar
        int64_t shotTimeMs = timestamp_ms;
        double nowElapsed = m_timer.GetElapsedMilliseconds();
        if (shotTimeMs < 0) {
            shotTimeMs = static_cast<int64_t>(std::round(nowElapsed));
        }

        // Update live counter (Task 2.4.3)
        int index = ++m_liveShotCounter;

        ShotData shot;
        shot.shot_index = index;
        shot.timestamp_ms = shotTimeMs;
        shot.delta_x = delta_x;
        shot.delta_y = delta_y;
        shot.source = m_sourceName;

        // Hitung kumulatif & interval live
        if (m_shots.empty()) {
            shot.cum_x = delta_x;
            shot.cum_y = delta_y;
            shot.interval_ms = 0.0f;
        } else {
            shot.cum_x = m_shots.back().cum_x + delta_x;
            shot.cum_y = m_shots.back().cum_y + delta_y;
            shot.interval_ms = static_cast<float>(shotTimeMs - m_shots.back().timestamp_ms);
        }

        m_shots.push_back(shot);
        m_lastShotTimeMs = nowElapsed;
    }

    // Task 2.4.2: Update / Tick loop untuk auto-finalize
    void Update() {
        if (m_state != RecorderState::Recording) return;

        if (m_autoFinalizeEnabled && m_liveShotCounter > 0) {
            double nowElapsed = m_timer.GetElapsedMilliseconds();
            if ((nowElapsed - m_lastShotTimeMs) >= static_cast<double>(m_autoFinalizeTimeoutMs)) {
                // Timeout tercapai: finalize burst secara otomatis
                StopRecording();
            }
        }
    }

    // Berhenti merekam manual
    void StopRecording() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state == RecorderState::Recording) {
            m_timer.Stop();
            m_state = RecorderState::BurstFinished;
        }
    }

    // Reset ke state IDLE
    void Reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_timer.Stop();
        m_shots.clear();
        m_liveShotCounter = 0;
        m_lastShotTimeMs = 0.0;
        m_state = RecorderState::Idle;
    }

    // Finalisasi dan ambil objek BurstRecording immutable
    BurstRecording FinalizeBurst() {
        std::lock_guard<std::mutex> lock(m_mutex);

        BurstRecording recording;
        recording.recording_id = m_currentBurstId;
        recording.timestamp_iso = m_timestampIso;
        recording.notes = "Recorded burst (" + std::to_string(m_shots.size()) + " shots, " + m_sourceName + ")";
        recording.raw_shots = m_shots;

        // Pastikan matematika kumulatif & interval 100% konsisten
        Math::ComputeCumulativeDisplacements(recording.raw_shots);
        Math::ComputeShotIntervals(recording.raw_shots);

        m_state = RecorderState::Idle;
        return recording;
    }

    // Getters & Setters
    RecorderState GetState() const { return m_state; }
    bool IsRecording() const { return m_state == RecorderState::Recording; }
    bool IsFinished() const { return m_state == RecorderState::BurstFinished; }
    bool IsIdle() const { return m_state == RecorderState::Idle; }

    int GetLiveShotCount() const { return m_liveShotCounter; }
    double GetElapsedMs() const { return m_timer.GetElapsedMilliseconds(); }

    void SetAutoFinalize(bool enabled, int64_t timeout_ms = 500) {
        m_autoFinalizeEnabled = enabled;
        m_autoFinalizeTimeoutMs = timeout_ms;
    }

    bool IsAutoFinalizeEnabled() const { return m_autoFinalizeEnabled; }
    int64_t GetAutoFinalizeTimeoutMs() const { return m_autoFinalizeTimeoutMs; }

    std::vector<ShotData> GetLiveShotsCopy() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_shots;
    }
};

} // namespace Recoil
