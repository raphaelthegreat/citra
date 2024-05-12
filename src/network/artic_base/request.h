// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <vector>
#include "common/common_types.h"
#include "network/artic_base/common.h"

namespace Network::ArticBase {

class Request {
public:
    explicit Request(u32 request_id, std::string_view method, size_t max_params)
        : method_name{method}, max_param_count{max_params} {
        ASSERT(method.size() <= ArticBaseCommon::MaxMethodSize);
        packet.request_id = request_id;
        method.copy(packet.method.data(), method.size());
    }

    u32 RequestID() const {
        return packet.request_id;
    }

    template <typename T, size_t size = sizeof(T)>
        requires std::is_integral_v<T> || std::is_enum_v<T>
    bool AddParameter(T parameter) {
        if (params.size() >= max_param_count) {
            LOG_ERROR(Network, "Too many parameters added to method: {}", method_name);
            return false;
        }
        packet.parameter_count++;
        auto& param = params.emplace_back();
        param.type = ArticBaseCommon::RequestParameterType::IN_INTEGER_8;
        std::memcpy(param.data, &parameter, sizeof(T));
        return true;
    }

    template <typename T> requires std::is_standard_layout_v<T>
    bool AddParameterBuffer(const T& value) {
        const u8* ptr = reinterpret_cast<const u8*>(&value);
        return AddParameterBuffer({ptr, sizeof(value)});
    }

    bool AddParameterBuffer(std::span<const u8> buffer) {
        if (params.size() >= max_param_count) {
            LOG_ERROR(Network, "Too many parameters added to method: {}", method_name);
            return false;
        }
        packet.parameter_count++;
        auto& param = params.emplace_back();
        const size_t size = buffer.size_bytes();
        if (size <= sizeof(param.data)) {
            param.type = ArticBaseCommon::RequestParameterType::IN_SMALL_BUFFER;
            std::memcpy(param.data, buffer.data(), size);
            param.parameterSize = static_cast<u16>(size);
        } else {
            param.type = ArticBaseCommon::RequestParameterType::IN_BIG_BUFFER;
            param.big_buffer_id = static_cast<u16>(pending_big_buffers.size());
            std::memcpy(param.data, &size, sizeof(u32));
            pending_big_buffers.push_back(buffer);
        }
        return true;
    }

public:
    ArticBaseCommon::RequestPacket packet{};
    std::vector<ArticBaseCommon::RequestParameter> params;
    std::string_view method_name;
    size_t max_param_count;
    std::vector<std::span<const u8>> pending_big_buffers;
};

} // namespace Network::ArticBase
