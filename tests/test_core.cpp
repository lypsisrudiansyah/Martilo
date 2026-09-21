#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include "../src/core/recoil_types.hpp"
#include "../src/core/recoil_math.hpp"

#include "../src/core/recoil_serialization.hpp"
#include "../src/recorder/precision_timer.hpp"
#include "../src/recorder/recoil_generator.hpp"
#include "../src/recorder/telemetry_adapter.hpp"
#include "../src/recorder/burst_recorder.hpp"

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

    // --- TEST 9: Milestone 2 Task 2.4.1 BurstRecorder State Machine Lifecycle ---
    {
        Recoil::BurstRecorder recorder;
        ASSERT_TRUE(recorder.IsIdle(), "Initial recorder state must be IDLE");
        ASSERT_TRUE(recorder.GetState() == Recoil::RecorderState::Idle, "Recorder state enum is Idle");

        recorder.StartRecording("test_burst_sm", "telemetry_raw");
        ASSERT_TRUE(recorder.IsRecording(), "Recorder state must be RECORDING after StartRecording()");
        ASSERT_TRUE(recorder.GetLiveShotCount() == 0, "Initial live shot count is 0");

        recorder.StopRecording();
        ASSERT_TRUE(recorder.IsFinished(), "Recorder state must be BURST_FINISHED after StopRecording()");

        Recoil::BurstRecording burst = recorder.FinalizeBurst();
        ASSERT_TRUE(recorder.IsIdle(), "Recorder state resets to IDLE after FinalizeBurst()");
        ASSERT_TRUE(burst.recording_id == "test_burst_sm", "Burst ID preserved in finalized recording");
    }

    // --- TEST 10: Milestone 2 Task 2.4.3 Live Shot Counter & Ingestion ---
    {
        Recoil::BurstRecorder recorder;
        recorder.StartRecording("burst_counter_test", "raw");

        recorder.OnShotReceived(1.0f, -2.5f, 0);
        ASSERT_TRUE(recorder.GetLiveShotCount() == 1, "Live shot count is 1 after first shot");

        recorder.OnShotReceived(-0.5f, -3.0f, 83);
        ASSERT_TRUE(recorder.GetLiveShotCount() == 2, "Live shot count is 2 after second shot");

        recorder.OnShotReceived(0.2f, -3.2f, 166);
        ASSERT_TRUE(recorder.GetLiveShotCount() == 3, "Live shot count is 3 after third shot");

        auto liveShots = recorder.GetLiveShotsCopy();
        ASSERT_TRUE(liveShots.size() == 3, "Live shots copy has 3 elements");
        ASSERT_NEAR(liveShots[0].cum_x, 1.0f, 1e-4f, "Shot 1 Cum X is 1.0");
        ASSERT_NEAR(liveShots[1].cum_x, 0.5f, 1e-4f, "Shot 2 Cum X is 0.5");
        ASSERT_NEAR(liveShots[2].cum_x, 0.7f, 1e-4f, "Shot 3 Cum X is 0.7");

        ASSERT_NEAR(liveShots[0].cum_y, -2.5f, 1e-4f, "Shot 1 Cum Y is -2.5");
        ASSERT_NEAR(liveShots[1].cum_y, -5.5f, 1e-4f, "Shot 2 Cum Y is -5.5");
        ASSERT_NEAR(liveShots[2].cum_y, -8.7f, 1e-4f, "Shot 3 Cum Y is -8.7");

        Recoil::BurstRecording burst = recorder.FinalizeBurst();
        ASSERT_TRUE(burst.ShotCount() == 3, "Finalized burst contains 3 shots");
    }

    // --- TEST 11: Milestone 2 Task 2.4.2 Auto-Finalize Timeout Logic ---
    {
        Recoil::BurstRecorder recorder;
        // Set timeout to 40ms for fast unit testing
        recorder.SetAutoFinalize(true, 40);
        ASSERT_TRUE(recorder.IsAutoFinalizeEnabled(), "Auto-finalize must be enabled");
        ASSERT_TRUE(recorder.GetAutoFinalizeTimeoutMs() == 40, "Timeout setting matches 40ms");

        recorder.StartRecording("auto_timeout_burst", "test");
        recorder.OnShotReceived(0.5f, -2.0f); // shot with internal timer

        // Immediate update: should still be recording
        recorder.Update();
        ASSERT_TRUE(recorder.IsRecording(), "Should still be recording immediately after shot");

        // Sleep 60ms (> 40ms timeout)
        ::Sleep(60);

        recorder.Update();
        ASSERT_TRUE(recorder.IsFinished(), "Should auto-finalize to BURST_FINISHED after timeout expires");

        Recoil::BurstRecording burst = recorder.FinalizeBurst();
        ASSERT_TRUE(burst.ShotCount() == 1, "Auto-finalized burst has 1 shot");
    }

    // --- TEST 12: Milestone 2 Task 2.3 Telemetry Adapters (Manual & UDP Socket) ---
    {
        // 1. Manual Telemetry Adapter
        Recoil::BurstRecorder recorder;
        Recoil::ManualTelemetryAdapter manualAdapter;
        recorder.AttachAdapter(&manualAdapter);

        manualAdapter.Start();
        ASSERT_TRUE(manualAdapter.IsActive(), "Manual adapter is active");

        recorder.StartRecording("manual_adapter_burst");
        manualAdapter.TriggerShot(1.5f, -3.0f, 0);
        manualAdapter.TriggerShot(0.5f, -2.5f, 83);
        ASSERT_TRUE(recorder.GetLiveShotCount() == 2, "Manual adapter properly dispatched 2 shots");
        recorder.FinalizeBurst();

        // 2. UDP Telemetry Adapter (127.0.0.1:9977)
        int testPort = 9977;
        Recoil::UdpTelemetryAdapter udpAdapter(testPort);
        recorder.AttachAdapter(&udpAdapter);

        bool udpStarted = udpAdapter.Start();
        ASSERT_TRUE(udpStarted, "UDP Adapter successfully bound and started on port 9977");
        ASSERT_TRUE(udpAdapter.IsActive(), "UDP Adapter is active");

        recorder.StartRecording("udp_adapter_burst");

        // Send CSV packet: "0.8,-3.5,0"
        bool sentCsv = Recoil::NetworkTestHelper::SendUdpPacket(testPort, "0.8,-3.5,0");
        ASSERT_TRUE(sentCsv, "Send CSV packet over UDP loopback succeeded");

        // Brief sleep to allow background worker to recv and dispatch
        ::Sleep(40);

        // Send JSON packet: {"dx": 1.1, "dy": -3.8, "timeMs": 85}
        bool sentJson = Recoil::NetworkTestHelper::SendUdpPacket(testPort, R"({"dx": 1.1, "dy": -3.8, "timeMs": 85})");
        ASSERT_TRUE(sentJson, "Send JSON packet over UDP loopback succeeded");

        ::Sleep(40);

        ASSERT_TRUE(recorder.GetLiveShotCount() == 2, "UDP adapter received and parsed both CSV and JSON packets");

        auto udpShots = recorder.GetLiveShotsCopy();
        ASSERT_TRUE(udpShots.size() == 2, "Recorder holds 2 UDP shots");
        ASSERT_NEAR(udpShots[0].delta_x, 0.8f, 1e-3f, "Shot 1 DX is 0.8");
        ASSERT_NEAR(udpShots[0].delta_y, -3.5f, 1e-3f, "Shot 1 DY is -3.5");
        ASSERT_NEAR(udpShots[1].delta_x, 1.1f, 1e-3f, "Shot 2 DX is 1.1");
        ASSERT_NEAR(udpShots[1].delta_y, -3.8f, 1e-3f, "Shot 2 DY is -3.8");

        udpAdapter.Stop();
        ASSERT_TRUE(!udpAdapter.IsActive(), "UDP adapter successfully stopped");
    }

    // --- TEST 13: Milestone 3 Task 3.2 Weapon Profile Management (Create, Duplicate, Status) ---
    {
        std::vector<Recoil::WeaponProfile> profileList;

        // 3.2.1 Create Profile
        Recoil::WeaponProfile p1;
        p1.weapon_id = "rifle_m4a1";
        p1.weapon_name = "M4A1 Carbine";
        p1.fire_rate_rpm = 720;
        p1.magazine_size = 30;
        p1.fire_mode = "Full-Auto";
        p1.status = Recoil::ProfileStatus::Draft;
        p1.profile_version = 1;
        profileList.push_back(p1);

        ASSERT_TRUE(profileList.size() == 1, "Profile list has 1 item");
        ASSERT_TRUE(profileList[0].weapon_id == "rifle_m4a1", "Weapon ID matches");
        ASSERT_TRUE(profileList[0].status == Recoil::ProfileStatus::Draft, "Initial status is Draft");

        // 3.2.2 Status Transitions
        profileList[0].status = Recoil::ProfileStatus::Tested;
        ASSERT_TRUE(Recoil::ProfileStatusToString(profileList[0].status) == "Tested", "Status transitioned to Tested");
        profileList[0].status = Recoil::ProfileStatus::Approved;
        ASSERT_TRUE(Recoil::ProfileStatusToString(profileList[0].status) == "Approved", "Status transitioned to Approved");

        // 3.2.3 Duplicate Profile
        Recoil::WeaponProfile copy = profileList[0];
        copy.weapon_name += " (Copy)";
        copy.weapon_id += "_copy";
        copy.status = Recoil::ProfileStatus::Draft;
        copy.profile_version = 1;
        profileList.push_back(copy);

        ASSERT_TRUE(profileList.size() == 2, "Profile list now has 2 items after duplicate");
        ASSERT_TRUE(profileList[1].weapon_name == "M4A1 Carbine (Copy)", "Duplicated weapon name matches");
        ASSERT_TRUE(profileList[1].status == Recoil::ProfileStatus::Draft, "Duplicated weapon status resets to Draft");

        // Delete Profile
        profileList.erase(profileList.begin());
        ASSERT_TRUE(profileList.size() == 1, "Profile list has 1 item after deletion");
        ASSERT_TRUE(profileList[0].weapon_id == "rifle_m4a1_copy", "Remaining profile is the duplicate");
    }

    // --- TEST 14: Milestone 3 Task 3.3 Recording Controller & Multi-Recording Visibility Flag ---
    {
        Recoil::WeaponProfile wpn;
        wpn.weapon_id = "test_smg";

        // Generate 3 simulated recordings
        for (int i = 1; i <= 3; ++i) {
            Recoil::BurstGeneratorConfig cfg;
            cfg.shot_count = 10;
            cfg.rpm = 600;
            cfg.seed = 100 + i;
            Recoil::BurstRecording rec = Recoil::MockBurstGenerator::GenerateBurst(cfg, "burst_" + std::to_string(i));
            ASSERT_TRUE(rec.is_visible, "New recording must be visible by default (is_visible == true)");
            wpn.recordings.push_back(rec);
        }

        ASSERT_TRUE(wpn.recordings.size() == 3, "Weapon profile holds 3 recordings");

        // 3.3.3 Checkbox visibilitas per recording untuk overlay
        wpn.recordings[1].is_visible = false; // Hide recording #2

        int visibleCount = 0;
        for (const auto& r : wpn.recordings) {
            if (r.is_visible) visibleCount++;
        }
        ASSERT_TRUE(visibleCount == 2, "Exactly 2 recordings are visible after toggling off burst #2");
        ASSERT_TRUE(!wpn.recordings[1].is_visible, "Burst #2 is_visible flag is false");
        ASSERT_TRUE(wpn.recordings[0].is_visible, "Burst #1 is_visible flag is true");
        ASSERT_TRUE(wpn.recordings[2].is_visible, "Burst #3 is_visible flag is true");
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "ALL 14 TEST SUITES PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
