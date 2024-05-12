// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <ranges>
#include <limits>
#include "common/assert.h"
#include "common/string_util.h"
#include "common/scope_exit.h"
#include "common/logging/log.h"
#include "network/artic_base/client.h"

namespace Network::ArticBase {

using namespace std::chrono_literals;

static constexpr bool DisablePingTimeout = false;
static constexpr auto MaxConnectTimeout = 10s;

Client::Client(const std::string& _address, u16 _port)
    : address(_address), port(_port) {}

Client::~Client() {
    StopImpl(false);
}

void Client::StopImpl(bool from_error) {
    bool expected = false;
    if (!stopped.compare_exchange_strong(expected, true)) {
        return;
    }

    if (!from_error) {
        SendSimpleRequest("STOP");
    }

    handlers.clear();
}

bool Client::Connect() {
    SCOPE_EXIT({
        if (!connected) {
            SignalCommunicationError();
        }
    });

    const auto str_to_int = [](std::string_view str) -> int {
        char* end_ptr{};
        const auto num = std::strtoul(str.data(), &end_ptr, 10);
        if (*end_ptr) {
            return -1;
        }
        return static_cast<int>(num);
    };

    LOG_INFO(Network, "Starting Artic Base Client");

    if (addr = Socket::GetAddress(address); addr == -1) {
        LOG_ERROR(Network, "Failed to get server address {}", address);
        return false;
    }

    if (main_socket == -1) {
        LOG_ERROR(Network, "Failed to create socket");
        return false;
    }

    if (!main_socket.SetNonBlock(true)) {
        LOG_ERROR(Network, "Cannot set non-blocking socket mode");
        return false;
    }

    if (!main_socket.ConnectWithTimeout(port, addr, MaxConnectTimeout)) {
        LOG_ERROR(Network, "Failed to connect");
        return false;
    }

    const auto version = SendSimpleRequest("VERSION");
    if (!version) {
        LOG_ERROR(Network, "Couldn't fetch server version.");
        return false;
    }
    const s32 version_value = str_to_int(*version);
    if (version_value != SERVER_VERSION) {
        LOG_ERROR(Network, "Incompatible server version: {}", version_value);
        return false;
    }

    const auto max_work_size = SendSimpleRequest("MAXSIZE");
    if (!max_work_size) {
        LOG_ERROR(Network, "Couldn't fetch server work ram size");
        return false;
    }
    max_server_work_ram = str_to_int(*max_work_size);
    if (max_server_work_ram < 0) {
        LOG_ERROR(Network, "Couldn't fetch server work ram size");
        return false;
    }

    const auto max_params = SendSimpleRequest("MAXPARAM");
    if (!max_params) {
        LOG_ERROR(Network, "Couldn't fetch server max params");
        return false;
    }
    max_parameter_count = str_to_int(*max_params);
    if (max_parameter_count < 0) {
        LOG_ERROR(Network, "Couldn't fetch server max params");
        return false;
    }

    const auto worker_ports = SendSimpleRequest("PORTS");
    if (!worker_ports) {
        LOG_ERROR(Network, "Couldn't fetch server worker ports");
        return false;
    }

    const auto ports = Common::SplitString(*worker_ports, ',') |
                       std::views::transform(str_to_int);
    const auto is_invalid = [&](u16 port) { return port > std::numeric_limits<u16>::max() ||
                                                   port < 0; };
    if (ports.empty() || std::ranges::any_of(ports, is_invalid)) {
        LOG_ERROR(Network, "Couldn't parse server worker ports");
        return false;
    }

    static constexpr u32 MaxTries = 101;
    static constexpr auto PollInterval = 100ms;

    for (int i = 0; i < MaxTries; i++) {
        const auto ready_server = SendSimpleRequest("READY");
        if (!ready_server || i == MaxTries - 1) {
            LOG_ERROR(Network, "Couldn't fetch server readiness");
            return false;
        }
        if (ready_server.value()[0] == '1') {
            break;
        }
        std::this_thread::sleep_for(PollInterval);
    }

    ping_thread = std::jthread(&Client::PingFunction, this);
    running_handlers = ports.size();

    for (s32 i = 0; i < running_handlers; i++) {
        handlers.emplace_back([&](std::stop_token token) {
            RunHandler(token, addr, ports[i]);
            if (--running_handlers == 0) {
                std::scoped_lock lk{recv_map_mutex};
                pending.clear();
            }
        });
    }

    connected = true;
    return true;
}

std::optional<Response> Client::Send(Request& request) {
    if (stopped) {
        return std::nullopt;
    }

    // Prepare a response for the server to reply to.
    // The handler will remove it from the list.
    Response* response{};
    {
        std::scoped_lock lk{recv_map_mutex};
        const u32 req_id = request.RequestID();
        response = &pending.emplace_back(req_id);
    }

    // Send the request.
    const auto resp_packet = SendRequestPacket(request, false);
    if (stopped || !resp_packet) {
        std::scoped_lock lk{recv_map_mutex};
        pending.pop_back();
        return std::nullopt;
    }

    // Wait for the response to be completed.
    response->Wait();
    return std::move(*response);
}

void Client::SignalCommunicationError() {
    StopImpl(true);
    LOG_CRITICAL(Network, "Communication error");
    if (communication_error_callback) {
        communication_error_callback();
    }
}

void Client::PingFunction(std::stop_token token) {
    // Max silence time => 7 secs interval + 3 secs wait + 10 seconds timeout = 25 seconds
    while (!token.stop_requested()) {
        const auto last = last_sent_request.load();
        if (std::chrono::steady_clock::now() - last > std::chrono::seconds(7)) {
            if constexpr (DisablePingTimeout) {
                last_sent_request = std::chrono::steady_clock::now();
            } else {
                const auto ping_reply = SendSimpleRequest("PING");
                if (!ping_reply.has_value()) {
                    SignalCommunicationError();
                    break;
                }
            }
        }

        std::unique_lock lk{ping_cv_mutex};
        ping_cv.wait_for(lk, std::chrono::seconds(3));
    }
}

std::optional<DataPacket> Client::SendRequestPacket(
    const Request& req, bool expect_response,
    const std::chrono::nanoseconds& read_timeout) {

    std::scoped_lock lk{send_mutex};
    if (main_socket == -1) {
        return std::nullopt;
    }

    const auto& packet = req.packet;
    if (!main_socket.Write(&packet, sizeof(packet))) {
        LOG_WARNING(Network, "Failed to write to socket");
        SignalCommunicationError();
        return std::nullopt;
    }

    const auto& params = req.params;
    if (!params.empty()) {
        if (!main_socket.Write(params)) {
            LOG_WARNING(Network, "Failed to write to socket");
            SignalCommunicationError();
            return std::nullopt;
        }
    }

    DataPacket resp{};
    if (expect_response) {
        if (!main_socket.Read(resp, read_timeout)) {
            LOG_WARNING(Network, "Failed to read from socket");
            SignalCommunicationError();
            return std::nullopt;
        }

        if (resp.request_id != packet.request_id) {
            return std::nullopt;
        }
    }

    last_sent_request = std::chrono::steady_clock::now();
    return resp;
}

std::optional<std::string> Client::SendSimpleRequest(const std::string& method) {
    const Request req{GetNextRequestID(), "$" + method, 0};
    const auto resp = SendRequestPacket(req, true, MaxConnectTimeout);
    if (!resp) {
        return std::nullopt;
    }
    return resp->data_raw;
}

void Client::RunHandler(std::stop_token token, u32 addr, u16 port) {
    Socket::Socket handler_socket{};
    if (!handler_socket) {
        LOG_ERROR(Network, "Failed to create socket");
        return;
    }

    if (!handler_socket.SetNonBlock(true)) {
        SignalCommunicationError();
        LOG_ERROR(Network, "Cannot set non-blocking socket mode");
        return;
    }

    if (!handler_socket.ConnectWithTimeout(addr, port, MaxConnectTimeout)) {
        LOG_ERROR(Network, "Failed to connect");
        SignalCommunicationError();
        return;
    }

    static constexpr auto SleepTime = 100ms;
    static constexpr u32 NumMaxRetries = 300;

    DataPacket packet{};
    u32 retry_count = 0;

    const auto signal_error = [&] {
        if (!token.stop_requested()) {
            SignalCommunicationError();
        }
    };

    const auto try_read = [&] {
        if (handler_socket.Read(packet) || token.stop_requested()) {
            return false;
        }
        LOG_WARNING(Network, "Failed to read from socket");
        std::this_thread::sleep_for(SleepTime);
        return retry_count < NumMaxRetries;
    };

    while (!token.stop_requested()) {
        // Read the data packet sent by the server.
        while (try_read()) {
            retry_count++;
        }
        if (retry_count >= NumMaxRetries) {
            SignalCommunicationError();
            break;
        }
        retry_count = 0;

        // Find which response the packet is being addressed to.
        Response* response{};
        {
            std::scoped_lock lk{recv_map_mutex};
            auto it = std::ranges::find(pending_responses, packet.requestID,
                                        &Response::request_id);
            if (it == pending_responses.end()) {
                continue;
            }
            response = &*it;
        }

        switch (packet.resp.artic_result) {
        case ResponseMethod::ArticResult::SUCCESS: {
            response->artic_result = packet.resp.artic_result;
            response->method_result = packet.resp.methodResult;
            const s32 buffer_size = packet.resp.bufferSize;
            if (!buffer_size) {
                break;
            }
            ASSERT(buffer_size % sizeof(Buffer) == 0);
            response->buffer.resize(buffer_size / sizeof(Buffer));
            if (!handler_socket.Read(response)) {
                signal_error();
            }
            break;
        }
        case ResponseMethod::ArticResult::METHOD_NOT_FOUND: {
            LOG_ERROR(Network, "Method {} not found by server",
                      request.method_name);
            response->artic_result = packet.resp.artic_result;
            break;
        }
        case ResponseMethod::ArticResult::PROVIDE_INPUT: {
            const size_t buffer_id = static_cast<size_t>(packet.resp.provideInputBufferID);
            if (buffer_id >= request.pending_big_buffers.size() ||
                request.pending_big_buffers[buffer_id].second !=
                    static_cast<size_t>(packet.resp.bufferSize)) {
                LOG_ERROR(Network, "Method {} incorrect big buffer state {}",
                          request.method_name, buffer_id);
                packet.resp.artic_result =
                    ResponseMethod::ArticResult::METHOD_ERROR;
                if (handler_socket.Write(packet)) {
                    continue;
                }
                signal_error();
            } else {
                const auto buffer = request.pending_big_buffers[buffer_id];
                if (!handler_socket.Write(packet)) {
                    signal_error();
                    break;
                }
                if (handler_socket.Write(buffer)) {
                    continue;
                }
                signal_error();
            }
            break;
        }
        case ResponseMethod::ArticResult::METHOD_ERROR:
        default: {
            LOG_ERROR(Network, "Method {} error {}", request.method_name,
                      packet.resp.methodResult);
            response->artic_result = packet.resp.artic_result;
            response->method_state = static_cast<MethodState>(packet.resp.methodResult);
            break;
        }
        }

        std::scoped_lock lk{recv_map_mutex};
        std::erase_if(pending, [&](auto& resp) {
            return resp.request_id == packet.request_id;
        });

        response->Signal();
    }
}

} // namespace Network::ArticBase
