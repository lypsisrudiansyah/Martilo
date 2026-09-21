#pragma once

#include "../core/recoil_types.hpp"
#include "../core/recoil_math.hpp"
#include <random>
#include <cmath>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace Recoil {

// Konfigurasi generator simulator tembakan
struct BurstGeneratorConfig {
    int shot_count = 30;                 // Jumlah peluru dalam 1 burst
    int rpm = 720;                       // Rate of fire (misal 720 RPM -> ~83.3ms)
    float base_vertical_recoil = -2.8f;  // Hentakan vertikal awal (negatif = ke atas)
    float vertical_decay_factor = 0.98f; // Pelunakan hentakan vertikal setelah peluru awal
    float horizontal_drift = 0.35f;      // Kecenderungan belok horizontal (misal drift ke kanan)
    float horizontal_wobble_freq = 0.3f; // Frekuensi osilasi goyangan kiri-kanan
    float random_spread = 0.4f;          // Noise / ketidakpastian acak
    float timing_jitter_ms = 1.5f;       // Variasi realistis frame/tick timing (ms)
    uint32_t seed = 0;                   // 0 = random waktu saat ini, >0 = deterministic
};

/**
 * @brief Mock / Simulator Telemetry Generator untuk Dev & Test (Task 2.2).
 * Menghasilkan kurva burst realistis (naik tajam di awal, stabil di tengah, goyang di akhir)
 * tanpa memerlukan game engine pihak ketiga yang sedang aktif.
 */
class MockBurstGenerator {
public:
    static BurstRecording GenerateBurst(const BurstGeneratorConfig& config, const std::string& custom_id = "") {
        BurstRecording recording;

        // ID & Timestamp ISO
        if (!custom_id.empty()) {
            recording.recording_id = custom_id;
        } else {
            static int counter = 1;
            std::ostringstream ss;
            ss << "sim_burst_" << std::setw(3) << std::setfill('0') << counter++;
            recording.recording_id = ss.str();
        }

        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        char time_buf[64];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c));
        recording.timestamp_iso = time_buf;
        recording.notes = "Simulated burst (" + std::to_string(config.shot_count) + " shots @ " + std::to_string(config.rpm) + " RPM)";

        if (config.shot_count <= 0) return recording;

        // Setup random engine
        uint32_t effective_seed = config.seed != 0 ? config.seed : static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::mt19937 rng(effective_seed);
        std::normal_distribution<float> spread_dist(0.0f, config.random_spread);
        std::normal_distribution<float> jitter_dist(0.0f, config.timing_jitter_ms);

        float ideal_interval = Math::RpmToIntervalMs(config.rpm);
        float current_time_ms = 0.0f;
        float current_vert_kick = config.base_vertical_recoil;

        recording.raw_shots.reserve(config.shot_count);

        for (int i = 0; i < config.shot_count; ++i) {
            ShotData shot;
            shot.shot_index = i + 1;
            shot.source = "simulated";

            if (i == 0) {
                // Shot pertama: titik awal (waktu 0, recoil vertikal murni dengan noise minim)
                shot.timestamp_ms = 0;
                shot.interval_ms = 0.0f;
                shot.delta_x = spread_dist(rng) * 0.2f;
                shot.delta_y = config.base_vertical_recoil;
            } else {
                // Update waktu dengan sedikit jitter
                float step_jitter = jitter_dist(rng);
                float step_interval = (std::max)(1.0f, ideal_interval + step_jitter);
                current_time_ms += step_interval;

                shot.timestamp_ms = static_cast<int64_t>(std::round(current_time_ms));
                shot.interval_ms = step_interval;

                // Dinamika Recoil:
                // 1. Vertikal: Tembakan 1-7 naik kuat, lalu mulai mendatar / konstan
                if (i < 8) {
                    current_vert_kick -= 0.12f; // Moncong makin terangkat
                } else {
                    current_vert_kick *= config.vertical_decay_factor; // Menstabil
                }
                shot.delta_y = current_vert_kick + spread_dist(rng);

                // 2. Horizontal: Tembakan awal lurus, tembakan lanjut mulai drift ke kanan/kiri + osilasi
                float phase = static_cast<float>(i) * config.horizontal_wobble_freq;
                float drift = (i > 3) ? (config.horizontal_drift * (std::sin(phase) + 0.35f)) : 0.0f;
                shot.delta_x = drift + spread_dist(rng);
            }

            recording.raw_shots.push_back(shot);
        }

        // Kalkulasi kumulatif
        Math::ComputeCumulativeDisplacements(recording.raw_shots);
        // Pastikan interval terhitung rapi
        Math::ComputeShotIntervals(recording.raw_shots);

        return recording;
    }
};

} // namespace Recoil
