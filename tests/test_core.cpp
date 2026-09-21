#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include "../src/core/recoil_types.hpp"
#include "../src/core/recoil_math.hpp"

#include "../src/core/recoil_serialization.hpp"
#include "../src/recorder/precision_timer.hpp"
#include "../src/recorder/recoil_generator.hpp"

// Simple unit test assertions with clear output
#define ASSERT_TRUE(expr, msg) \
    if (!(expr)) { \
        std::cerr << "[FAIL] " << msg << " (" << #expr << ") at line " << __LINE__ << std::endl; \
        return 1; \
    } else { \
        std::cout << "[PASS] " << msg << std::endl; \
    }

#define ASSERT_NEAR(val1, val2, eps, msg) \
    if (std::abs((val1) - (val2)) > (eps)) { \
        std::cerr << "[FAIL] " << msg << ": Expected " << (val2) << ", got " << (val1) << " at line " << __LINE__ << std::endl; \
        return 1; \
    } else { \
        std::cout << "[PASS] " << msg << std::endl; \
    }

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "RUNNING CORE RECOIL TESTS (Milestone 1)" << std::endl;
    std::cout << "========================================" << std::endl;

    // --- TEST 1: Task 1.1 Model Data Instantiation ---
    {
        Recoil::WeaponProfile profile;
        profile.weapon_id = "rifle_ak47";
        profile.weapon_name = "AK-47 Custom";
        profile.fire_rate_rpm = 600;
        profile.magazine_size = 30;
        profile.status = Recoil::ProfileStatus::Draft;

        ASSERT_TRUE(profile.profile_version == 1, "Initial profile version should be 1");
        ASSERT_TRUE(profile.recordings.empty(), "Initial recordings list should be empty");
        ASSERT_TRUE(Recoil::ProfileStatusToString(profile.status) == "Draft", "Status string should match 'Draft'");
    }

    // --- TEST 2: Task 1.2.1 Cumulative Displacements Calculation ---
    {
        std::vector<Recoil::ShotData> shots(3);
        // Shot 1
        shots[0].delta_x = 0.5f;
        shots[0].delta_y = -2.0f;
        // Shot 2
        shots[1].delta_x = -0.2f;
        shots[1].delta_y = -3.5f;
        // Shot 3
        shots[2].delta_x = 1.0f;
        shots[2].delta_y = -4.0f;

        Recoil::Math::ComputeCumulativeDisplacements(shots);

        // Cum X: 0.5 -> 0.3 -> 1.3
        ASSERT_NEAR(shots[0].cum_x, 0.5f, 1e-4f, "Shot 1 Cum X is 0.5");
        ASSERT_NEAR(shots[1].cum_x, 0.3f, 1e-4f, "Shot 2 Cum X is 0.3");
        ASSERT_NEAR(shots[2].cum_x, 1.3f, 1e-4f, "Shot 3 Cum X is 1.3");

        // Cum Y: -2.0 -> -5.5 -> -9.5
        ASSERT_NEAR(shots[0].cum_y, -2.0f, 1e-4f, "Shot 1 Cum Y is -2.0");
        ASSERT_NEAR(shots[1].cum_y, -5.5f, 1e-4f, "Shot 2 Cum Y is -5.5");
        ASSERT_NEAR(shots[2].cum_y, -9.5f, 1e-4f, "Shot 3 Cum Y is -9.5");
    }

    // --- TEST 3: Task 1.2.2 Shot Intervals Calculation ---
    {
        std::vector<Recoil::ShotData> shots(3);
        shots[0].timestamp_ms = 1000;
        shots[1].timestamp_ms = 1083; // +83 ms
        shots[2].timestamp_ms = 1166; // +83 ms

        Recoil::Math::ComputeShotIntervals(shots);

        ASSERT_NEAR(shots[0].interval_ms, 0.0f, 1e-4f, "Shot 1 interval should be 0.0");
        ASSERT_NEAR(shots[1].interval_ms, 83.0f, 1e-4f, "Shot 2 interval should be 83.0");
        ASSERT_NEAR(shots[2].interval_ms, 83.0f, 1e-4f, "Shot 3 interval should be 83.0");
    }

    // --- TEST 4: Task 1.2 RPM & Interval Math Utility ---
    {
        // 600 RPM = 100ms interval
        ASSERT_NEAR(Recoil::Math::RpmToIntervalMs(600), 100.0f, 1e-4f, "600 RPM corresponds to 100ms");
        ASSERT_TRUE(Recoil::Math::IntervalMsToRpm(100.0f) == 600, "100ms interval corresponds to 600 RPM");

        // 720 RPM = 83.333ms interval
        ASSERT_NEAR(Recoil::Math::RpmToIntervalMs(720), 83.3333f, 0.01f, "720 RPM corresponds to ~83.33ms");
    }

    // --- TEST 5: Task 1.3 Immutability & Versioning Layer ---
    {
        Recoil::WeaponProfile weapon;
        weapon.weapon_id = "smg_mp5";
        weapon.profile_version = 1;

        // Raw burst recording
        Recoil::BurstRecording raw_rec;
        raw_rec.recording_id = "rec_001";
        raw_rec.timestamp_iso = "2026-09-21T10:00:00Z";

        Recoil::ShotData s1; s1.delta_x = 0.1f; s1.delta_y = -1.5f; s1.source = "raw";
        Recoil::ShotData s2; s2.delta_x = 0.3f; s2.delta_y = -2.0f; s2.source = "raw";
        raw_rec.raw_shots.push_back(s1);
        raw_rec.raw_shots.push_back(s2);

        weapon.recordings.push_back(raw_rec);

        // Derive processed pattern
        weapon.active_pattern.source_recording_ids.push_back(raw_rec.recording_id);
        weapon.active_pattern.shots = raw_rec.raw_shots; // copy
        weapon.active_pattern.shots[0].delta_x = 0.0f;    // modified in pattern editor
        weapon.active_pattern.shots[0].source = "cleaned";

        // Raw data must remain completely unchanged!
        ASSERT_NEAR(weapon.recordings[0].raw_shots[0].delta_x, 0.1f, 1e-4f, "Raw recording shot 0 must remain 0.1f (immutable)");
        ASSERT_TRUE(weapon.recordings[0].raw_shots[0].source == "raw", "Raw recording source remains 'raw'");

        // Version increment
        weapon.IncrementVersion();
        ASSERT_TRUE(weapon.profile_version == 2, "Profile version should increment to 2");
    }

    // --- TEST 6: PRD Section 7 Canonical JSON Round-Trip ---
    {
        std::string rawJsonStr = R"({
            "weaponId": "rifle_001",
            "profileVersion": 3,
            "fireRateRpm": 720,
            "shots": [
                { "index": 1, "timeMs": 0,   "dx": 0.0, "dy": -2.8 },
                { "index": 2, "timeMs": 83,  "dx": 0.4, "dy": -3.1 },
                { "index": 3, "timeMs": 167, "dx": 0.8, "dy": -3.7 }
            ]
        })";

        nlohmann::json inputJson = nlohmann::json::parse(rawJsonStr);
        Recoil::WeaponProfile loadedProfile;
        std::vector<Recoil::ShotData> loadedShots;

        bool ok = Recoil::Serialization::FromCanonicalJson(inputJson, loadedProfile, loadedShots);
        ASSERT_TRUE(ok, "Parse PRD sample JSON should succeed");
        ASSERT_TRUE(loadedProfile.weapon_id == "rifle_001", "Loaded weaponId should be rifle_001");
        ASSERT_TRUE(loadedProfile.profile_version == 3, "Loaded profileVersion should be 3");
        ASSERT_TRUE(loadedProfile.fire_rate_rpm == 720, "Loaded fireRateRpm should be 720");
        ASSERT_TRUE(loadedShots.size() == 3, "Loaded shots size should be 3");

        // Verify values
        ASSERT_NEAR(loadedShots[0].delta_y, -2.8f, 1e-4f, "Shot 1 dy = -2.8");
        ASSERT_NEAR(loadedShots[1].delta_y, -3.1f, 1e-4f, "Shot 2 dy = -3.1");
        ASSERT_NEAR(loadedShots[2].delta_y, -3.7f, 1e-4f, "Shot 3 dy = -3.7");

        // Check cumulative calculation
        ASSERT_NEAR(loadedShots[0].cum_y, -2.8f, 1e-4f, "Shot 1 cum_y = -2.8");
        ASSERT_NEAR(loadedShots[1].cum_y, -5.9f, 1e-4f, "Shot 2 cum_y = -5.9");
        ASSERT_NEAR(loadedShots[2].cum_y, -9.6f, 1e-4f, "Shot 3 cum_y = -9.6");

        // Re-export to canonical JSON
        nlohmann::json exportedJson = Recoil::Serialization::ToCanonicalJson(loadedProfile, loadedShots);
        ASSERT_TRUE(exportedJson["weaponId"] == "rifle_001", "Exported JSON weaponId matches");
        ASSERT_TRUE(exportedJson["profileVersion"] == 3, "Exported JSON profileVersion matches");
        ASSERT_TRUE(exportedJson["fireRateRpm"] == 720, "Exported JSON fireRateRpm matches");
        ASSERT_TRUE(exportedJson["shots"].size() == 3, "Exported JSON shots count matches");
    }

    // --- TEST 7: Milestone 2 Task 2.1 PrecisionTimer (Windows QPC) ---
    {
        Recoil::PrecisionTimer timer;
        ASSERT_TRUE(timer.GetFrequencyHz() > 0, "QPC timer hardware frequency must be > 0");
        ASSERT_TRUE(!timer.IsRunning(), "Timer should initially not be running");

        timer.Start();
        ASSERT_TRUE(timer.IsRunning(), "Timer should be running after Start()");

        // Sleep 30 ms
        ::Sleep(30);

        double elapsed_ms = timer.GetElapsedMilliseconds();
        double elapsed_us = timer.GetElapsedMicroseconds();
        ASSERT_TRUE(elapsed_ms >= 25.0 && elapsed_ms <= 60.0, "Elapsed ms should accurately reflect ~30ms sleep");
        ASSERT_TRUE(elapsed_us >= 25000.0, "Elapsed microseconds must be consistent");

        double delta_ms = timer.GetDeltaMilliseconds();
        ASSERT_TRUE(delta_ms >= 25.0, "GetDeltaMilliseconds() returns valid delta");

        timer.Stop();
        ASSERT_TRUE(!timer.IsRunning(), "Timer should be stopped");
    }

    // --- TEST 8: Milestone 2 Task 2.2 MockBurstGenerator ---
    {
        Recoil::BurstGeneratorConfig cfg;
        cfg.shot_count = 30;
        cfg.rpm = 720;
        cfg.seed = 1337; // Deterministic seed

        Recoil::BurstRecording burst = Recoil::MockBurstGenerator::GenerateBurst(cfg, "test_burst_01");

        ASSERT_TRUE(burst.recording_id == "test_burst_01", "Burst recording ID matches");
        ASSERT_TRUE(burst.ShotCount() == 30, "Generated burst must have exactly 30 shots");
        ASSERT_TRUE(!burst.timestamp_iso.empty(), "Timestamp ISO must be set");

        // First shot checks
        ASSERT_TRUE(burst.raw_shots[0].shot_index == 1, "First shot index is 1");
        ASSERT_TRUE(burst.raw_shots[0].timestamp_ms == 0, "First shot timestamp is 0");
        ASSERT_TRUE(burst.raw_shots[0].interval_ms == 0.0f, "First shot interval is 0");
        ASSERT_TRUE(burst.raw_shots[0].source == "simulated", "Source is 'simulated'");

        // Subsequent shots checks
        for (size_t i = 1; i < burst.raw_shots.size(); ++i) {
            ASSERT_TRUE(burst.raw_shots[i].shot_index == static_cast<int>(i + 1), "Shot indices must be strictly sequential");
            ASSERT_TRUE(burst.raw_shots[i].timestamp_ms > burst.raw_shots[i - 1].timestamp_ms, "Timestamps must be strictly monotonic");
            ASSERT_TRUE(burst.raw_shots[i].interval_ms > 0.0f, "Intervals must be positive");
            ASSERT_TRUE(burst.raw_shots[i].source == "simulated", "Source is 'simulated'");
        }

        // Verify cumulative calculation accuracy
        float manual_cum_x = 0.0f;
        float manual_cum_y = 0.0f;
        for (const auto& s : burst.raw_shots) {
            manual_cum_x += s.delta_x;
            manual_cum_y += s.delta_y;
            ASSERT_NEAR(s.cum_x, manual_cum_x, 1e-4f, "Cumulative X matches sum of delta X");
            ASSERT_NEAR(s.cum_y, manual_cum_y, 1e-4f, "Cumulative Y matches sum of delta Y");
        }

        // Average interval check: 720 RPM ideal is ~83.33ms
        float total_time = static_cast<float>(burst.raw_shots.back().timestamp_ms);
        float avg_interval = total_time / 29.0f; // 29 intervals for 30 shots
        ASSERT_NEAR(avg_interval, 83.33f, 3.0f, "Average interval must be close to ~83.3ms for 720 RPM");
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "ALL 8 TEST SUITES PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
