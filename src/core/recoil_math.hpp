#pragma once

#include "recoil_types.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace Recoil {
namespace Math {

// 1.2.1: Menghitung ulang nilai kumulatif cum_x dan cum_y dari rentetan delta_x dan delta_y
inline void ComputeCumulativeDisplacements(std::vector<ShotData>& shots) {
    float running_x = 0.0f;
    float running_y = 0.0f;

    for (size_t i = 0; i < shots.size(); ++i) {
        running_x += shots[i].delta_x;
        running_y += shots[i].delta_y;
        shots[i].cum_x = running_x;
        shots[i].cum_y = running_y;
    }
}

// 1.2.2: Menghitung interval waktu (ms) antar tembakan berturutan
inline void ComputeShotIntervals(std::vector<ShotData>& shots) {
    if (shots.empty()) return;

    shots[0].interval_ms = 0.0f;
    for (size_t i = 1; i < shots.size(); ++i) {
        int64_t diff = shots[i].timestamp_ms - shots[i - 1].timestamp_ms;
        shots[i].interval_ms = static_cast<float>(diff);
    }
}

// Rekalkulasi lengkap seluruh turunan (Index 1-based, Cumulative, Interval)
inline void RecalculateShotSequence(std::vector<ShotData>& shots, const std::string& source_label = "derived") {
    float running_x = 0.0f;
    float running_y = 0.0f;

    for (size_t i = 0; i < shots.size(); ++i) {
        shots[i].shot_index = static_cast<int>(i + 1); // 1-based index
        
        running_x += shots[i].delta_x;
        running_y += shots[i].delta_y;
        shots[i].cum_x = running_x;
        shots[i].cum_y = running_y;

        if (i == 0) {
            shots[i].interval_ms = 0.0f;
        } else {
            shots[i].interval_ms = static_cast<float>(shots[i].timestamp_ms - shots[i - 1].timestamp_ms);
        }

        if (!source_label.empty()) {
            shots[i].source = source_label;
        }
    }
}

// Mengonversi RPM (Rounds Per Minute) ke target interval milidetik
inline float RpmToIntervalMs(int rpm) {
    if (rpm <= 0) return 100.0f;
    return 60000.0f / static_cast<float>(rpm);
}

// Mengonversi rata-rata interval ke estimasi RPM
inline int IntervalMsToRpm(float interval_ms) {
    if (interval_ms <= 0.001f) return 0;
    return static_cast<int>(std::round(60000.0f / interval_ms));
}

// Total horizontal drift (akumulasi absolut atau total simpangan)
inline float CalculateTotalDriftX(const std::vector<ShotData>& shots) {
    return shots.empty() ? 0.0f : shots.back().cum_x;
}

// Total vertical drift (akumulasi vertikal)
inline float CalculateTotalDriftY(const std::vector<ShotData>& shots) {
    return shots.empty() ? 0.0f : shots.back().cum_y;
}

} // namespace Math
} // namespace Recoil
