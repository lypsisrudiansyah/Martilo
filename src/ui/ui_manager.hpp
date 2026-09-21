#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"

#include "../core/recoil_types.hpp"
#include "../core/recoil_math.hpp"
#include "../core/recoil_serialization.hpp"
#include "../recorder/burst_recorder.hpp"
#include "../recorder/telemetry_adapter.hpp"
#include "../recorder/recoil_generator.hpp"

#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <cstdio>

namespace Recoil {

class UIManager {
public:
    // State aplikasi yang dikelola
    std::vector<WeaponProfile> profiles;
    size_t activeProfileIdx{0};
    int selectedRecordingIdx{-1};

    // Controller dan telemetry
    BurstRecorder recorder;
    std::unique_ptr<UdpTelemetryAdapter> udpAdapter;
    ManualTelemetryAdapter manualAdapter;
    BurstGeneratorConfig simConfig;

    // Pengaturan UI & Modal
    bool showCreateWeaponModal{false};
    bool showEditWeaponModal{false};
    bool showDeleteConfirmModal{false};
    bool isFirstLayout{true};

    // Buffer form input senjata
    char inputWeaponName[64] = "AK-47 Custom";
    char inputWeaponId[64] = "rifle_ak47";
    int inputRpm = 600;
    int inputMagSize = 30;
    int inputFireModeIdx = 0; // 0: Full-Auto, 1: Burst, 2: Semi-Auto
    char inputNotes[256] = "Standard 7.62mm assault rifle with heavy vertical kick.";

    // Buffer Manual Trigger
    float manualDeltaX = 0.5f;
    float manualDeltaY = -3.0f;
    int udpPort = 9988;

    UIManager() {
        // Setup profil default awal jika kosong
        WeaponProfile defaultWpn;
        defaultWpn.weapon_id = "rifle_m4a1";
        defaultWpn.weapon_name = "M4A1 Carbine";
        defaultWpn.fire_mode = "Full-Auto";
        defaultWpn.fire_rate_rpm = 720;
        defaultWpn.magazine_size = 30;
        defaultWpn.notes = "Standard 5.56mm weapon with predictable spray pattern.";
        defaultWpn.status = ProfileStatus::Tested;
        defaultWpn.profile_version = 1;

        // Tambahkan satu contoh rekaman awal agar chart langsung memiliki visual menarik
        BurstGeneratorConfig cfg;
        cfg.shot_count = 30;
        cfg.rpm = 720;
        cfg.seed = 42;
        BurstRecording sampleBurst = MockBurstGenerator::GenerateBurst(cfg, "sample_burst_01");
        sampleBurst.notes = "Baseline calibration burst";
        defaultWpn.recordings.push_back(sampleBurst);

        profiles.push_back(defaultWpn);
        activeProfileIdx = 0;
        selectedRecordingIdx = 0;

        // Setup telemetry adapter
        manualAdapter.Start();
        recorder.AttachAdapter(&manualAdapter);

        udpAdapter = std::make_unique<UdpTelemetryAdapter>(udpPort);
        recorder.AttachAdapter(udpAdapter.get());
    }

    WeaponProfile* GetActiveWeapon() {
        if (profiles.empty()) return nullptr;
        if (activeProfileIdx >= profiles.size()) activeProfileIdx = 0;
        return &profiles[activeProfileIdx];
    }

