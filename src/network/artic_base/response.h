// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include "network/artic_base/common.h"

namespace Network::ArticBase {

using namespace ArticBaseCommon;

class Response {
public:
    explicit Response(u32 req_id) : flag{std::make_unique<std::atomic_flag>()},
          request_id{req_id} {}
    ~Response() {
        // Unblock anyone waiting on the response.
        flag->test_and_set();
    }

    Response(Response&) = delete;
    Response& operator=(Response&) = delete;

    Response(Response&&) noexcept = default;
    Response& operator=(Response&&) noexcept = default;

    bool Succeeded() const {
        return artic_result == ResponseMethod::ArticResult::SUCCESS;
    }

    int GetMethodResult() const {
        return method_result;
    }

    void Wait() {
        flag->wait(true);
    }

    void Signal() {
        flag->test_and_set();
    }

    template <std::integral T>
    std::optional<T> GetResponse(u32 buffer_id) const {
        const auto it = std::ranges::find(buffer, buffer_id, &Buffer::bufferID);
        if (it == buffer.end() || it->bufferSize != sizeof(T)) {
            return std::nullopt;
        }
        T value;
        std::memcpy(&value, it->data, sizeof(T));
        return value;
    }

public:
    // Start in error state in case the request is not fullfilled properly.
    ResponseMethod::ArticResult artic_result{ResponseMethod::ArticResult::METHOD_ERROR};
    union {
        MethodState method_state{MethodState::INTERNAL_METHOD_ERROR};
        s32 method_result;
    };
    std::vector<Buffer> buffer{};
    std::unique_ptr<std::atomic_flag> flag;
    u32 request_id;
};

} // namespace Network::ArticBase
