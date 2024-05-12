// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <condition_variable>
#include <cstring>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include "common/polyfill_thread.h"
#include "network/artic_base/request.h"
#include "network/artic_base/response.h"
#include "network/socket_manager.h"

namespace Network::ArticBase {

class Client {
    static constexpr const s32 SERVER_VERSION = 0;
    using Timepoint = std::chrono::time_point<std::chrono::steady_clock>;

public:
    explicit Client(const std::string& _address, u16 _port);
    ~Client();

    bool Connect();

    std::optional<Response> Send(Request& request);

    size_t GetServerRequestMaxSize() {
        return max_server_work_ram;
    }

    Request NewRequest(std::string_view method) {
        return Request(GetNextRequestID(), method, max_parameter_count);
    }

    void Stop() {
        StopImpl(false);
    }

    void SetCommunicationErrorCallback(const std::function<void()>& callback) {
        communication_error_callback = callback;
    }

    void SetArticReportTrafficCallback(const std::function<void(s32)>& callback) {
        traffic_callback = callback;
    }

    void ReportArticEvent(u64 event) {
        if (report_artic_event_callback) {
            report_artic_event_callback(event);
        }
    }

    void SetReportArticEventCallback(const std::function<void(u64)>& callback) {
        report_artic_event_callback = callback;
    }

private:
    u32 GetNextRequestID() {
        return current_req_id++;
    }

    void SignalCommunicationError();

    void StopImpl(bool from_error);

    void PingFunction(std::stop_token token);

    void RunHandler(std::stop_token token, u32 addr, u16 port);

    std::optional<DataPacket> SendRequestPacket(
        const Request& req, bool expect_response,
        const std::chrono::nanoseconds& read_timeout = std::chrono::nanoseconds(0));

    std::optional<std::string> SendSimpleRequest(const std::string& method);

private:
    bool connected = false;
    std::string address;
    u32 addr;
    u16 port;
    Socket::Socket main_socket;
    std::atomic<u32> current_req_id{};
    std::function<void()> communication_error_callback;
    std::function<void(u64)> report_artic_event_callback;
    size_t max_server_work_ram = 0;
    size_t max_parameter_count = 0;
    std::mutex send_mutex;
    std::atomic_bool stopped = false;
    std::atomic<Timepoint> last_sent_request;
    std::jthread ping_thread;
    std::condition_variable_any ping_cv;
    std::mutex ping_cv_mutex;
    Socket::TrafficFunc traffic_callback;
    std::mutex recv_map_mutex;
    std::vector<Response> pending;
    std::vector<std::jthread> handlers;
    std::atomic<u32> running_handlers;
};

} // namespace Network::ArticBase