    // Terapkan Modern Dark Theme (Task 3.1)
    void ApplyDarkTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 5.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg]             = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
        colors[ImGuiCol_Header]               = ImVec4(0.20f, 0.25f, 0.32f, 1.00f);
        colors[ImGuiCol_HeaderHovered]        = ImVec4(0.28f, 0.35f, 0.45f, 1.00f);
        colors[ImGuiCol_HeaderActive]         = ImVec4(0.35f, 0.44f, 0.56f, 1.00f);
        colors[ImGuiCol_Button]               = ImVec4(0.20f, 0.24f, 0.30f, 1.00f);
        colors[ImGuiCol_ButtonHovered]        = ImVec4(0.28f, 0.34f, 0.44f, 1.00f);
        colors[ImGuiCol_ButtonActive]         = ImVec4(0.36f, 0.45f, 0.58f, 1.00f);
        colors[ImGuiCol_FrameBg]              = ImVec4(0.16f, 0.17f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.23f, 0.27f, 1.00f);
        colors[ImGuiCol_FrameBgActive]        = ImVec4(0.26f, 0.28f, 0.34f, 1.00f);
        colors[ImGuiCol_Tab]                  = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
        colors[ImGuiCol_TabHovered]           = ImVec4(0.28f, 0.35f, 0.45f, 1.00f);
        colors[ImGuiCol_TabActive]            = ImVec4(0.22f, 0.28f, 0.37f, 1.00f);
        colors[ImGuiCol_TabUnfocused]         = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.17f, 0.20f, 0.25f, 1.00f);
        colors[ImGuiCol_TitleBg]              = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_TitleBgActive]        = ImVec4(0.14f, 0.17f, 0.22f, 1.00f);
        colors[ImGuiCol_CheckMark]            = ImVec4(0.38f, 0.72f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrab]           = ImVec4(0.35f, 0.65f, 0.95f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.45f, 0.78f, 1.00f, 1.00f);
        colors[ImGuiCol_DockingPreview]       = ImVec4(0.35f, 0.65f, 0.95f, 0.70f);
    }

    // Main Menu Bar (Task 3.1)
    void RenderMainMenuBar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Weapon Profile...", "Ctrl+N")) {
                    PrepareCreateProfileModal();
                    showCreateWeaponModal = true;
                }
                if (ImGui::MenuItem("Duplicate Active Profile", "Ctrl+D")) {
                    DuplicateActiveProfile();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Reset Layout to Default")) {
                    isFirstLayout = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    ::PostQuitMessage(0);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Weapon")) {
                if (ImGui::MenuItem("Edit Active Profile Metadata...")) {
                    PrepareEditProfileModal();
                    showEditWeaponModal = true;
                }
                if (ImGui::MenuItem("Bump Version")) {
                    WeaponProfile* w = GetActiveWeapon();
                    if (w) w->IncrementVersion();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete Active Profile", nullptr, false, profiles.size() > 1)) {
                    showDeleteConfirmModal = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Telemetry")) {
                bool isListening = udpAdapter && udpAdapter->IsActive();
                if (ImGui::MenuItem(isListening ? "Stop UDP Listener" : "Start UDP Listener")) {
                    if (isListening) udpAdapter->Stop();
                    else udpAdapter->Start();
                }
                if (ImGui::MenuItem("Send Test Packet (127.0.0.1:9988)")) {
                    NetworkTestHelper::SendUdpPacket(udpPort, "0.5,-3.0,0");
                }
                ImGui::EndMenu();
            }

            // Right side status text
            WeaponProfile* activeWpn = GetActiveWeapon();
            if (activeWpn) {
                float menuWidth = ImGui::GetWindowWidth();
                char statusText[128];
                snprintf(statusText, sizeof(statusText), "Active: %s (v%d) [%s]", 
                    activeWpn->weapon_name.c_str(), activeWpn->profile_version, ProfileStatusToString(activeWpn->status).c_str());
                float textWidth = ImGui::CalcTextSize(statusText).x;
                ImGui::SameLine(menuWidth - textWidth - 20.0f);
                ImGui::TextDisabled("%s", statusText);
            }

            ImGui::EndMainMenuBar();
        }
    }

    // Task 3.1: Setup DockSpace and default multi-panel layout
    void SetupDockSpace() {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiID dockspace_id = ImGui::GetID("RecoilRecorderDockspace");

        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        if (isFirstLayout) {
            isFirstLayout = false;

            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.28f, nullptr, &dock_main_id);
            ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.30f, nullptr, &dock_main_id);
            ImGuiID dock_bottom_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.35f, nullptr, &dock_main_id);

            // Dock windows to respective zones
            ImGui::DockBuilderDockWindow("Weapon Profiles", dock_left_id);
            ImGui::DockBuilderDockWindow("Burst Recording Controller", dock_left_id);

            ImGui::DockBuilderDockWindow("Recoil Trajectory (2D)", dock_main_id);

            ImGui::DockBuilderDockWindow("Shot Data Inspector", dock_bottom_id);

            ImGui::DockBuilderDockWindow("Live Telemetry & Input", dock_right_id);
            ImGui::DockBuilderDockWindow("Burst Simulator", dock_right_id);

            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    // Task 3.2: Panel Weapon Profile Manager
    void RenderWeaponProfileManager() {
        ImGui::Begin("Weapon Profiles");

        WeaponProfile* active = GetActiveWeapon();

        // 3.2.2 Dropdown pemilihan profile aktif & status badge
        ImGui::Text("Active Weapon:");
        if (ImGui::BeginCombo("##SelectWeapon", active ? active->weapon_name.c_str() : "None")) {
            for (size_t i = 0; i < profiles.size(); ++i) {
                bool isSelected = (i == activeProfileIdx);
                std::string itemLabel = profiles[i].weapon_name + " (" + profiles[i].weapon_id + ")";
                if (ImGui::Selectable(itemLabel.c_str(), isSelected)) {
                    activeProfileIdx = i;
                    selectedRecordingIdx = profiles[i].recordings.empty() ? -1 : 0;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Status Badge & Version
        if (active) {
            ImGui::SameLine();
            RenderStatusBadge(active->status);

            ImGui::TextDisabled("ID: %s | Version: v%d | Fire Mode: %s", 
                active->weapon_id.c_str(), active->profile_version, active->fire_mode.c_str());
            ImGui::TextDisabled("RPM: %d | Mag Size: %d | Recordings: %d", 
                active->fire_rate_rpm, active->magazine_size, (int)active->recordings.size());
        }

        ImGui::Separator();

        // 3.2.3 Tombol aksi profile
        if (ImGui::Button("+ New Profile", ImVec2(105, 26))) {
            PrepareCreateProfileModal();
            showCreateWeaponModal = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate", ImVec2(80, 26))) {
            DuplicateActiveProfile();
        }
        ImGui::SameLine();
        if (ImGui::Button("Edit Metadata", ImVec2(100, 26))) {
            PrepareEditProfileModal();
            showEditWeaponModal = true;
        }

        // Status selector dropdown
        if (active) {
            ImGui::Spacing();
            ImGui::Text("Profile Status:");
            int currentStatusIdx = static_cast<int>(active->status);
            const char* statusOptions[] = { "Draft", "Tested", "Approved" };
            if (ImGui::Combo("##StatusCombo", &currentStatusIdx, statusOptions, IM_ARRAYSIZE(statusOptions))) {
                active->status = static_cast<ProfileStatus>(currentStatusIdx);
            }
        }

        // Metadata Notes section
        if (active && !active->notes.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Notes: %s", active->notes.c_str());
        }

        // Delete Profile Button
        if (profiles.size() > 1) {
            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Delete Profile...", ImVec2(-1, 24))) {
                showDeleteConfirmModal = true;
            }
        }

        ImGui::End();
    }

    // Task 3.3: Panel Recording Controller
    void RenderRecordingController() {
        ImGui::Begin("Burst Recording Controller");

        WeaponProfile* active = GetActiveWeapon();
        if (!active) {
            ImGui::Text("No active weapon profile.");
            ImGui::End();
            return;
        }

        // 3.3.1 Tombol Record Burst (Start / Stop) dengan indikator visual
        RecorderState state = recorder.GetState();
        if (state == RecorderState::Recording) {
            // Tombol Merah Berkedip saat merekam
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.25f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.10f, 0.10f, 1.0f));
            
            char btnText[128];
            snprintf(btnText, sizeof(btnText), "STOP RECORDING (%d SHOTS)", recorder.GetLiveShotCount());
            if (ImGui::Button(btnText, ImVec2(-1, 38))) {
                recorder.StopRecording();
            }
            ImGui::PopStyleColor(3);

            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "LIVE: Recording in progress... (%.1f ms)", recorder.GetElapsedMs());
        } else {
            // Tombol Hijau Siap merekam
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.60f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.72f, 0.32f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.14f, 0.50f, 0.20f, 1.0f));
            if (ImGui::Button("START BURST RECORDING", ImVec2(-1, 38))) {
                std::string burstId = "burst_" + std::to_string(active->recordings.size() + 1);
                recorder.StartRecording(burstId, "telemetry");
            }
            ImGui::PopStyleColor(3);

            ImGui::TextDisabled("Status: Standby. Ready for next burst trigger.");
        }

        ImGui::Separator();

        // 3.3.2 List daftar recording yang sudah tersimpan untuk senjata aktif
        ImGui::Text("Stored Recordings (%d):", (int)active->recordings.size());

        // Quick Bulk Actions (Show All / Hide All)
        if (!active->recordings.empty()) {
            if (ImGui::SmallButton("Show All")) {
                for (auto& r : active->recordings) r.is_visible = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Hide All")) {
                for (auto& r : active->recordings) r.is_visible = false;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear Recordings")) {
                active->recordings.clear();
                selectedRecordingIdx = -1;
            }
        }

        ImGui::BeginChild("RecordingsListRegion", ImVec2(0, 0), true);
        if (active->recordings.empty()) {
            ImGui::TextDisabled("No bursts recorded yet.\nClick Start Recording or Simulate Burst.");
        } else {
            for (size_t i = 0; i < active->recordings.size(); ++i) {
                auto& rec = active->recordings[i];
                ImGui::PushID(static_cast<int>(i));

                // 3.3.3 Checkbox visibilitas per recording untuk overlay chart
                ImGui::Checkbox("##visible", &rec.is_visible);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Toggle visibility on trajectory chart overlay");
                }
                ImGui::SameLine();

                // Selectable burst item
                char itemText[128];
                snprintf(itemText, sizeof(itemText), "%s [%d shots, %lld ms]", 
                    rec.recording_id.c_str(), (int)rec.ShotCount(), (long long)rec.TotalDurationMs());

                bool isSelected = (selectedRecordingIdx == static_cast<int>(i));
                if (ImGui::Selectable(itemText, isSelected, ImGuiSelectableFlags_None, ImVec2(ImGui::GetContentRegionAvail().x - 30, 0))) {
                    selectedRecordingIdx = static_cast<int>(i);
                }

                // Delete button
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    active->recordings.erase(active->recordings.begin() + i);
                    if (selectedRecordingIdx >= static_cast<int>(active->recordings.size())) {
                        selectedRecordingIdx = static_cast<int>(active->recordings.size() - 1);
                    }
                    ImGui::PopID();
                    break;
                }

                if (isSelected && !rec.notes.empty()) {
                    ImGui::Indent(28.0f);
                    ImGui::TextDisabled("Notes: %s", rec.notes.c_str());
                    ImGui::Unindent(28.0f);
                }

                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }

    // Modal Form Pembuatan Profil Baru (Task 3.2.1)
    void RenderModals() {
        // Modal: Create New Profile
        if (showCreateWeaponModal) {
            ImGui::OpenPopup("Create Weapon Profile");
        }

        if (ImGui::BeginPopupModal("Create Weapon Profile", &showCreateWeaponModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter weapon metadata for the new profile:");
            ImGui::Separator();

            ImGui::InputText("Weapon Name", inputWeaponName, sizeof(inputWeaponName));
            ImGui::InputText("Weapon ID", inputWeaponId, sizeof(inputWeaponId));
            ImGui::SliderInt("Fire Rate (RPM)", &inputRpm, 100, 1500);
            ImGui::SliderInt("Magazine Size", &inputMagSize, 1, 150);

            const char* modes[] = { "Full-Auto", "Burst-3", "Semi-Auto" };
            ImGui::Combo("Fire Mode", &inputFireModeIdx, modes, IM_ARRAYSIZE(modes));

            ImGui::InputTextMultiline("Notes", inputNotes, sizeof(inputNotes), ImVec2(320, 60));

            ImGui::Separator();
            if (ImGui::Button("Create", ImVec2(120, 28))) {
                WeaponProfile newWpn;
                newWpn.weapon_name = inputWeaponName;
                newWpn.weapon_id = inputWeaponId;
                newWpn.fire_rate_rpm = inputRpm;
                newWpn.magazine_size = inputMagSize;
                newWpn.fire_mode = modes[inputFireModeIdx];
                newWpn.notes = inputNotes;
                newWpn.status = ProfileStatus::Draft;
                newWpn.profile_version = 1;

                profiles.push_back(newWpn);
                activeProfileIdx = profiles.size() - 1;
                selectedRecordingIdx = -1;

                showCreateWeaponModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 28))) {
                showCreateWeaponModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Modal: Edit Active Profile Metadata (Task 3.2.3)
        if (showEditWeaponModal) {
            ImGui::OpenPopup("Edit Weapon Metadata");
        }

        if (ImGui::BeginPopupModal("Edit Weapon Metadata", &showEditWeaponModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            WeaponProfile* active = GetActiveWeapon();
            if (active) {
                ImGui::Text("Editing: %s (v%d)", active->weapon_name.c_str(), active->profile_version);
                ImGui::Separator();

                ImGui::InputText("Weapon Name", inputWeaponName, sizeof(inputWeaponName));
                ImGui::SliderInt("Fire Rate (RPM)", &inputRpm, 100, 1500);
                ImGui::SliderInt("Magazine Size", &inputMagSize, 1, 150);

                const char* modes[] = { "Full-Auto", "Burst-3", "Semi-Auto" };
                ImGui::Combo("Fire Mode", &inputFireModeIdx, modes, IM_ARRAYSIZE(modes));

                ImGui::InputTextMultiline("Notes", inputNotes, sizeof(inputNotes), ImVec2(320, 60));

                ImGui::Separator();
                if (ImGui::Button("Save Changes", ImVec2(140, 28))) {
                    active->weapon_name = inputWeaponName;
                    active->fire_rate_rpm = inputRpm;
                    active->magazine_size = inputMagSize;
                    active->fire_mode = modes[inputFireModeIdx];
                    active->notes = inputNotes;
                    active->IncrementVersion();

                    showEditWeaponModal = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(100, 28))) {
                    showEditWeaponModal = false;
                    ImGui::CloseCurrentPopup();
                }
            } else {
                showEditWeaponModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Modal: Delete Profile Confirmation (Task 3.2.3)
        if (showDeleteConfirmModal) {
            ImGui::OpenPopup("Delete Weapon Profile?");
        }

        if (ImGui::BeginPopupModal("Delete Weapon Profile?", &showDeleteConfirmModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            WeaponProfile* active = GetActiveWeapon();
            if (active) {
                ImGui::Text("Are you sure you want to delete profile:\n'%s' (%s)?", 
                    active->weapon_name.c_str(), active->weapon_id.c_str());
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "This will delete all associated recordings for this weapon.");
                ImGui::Separator();

                if (ImGui::Button("Yes, Delete", ImVec2(120, 28))) {
                    profiles.erase(profiles.begin() + activeProfileIdx);
                    if (activeProfileIdx >= profiles.size()) {
                        activeProfileIdx = profiles.size() - 1;
                    }
                    selectedRecordingIdx = -1;
                    showDeleteConfirmModal = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 28))) {
                    showDeleteConfirmModal = false;
                    ImGui::CloseCurrentPopup();
                }
            } else {
                showDeleteConfirmModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // Helper: Duplicate Profile (Task 3.2.3)
    void DuplicateActiveProfile() {
        WeaponProfile* active = GetActiveWeapon();
        if (!active) return;

        WeaponProfile copyWpn = *active;
        copyWpn.weapon_name += " (Copy)";
        copyWpn.weapon_id += "_copy";
        copyWpn.profile_version = 1;
        copyWpn.status = ProfileStatus::Draft;

        profiles.push_back(copyWpn);
        activeProfileIdx = profiles.size() - 1;
        selectedRecordingIdx = copyWpn.recordings.empty() ? -1 : 0;
    }

    // Helper: Render Status Badge dengan warna konsisten
    void RenderStatusBadge(ProfileStatus status) {
        ImVec4 color;
        const char* label = "Draft";
        switch (status) {
            case ProfileStatus::Draft:
                color = ImVec4(0.9f, 0.7f, 0.1f, 1.0f); // Kuning / Amber
                label = "[Draft]";
                break;
            case ProfileStatus::Tested:
                color = ImVec4(0.2f, 0.7f, 0.9f, 1.0f); // Cyan
                label = "[Tested]";
                break;
            case ProfileStatus::Approved:
                color = ImVec4(0.2f, 0.9f, 0.3f, 1.0f); // Hijau
                label = "[Approved]";
                break;
        }
        ImGui::TextColored(color, "%s", label);
    }

    void PrepareCreateProfileModal() {
        snprintf(inputWeaponName, sizeof(inputWeaponName), "New Weapon #%d", (int)profiles.size() + 1);
        snprintf(inputWeaponId, sizeof(inputWeaponId), "weapon_%03d", (int)profiles.size() + 1);
        inputRpm = 650;
        inputMagSize = 30;
        inputFireModeIdx = 0;
        inputNotes[0] = '\0';
    }

    void PrepareEditProfileModal() {
        WeaponProfile* active = GetActiveWeapon();
        if (!active) return;

        snprintf(inputWeaponName, sizeof(inputWeaponName), "%s", active->weapon_name.c_str());
        snprintf(inputWeaponId, sizeof(inputWeaponId), "%s", active->weapon_id.c_str());
        inputRpm = active->fire_rate_rpm;
        inputMagSize = active->magazine_size;
        inputFireModeIdx = (active->fire_mode == "Semi-Auto") ? 2 : ((active->fire_mode == "Burst-3") ? 1 : 0);
        snprintf(inputNotes, sizeof(inputNotes), "%s", active->notes.c_str());
    }

    // Render Trajectory 2D Plot dengan Multi-Recording Overlay (Task 3.3.3)
    void RenderTrajectoryChart() {
        ImGui::Begin("Recoil Trajectory (2D)");
        WeaponProfile* active = GetActiveWeapon();
        if (!active || active->recordings.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No burst recordings available. Click 'START BURST RECORDING' or 'Simulate Burst'.");
            ImGui::End();
            return;
        }

        int visibleCount = 0;
        for (const auto& rec : active->recordings) {
            if (rec.is_visible) visibleCount++;
        }

        ImGui::Text("Active Weapon: %s | Total Recordings: %d | Visible Overlays: %d",
            active->weapon_name.c_str(), (int)active->recordings.size(), visibleCount);

        if (ImPlot::BeginPlot("Trajectory (Cumulative X / -Y)", ImVec2(-1, -1))) {
            ImPlot::SetupAxes("Cumulative Horizontal Drift (X)", "Cumulative Upward Climb (-Y)");

            for (size_t r = 0; r < active->recordings.size(); ++r) {
                const auto& rec = active->recordings[r];
                // Task 3.3.3: Hormati flag is_visible
                if (!rec.is_visible || rec.raw_shots.empty()) continue;

                std::vector<float> xs(rec.raw_shots.size());
                std::vector<float> ys(rec.raw_shots.size());
                for (size_t s = 0; s < rec.raw_shots.size(); ++s) {
                    xs[s] = rec.raw_shots[s].cum_x;
                    ys[s] = -rec.raw_shots[s].cum_y; // Invert Y agar moncong naik ke atas
                }

                std::string lineLabel = rec.recording_id + " (Line)";
                std::string scatterLabel = rec.recording_id;

                ImPlot::PlotLine(lineLabel.c_str(), xs.data(), ys.data(), (int)xs.size());
                ImPlot::PlotScatter(scatterLabel.c_str(), xs.data(), ys.data(), (int)xs.size());
            }

            ImPlot::EndPlot();
        }

        ImGui::End();
    }

    // Render Shot Data Table Inspector
    void RenderShotTable() {
        ImGui::Begin("Shot Data Inspector");
        WeaponProfile* active = GetActiveWeapon();
        if (active && selectedRecordingIdx >= 0 && selectedRecordingIdx < static_cast<int>(active->recordings.size())) {
            const auto& rec = active->recordings[selectedRecordingIdx];
            ImGui::Text("Inspecting: %s (%d shots, Total Drift: X=%.2f, Y=%.2f)",
                rec.recording_id.c_str(), (int)rec.ShotCount(), 
                rec.raw_shots.empty() ? 0.0f : rec.raw_shots.back().cum_x,
                rec.raw_shots.empty() ? 0.0f : rec.raw_shots.back().cum_y);

            if (ImGui::BeginTable("InspectorTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Shot #", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Interval (ms)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Delta X", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableSetupColumn("Delta Y", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableSetupColumn("Cum X", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableSetupColumn("Cum Y", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                for (const auto& shot : rec.raw_shots) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%d", shot.shot_index);
                    ImGui::TableNextColumn(); ImGui::Text("%lld", (long long)shot.timestamp_ms);
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", shot.interval_ms);
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", shot.delta_x);
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", shot.delta_y);
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", shot.cum_x);
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", shot.cum_y);
                    ImGui::TableNextColumn(); ImGui::Text("%s", shot.source.c_str());
                }
                ImGui::EndTable();
            }
        } else {
            ImGui::TextDisabled("Select a burst recording in the sidebar to inspect per-shot data.");
        }
        ImGui::End();
    }

    // Render Telemetry & Burst Simulator Panels
    void RenderTelemetryAndSimulator() {
        ImGui::Begin("Live Telemetry & Input");
        // UDP controls
        ImGui::Text("UDP Telemetry Receiver (Task 2.3.2)");
        ImGui::Text("Endpoint: 127.0.0.1:%d", udpPort);
        if (udpAdapter && udpAdapter->IsActive()) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Status: [ LISTENING ]");
            if (ImGui::Button("Stop UDP Listener", ImVec2(-1, 26))) {
                udpAdapter->Stop();
            }
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Status: [ STOPPED ]");
            if (ImGui::Button("Start UDP Listener", ImVec2(-1, 26))) {
                if (udpAdapter) udpAdapter->Start();
            }
        }

        if (ImGui::Button("Send Test UDP Shot", ImVec2(-1, 26))) {
            NetworkTestHelper::SendUdpPacket(udpPort, "0.65,-2.9,0");
        }

        ImGui::Separator();
        // Manual input
        ImGui::Text("Manual Input Trigger (Task 2.3.3)");
        ImGui::SliderFloat("ΔX", &manualDeltaX, -5.0f, 5.0f, "%.2f");
        ImGui::SliderFloat("ΔY", &manualDeltaY, -10.0f, 0.0f, "%.2f");
        if (ImGui::Button("Trigger Manual Shot", ImVec2(-1, 28))) {
            manualAdapter.TriggerShot(manualDeltaX, manualDeltaY);
        }

        ImGui::Separator();
        // Auto finalize
        bool autoFin = recorder.IsAutoFinalizeEnabled();
        if (ImGui::Checkbox("Auto-Finalize on Silence", &autoFin)) {
            recorder.SetAutoFinalize(autoFin, recorder.GetAutoFinalizeTimeoutMs());
        }
        int toMs = static_cast<int>(recorder.GetAutoFinalizeTimeoutMs());
        if (ImGui::SliderInt("Timeout (ms)", &toMs, 100, 2000)) {
            recorder.SetAutoFinalize(autoFin, toMs);
        }

        ImGui::End();

        // Simulator panel
        ImGui::Begin("Burst Simulator");
        ImGui::Text("Mock Recoil Generator (Task 2.2)");
        ImGui::Separator();
        ImGui::SliderInt("Shots", &simConfig.shot_count, 5, 60);
        ImGui::SliderInt("RPM", &simConfig.rpm, 300, 1200);
        ImGui::SliderFloat("Vertical Kick", &simConfig.base_vertical_recoil, -8.0f, -0.5f, "%.2f");
        ImGui::SliderFloat("Horizontal Drift", &simConfig.horizontal_drift, -2.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Random Spread", &simConfig.random_spread, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Timing Jitter", &simConfig.timing_jitter_ms, 0.0f, 5.0f, "%.2f");

        if (ImGui::Button("Simulate Burst", ImVec2(-1, 32))) {
            WeaponProfile* active = GetActiveWeapon();
            if (active) {
                BurstRecording burst = MockBurstGenerator::GenerateBurst(simConfig);
                active->recordings.push_back(burst);
                selectedRecordingIdx = static_cast<int>(active->recordings.size() - 1);
            }
        }
        ImGui::End();
    }

    // Tick recorder & update state
    void Update() {
        recorder.Update();
        if (recorder.IsFinished()) {
            BurstRecording finishedBurst = recorder.FinalizeBurst();
            if (!finishedBurst.raw_shots.empty()) {
                WeaponProfile* active = GetActiveWeapon();
                if (active) {
                    active->recordings.push_back(finishedBurst);
                    selectedRecordingIdx = static_cast<int>(active->recordings.size() - 1);
                }
            }
        }
    }
};

} // namespace Recoil
