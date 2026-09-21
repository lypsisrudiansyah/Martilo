#include <windows.h>
#include <d3d11.h>
#include <tchar.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "implot.h"

#include "core/recoil_types.hpp"
#include "core/recoil_math.hpp"
#include "core/recoil_serialization.hpp"
#include "recorder/precision_timer.hpp"
#include "recorder/recoil_generator.hpp"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Global DirectX 11 Variables
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Application State (Recoil Pattern Recorder)
static Recoil::WeaponProfile       g_activeWeapon;
static Recoil::BurstGeneratorConfig g_simConfig;
static Recoil::PrecisionTimer      g_testTimer;
static int                         g_selectedRecordingIdx = -1;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Register window class
    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_CLASSDC,
        WndProc,
        0L,
        0L,
        GetModuleHandle(nullptr),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"RecoilRecorderWindowClass",
        nullptr
    };
    ::RegisterClassExW(&wc);

    // Create application window
    HWND hwnd = ::CreateWindowW(
        wc.lpszClassName,
        L"Recoil Pattern Recorder & Analyzer",
        WS_OVERLAPPEDWINDOW,
        100, 100, 1280, 800,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup style
    ImGui::StyleColorsDark();
    ImPlot::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Main application loop
    bool bRunning = true;
    while (bRunning)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                bRunning = false;
        }
        if (!bRunning)
            break;

        // Handle window resize
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Enable DockSpace
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // Panel: Precision Timer Status (Task 2.1)
        {
            ImGui::Begin("Precision Timer (Task 2.1)");
            ImGui::Text("Windows QPC Hardware Frequency: %lld Hz", (long long)g_testTimer.GetFrequencyHz());
            ImGui::Separator();
            
            bool isRunning = g_testTimer.IsRunning();
            if (!isRunning) {
                if (ImGui::Button("Start Timer", ImVec2(120, 0))) {
                    g_testTimer.Start();
                }
            } else {
                if (ImGui::Button("Stop Timer", ImVec2(120, 0))) {
                    g_testTimer.Stop();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset Timer", ImVec2(120, 0))) {
                g_testTimer.Reset();
            }

            ImGui::Separator();
            double elapsedMs = g_testTimer.GetElapsedMilliseconds();
            double elapsedUs = g_testTimer.GetElapsedMicroseconds();
            ImGui::Text("Elapsed Time (ms): %.4f ms", elapsedMs);
            ImGui::Text("Elapsed Time (us): %.1f us", elapsedUs);
            ImGui::Text("Resolution: < 1 microsecond (Sub-millisecond guaranteed)");
            ImGui::End();
        }

        // Panel: Burst Simulator Controller (Task 2.2)
        {
            ImGui::Begin("Burst Simulator (Task 2.2)");
            ImGui::Text("Mock Recoil Generator for Development & Testing");
            ImGui::Separator();

            ImGui::SliderInt("Shot Count", &g_simConfig.shot_count, 5, 60);
            ImGui::SliderInt("Rate of Fire (RPM)", &g_simConfig.rpm, 300, 1200);
            float stepMs = Recoil::Math::RpmToIntervalMs(g_simConfig.rpm);
            ImGui::TextDisabled("Theoretical Interval: %.2f ms/shot", stepMs);

            ImGui::SliderFloat("Vertical Kick", &g_simConfig.base_vertical_recoil, -8.0f, -0.5f, "%.2f px");
            ImGui::SliderFloat("Horizontal Drift", &g_simConfig.horizontal_drift, -2.0f, 2.0f, "%.2f px");
            ImGui::SliderFloat("Random Spread", &g_simConfig.random_spread, 0.0f, 2.0f, "%.2f px");
            ImGui::SliderFloat("Timing Jitter", &g_simConfig.timing_jitter_ms, 0.0f, 5.0f, "%.2f ms");

            ImGui::Separator();
            if (ImGui::Button("Simulate Burst", ImVec2(160, 32))) {
                Recoil::BurstRecording burst = Recoil::MockBurstGenerator::GenerateBurst(g_simConfig);
                g_activeWeapon.recordings.push_back(burst);
                g_selectedRecordingIdx = static_cast<int>(g_activeWeapon.recordings.size() - 1);
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear All", ImVec2(100, 32))) {
                g_activeWeapon.recordings.clear();
                g_selectedRecordingIdx = -1;
            }

            ImGui::Separator();
            ImGui::Text("Recordings Stored: %d", (int)g_activeWeapon.recordings.size());
            for (size_t i = 0; i < g_activeWeapon.recordings.size(); ++i) {
                const auto& rec = g_activeWeapon.recordings[i];
                char label[128];
                snprintf(label, sizeof(label), "%s (%d shots)##%d", rec.recording_id.c_str(), (int)rec.ShotCount(), (int)i);
                bool isSelected = (g_selectedRecordingIdx == (int)i);
                if (ImGui::Selectable(label, isSelected)) {
                    g_selectedRecordingIdx = (int)i;
                }
            }

            ImGui::End();
        }

        // Panel: Recoil Trajectory 2D Chart (ImPlot)
        {
            ImGui::Begin("Recoil Trajectory (Chart Preview)");
            if (g_selectedRecordingIdx >= 0 && g_selectedRecordingIdx < (int)g_activeWeapon.recordings.size()) {
                const auto& rec = g_activeWeapon.recordings[g_selectedRecordingIdx];
                
                ImGui::Text("Active: %s | Shots: %d | Duration: %lld ms", 
                    rec.recording_id.c_str(), (int)rec.ShotCount(), (long long)rec.TotalDurationMs());

                if (!rec.raw_shots.empty()) {
                    // Extract X and Y coords
                    std::vector<float> xs(rec.raw_shots.size());
                    std::vector<float> ys(rec.raw_shots.size());
                    for (size_t i = 0; i < rec.raw_shots.size(); ++i) {
                        xs[i] = rec.raw_shots[i].cum_x;
                        ys[i] = -rec.raw_shots[i].cum_y; // Invert Y for screen display: upward kick goes UP
                    }

                    if (ImPlot::BeginPlot("Trajectory (Cumulative X / -Y)", ImVec2(-1, -1))) {
                        ImPlot::SetupAxes("Cumulative Horizontal Drift (X)", "Cumulative Upward Climb (-Y)");
                        ImPlot::PlotLine("Spray Line", xs.data(), ys.data(), (int)xs.size());
                        ImPlot::PlotScatter("Shots", xs.data(), ys.data(), (int)xs.size());
                        ImPlot::EndPlot();
                    }
                }
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No burst recorded. Click 'Simulate Burst' in the simulator panel.");
            }
            ImGui::End();
        }

        // Panel: Shot Table Inspector
        {
            ImGui::Begin("Shot Data Table");
            if (g_selectedRecordingIdx >= 0 && g_selectedRecordingIdx < (int)g_activeWeapon.recordings.size()) {
                const auto& rec = g_activeWeapon.recordings[g_selectedRecordingIdx];
                if (ImGui::BeginTable("ShotTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                    ImGui::TableSetupColumn("Shot #");
                    ImGui::TableSetupColumn("Time (ms)");
                    ImGui::TableSetupColumn("Interval (ms)");
                    ImGui::TableSetupColumn("Delta X");
                    ImGui::TableSetupColumn("Delta Y");
                    ImGui::TableSetupColumn("Cum X");
                    ImGui::TableSetupColumn("Cum Y");
                    ImGui::TableSetupColumn("Source");
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
                ImGui::Text("No active recording selected.");
            }
            ImGui::End();
        }

        // Render
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.1f, 0.1f, 0.12f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = g_pSwapChain->Present(1, 0); // VSync enabled
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions for DirectX 11
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        &featureLevel,
        &g_pd3dDeviceContext
    );
    if (res == DXGI_ERROR_UNSUPPORTED) // Fallback to WARP if no HW D3D11
    {
        res = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &g_pSwapChain,
            &g_pd3dDevice,
            &featureLevel,
            &g_pd3dDeviceContext
        );
    }
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
