#pragma once

#include "recoil_types.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

namespace Recoil {
namespace Serialization {

// Serialisasi canonical JSON sesuai PRD Bagian 7
inline nlohmann::json ToCanonicalJson(const WeaponProfile& profile, const std::vector<ShotData>& shots) {
    nlohmann::json j;
    j["weaponId"] = profile.weapon_id;
    j["profileVersion"] = profile.profile_version;
    j["fireRateRpm"] = profile.fire_rate_rpm;
    
    nlohmann::json shotsArray = nlohmann::json::array();
    for (const auto& s : shots) {
        nlohmann::json shotJson;
        shotJson["index"] = s.shot_index;
        shotJson["timeMs"] = s.timestamp_ms;
        shotJson["dx"] = s.delta_x;
        shotJson["dy"] = s.delta_y;
        shotsArray.push_back(shotJson);
    }
    j["shots"] = shotsArray;
    return j;
}

// Deserialisasi dari canonical JSON PRD Bagian 7
inline bool FromCanonicalJson(const nlohmann::json& j, WeaponProfile& outProfile, std::vector<ShotData>& outShots) {
    try {
        if (j.contains("weaponId")) outProfile.weapon_id = j["weaponId"].get<std::string>();
        if (j.contains("profileVersion")) outProfile.profile_version = j["profileVersion"].get<int>();
        if (j.contains("fireRateRpm")) outProfile.fire_rate_rpm = j["fireRateRpm"].get<int>();

        outShots.clear();
        if (j.contains("shots") && j["shots"].is_array()) {
            float running_x = 0.0f;
            float running_y = 0.0f;
            int64_t prev_time = 0;

            for (const auto& item : j["shots"]) {
                ShotData s;
                s.shot_index = item.value("index", static_cast<int>(outShots.size() + 1));
                s.timestamp_ms = item.value("timeMs", 0LL);
                s.delta_x = item.value("dx", 0.0f);
                s.delta_y = item.value("dy", 0.0f);

                running_x += s.delta_x;
                running_y += s.delta_y;
                s.cum_x = running_x;
                s.cum_y = running_y;

                if (outShots.empty()) {
                    s.interval_ms = 0.0f;
                } else {
                    s.interval_ms = static_cast<float>(s.timestamp_ms - prev_time);
                }
                prev_time = s.timestamp_ms;
                s.source = "imported";

                outShots.push_back(s);
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Deserialization error: " << e.what() << std::endl;
        return false;
    }
}

// Simpan JSON string ke file
inline bool SaveJsonToFile(const std::string& filepath, const nlohmann::json& j) {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << j.dump(2);
    return true;
}

// Baca JSON string dari file
inline bool LoadJsonFromFile(const std::string& filepath, nlohmann::json& outJson) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    try {
        file >> outJson;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace Serialization
} // namespace Recoil
