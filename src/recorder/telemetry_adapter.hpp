#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <iostream>
#include <sstream>
#include <vector>
#include "json.hpp"

namespace Recoil {

// Callback ketika event shot/displacement diterima dari telemetry
// timestamp_ms <= 0 menandakan recorder sebaiknya menggunakan timer internal QPC
using ShotTelemetryCallback = std::function<void(float delta_x, float delta_y, int64_t timestamp_ms)>;

/**
 * @brief Interface ITelemetryAdapter sesuai Task 2.3.1
 * Memisahkan secara modular sumber input tembakan (UDP, Named Pipe, Mock, Hotkey)
 * dari engine analisis recorder.
 */
class ITelemetryAdapter {
protected:
    ShotTelemetryCallback m_callback;

public:
    virtual ~ITelemetryAdapter() = default;

    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual bool IsActive() const = 0;
    virtual std::string GetName() const = 0;

    void SetCallback(ShotTelemetryCallback callback) {
        m_callback = callback;
    }

    void DispatchShot(float delta_x, float delta_y, int64_t timestamp_ms = -1) {
        if (m_callback) {
            m_callback(delta_x, delta_y, timestamp_ms);
        }
    }
};

/**
 * @brief Local UDP Socket Telemetry Adapter (Task 2.3.2)
 * Menerima telemetry tembakan secara asynchronous melalui UDP loopback port (default 127.0.0.1:9988).
 * Mendukung format:
 * 1. Plaintext CSV: "dx,dy" atau "dx,dy,time_ms" (misal: "1.25,-4.80,1833")
 * 2. JSON: {"dx": 1.25, "dy": -4.8, "timeMs": 1833}
 * 3. Binary packet: struct { float dx; float dy; int64_t timestamp_ms; }
 */
class UdpTelemetryAdapter : public ITelemetryAdapter {
private:
    int m_port;
    SOCKET m_socket{INVALID_SOCKET};
    std::thread m_workerThread;
    std::atomic<bool> m_running{false};
    bool m_wsaInitialized{false};

public:
    explicit UdpTelemetryAdapter(int port = 9988) : m_port(port) {}

    ~UdpTelemetryAdapter() override {
        Stop();
    }

    std::string GetName() const override {
        return "UDP Socket (" + std::to_string(m_port) + ")";
    }

    int GetPort() const { return m_port; }

    bool IsActive() const override {
        return m_running;
    }

    bool Start() override {
        if (m_running) return true;

        WSADATA wsaData;
        int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (wsaResult != 0) {
            std::cerr << "[UDP] WSAStartup failed: " << wsaResult << std::endl;
            return false;
        }
        m_wsaInitialized = true;

        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == INVALID_SOCKET) {
            std::cerr << "[UDP] Socket creation failed: " << WSAGetLastError() << std::endl;
            WSACleanup();
            m_wsaInitialized = false;
            return false;
        }

        // Set socket non-blocking atau timeout pendek agar thread bisa berhenti dengan aman
        DWORD timeout_ms = 250;
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        serverAddr.sin_port = htons(static_cast<u_short>(m_port));

        if (bind(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "[UDP] Bind failed on port " << m_port << ": " << WSAGetLastError() << std::endl;
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            WSACleanup();
            m_wsaInitialized = false;
            return false;
        }

        m_running = true;
        m_workerThread = std::thread(&UdpTelemetryAdapter::ListenLoop, this);
        return true;
    }

    void Stop() override {
        if (!m_running) return;

        m_running = false;
        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }

        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        if (m_wsaInitialized) {
            WSACleanup();
            m_wsaInitialized = false;
        }
    }

private:
    void ListenLoop() {
        char buffer[1024];
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);

        while (m_running) {
            int bytesReceived = recvfrom(m_socket, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&clientAddr, &clientLen);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                ParsePacket(buffer, bytesReceived);
            } else {
                int err = WSAGetLastError();
                if (err != WSAETIMEDOUT && err != WSAEWOULDBLOCK && m_running) {
                    // Jeda sejenak untuk menghindari CPU busy wait saat socket error
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        }
    }

    void ParsePacket(const char* data, int length) {
        // 1. Coba Binary Struct jika panjang persis 16 byte (2 float + 1 int64_t)
        #pragma pack(push, 1)
        struct BinaryShotPacket {
            float dx;
            float dy;
            int64_t timestamp_ms;
        };
        #pragma pack(pop)

        if (length == sizeof(BinaryShotPacket)) {
            const auto* pkt = reinterpret_cast<const BinaryShotPacket*>(data);
            DispatchShot(pkt->dx, pkt->dy, pkt->timestamp_ms);
            return;
        }

        std::string text(data, length);

        // 2. Coba JSON jika dimulai dengan '{'
        if (!text.empty() && text.front() == '{') {
            try {
                auto j = nlohmann::json::parse(text);
                float dx = j.value("dx", 0.0f);
                float dy = j.value("dy", 0.0f);
                int64_t timeMs = j.value("timeMs", (int64_t)-1);
                DispatchShot(dx, dy, timeMs);
                return;
            } catch (...) {
                // Bukan valid JSON, coba CSV fallback
            }
        }

        // 3. Fallback: Parse CSV text (format: "dx,dy" atau "dx,dy,time_ms")
        std::stringstream ss(text);
        std::string item;
        std::vector<std::string> tokens;
        while (std::getline(ss, item, ',')) {
            tokens.push_back(item);
        }

        if (tokens.size() >= 2) {
            try {
                float dx = std::stof(tokens[0]);
                float dy = std::stof(tokens[1]);
                int64_t timeMs = (tokens.size() >= 3) ? std::stoll(tokens[2]) : -1;
                DispatchShot(dx, dy, timeMs);
            } catch (...) {
                // Ignore parse errors
            }
        }
    }
};

/**
 * @brief Manual / Trigger Telemetry Adapter (Task 2.3.3)
 * Digunakan untuk trigger tembakan manual via UI / Hotkey / Script.
 */
class ManualTelemetryAdapter : public ITelemetryAdapter {
private:
    std::atomic<bool> m_active{false};

public:
    std::string GetName() const override {
        return "Manual Trigger / Hotkey";
    }

    bool Start() override {
        m_active = true;
        return true;
    }

    void Stop() override {
        m_active = false;
    }

    bool IsActive() const override {
        return m_active;
    }

    void TriggerShot(float delta_x, float delta_y, int64_t timestamp_ms = -1) {
        if (m_active) {
            DispatchShot(delta_x, delta_y, timestamp_ms);
        }
    }
};

/**
 * @brief Helper utility untuk mengirim paket UDP test (berguna untuk unit testing & script dev)
 */
namespace NetworkTestHelper {
    inline bool SendUdpPacket(int port, const std::string& message) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

        SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }

        sockaddr_in destAddr{};
        destAddr.sin_family = AF_INET;
        destAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        destAddr.sin_port = htons(static_cast<u_short>(port));

        int result = sendto(sock, message.c_str(), static_cast<int>(message.length()), 0, (sockaddr*)&destAddr, sizeof(destAddr));

        closesocket(sock);
        WSACleanup();
        return result != SOCKET_ERROR;
    }
}

} // namespace Recoil
