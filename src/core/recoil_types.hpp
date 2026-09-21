#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "json.hpp"

namespace Recoil {

// Status profile senjata sesuai PRD 6.1
enum class ProfileStatus {
    Draft,
    Tested,
    Approved
};

inline std::string ProfileStatusToString(ProfileStatus status) {
    switch (status) {
        case ProfileStatus::Draft: return "Draft";
        case ProfileStatus::Tested: return "Tested";
        case ProfileStatus::Approved: return "Approved";
        default: return "Draft";
    }
}

inline ProfileStatus StringToProfileStatus(const std::string& str) {
    if (str == "Tested") return ProfileStatus::Tested;
    if (str == "Approved") return ProfileStatus::Approved;
    return ProfileStatus::Draft;
}

// Data point individual shot sesuai PRD 6.3
struct ShotData {
    int shot_index = 0;              // Urutan shot dalam burst (1-based)
    int64_t timestamp_ms = 0;        // Waktu relatif terhadap awal burst (ms)
    float delta_x = 0.0f;            // Perpindahan horizontal shot ini
    float delta_y = 0.0f;            // Perpindahan vertikal shot ini
    float cum_x = 0.0f;              // Akumulasi perpindahan X
    float cum_y = 0.0f;              // Akumulasi perpindahan Y
    float interval_ms = 0.0f;        // Jarak waktu dari shot sebelumnya (ms)
    std::string source = "raw";      // "raw", "cleaned", atau "derived"

    // Serialisasi JSON
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ShotData, shot_index, timestamp_ms, delta_x, delta_y, cum_x, cum_y, interval_ms, source)
};

// Satu sesi perekaman burst (Raw recording immutable sesuai PRD 6.2 & 6.5)
struct BurstRecording {
    std::string recording_id;
    std::string timestamp_iso;       // Waktu saat recording dibuat
    std::string notes;
    bool is_visible = true;          // Flag visibilitas untuk overlay chart (Task 3.3.3)
    std::vector<ShotData> raw_shots; // IMMUTABLE: Tidak boleh di-overwrite saat cleaning

    // Helper checks
    bool IsEmpty() const { return raw_shots.empty(); }
    size_t ShotCount() const { return raw_shots.size(); }
    int64_t TotalDurationMs() const {
        return raw_shots.empty() ? 0 : raw_shots.back().timestamp_ms;
    }
};

// Parameter pembersihan & normalisasi yang diterapkan pada raw recording
struct CleaningSettings {
    int trim_head = 0;                      // Potong N shot awal
    int trim_tail = 0;                      // Potong M shot akhir
    bool remove_outliers = false;           // Deteksi outlier otomatis
    float outlier_std_multiplier = 2.5f;    // Threshold deviasi standar
    bool apply_smoothing = false;           // Terapkan moving average smoothing
    int smoothing_window = 3;               // Ukuran window (ganjil, misal 3 atau 5)
};

// Pattern hasil olahan / tuning (Derived / Cleaned pattern)
struct ProcessedPattern {
    std::string pattern_name = "Default Tuned Pattern";
    std::vector<std::string> source_recording_ids; // ID rekaman sumber
    CleaningSettings settings;
    std::vector<ShotData> shots;                   // Data shot yang sudah dibersihkan/diedit
    bool is_locked = false;                        // Jika true, mencegah edit tidak sengaja
};

// Weapon Profile utama sesuai PRD 6.1
struct WeaponProfile {
    std::string weapon_id = "weapon_default";
    std::string weapon_name = "New Weapon";
    std::string fire_mode = "Full-Auto";
    int fire_rate_rpm = 600;
    int magazine_size = 30;
    std::string notes = "";
    int profile_version = 1;                       // Versioning sederhana (Task 1.3.2)
    ProfileStatus status = ProfileStatus::Draft;

    // Koleksi raw recordings (selalu disimpan tanpa ditimpa)
    std::vector<BurstRecording> recordings;

    // Pattern aktif yang sudah di-clean / di-tune untuk diekspor
    ProcessedPattern active_pattern;

    // Bump versioning
    void IncrementVersion() {
        profile_version++;
    }
};

} // namespace Recoil
