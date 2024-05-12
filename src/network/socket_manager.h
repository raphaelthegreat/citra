// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <span>

namespace Network::Socket {

#ifdef _WIN32
using Handle = unsigned long long;
#else
using Holder = int;
#endif // _WIN32

static constexpr auto NoTimeout = std::chrono::nanoseconds(0);

using TrafficFunc = std::function<void(u32)>;

void EnableSockets();

void DisableSockets();

Handle Open();

void Close(Handle fd);

u32 GetAddress(std::string_view addr);

class Socket {
public:
    Socket() {
        fd = Open();
    }
    ~Socket() {
        Close(fd);
    }

    operator Handle() const {
        return fd;
    }

    bool SetNonBlock(bool non_blocking);

    bool ConnectWithTimeout(u16 port, u32 addr,
                            std::chrono::seconds timeout);

    template <typename T>
    bool Read(std::span<T> buffer,
              const std::chrono::nanoseconds& timeout = NoTimeout) {
        return Read(buffer.data(), buffer.size_bytes(), timeout);
    }

    template <typename T> requires std::is_standard_layout_v<T>
    bool Read(T& value,
              const std::chrono::nanoseconds& timeout = NoTimeout) {
        return Read(&value, sizeof(value), timeout);
    }

    bool Read(void* buffer, size_t size,
              const std::chrono::nanoseconds& timeout = NoTimeout);

    template <typename T> requires std::is_standard_layout_v<T>
    bool Write(const T& value,
               const std::chrono::nanoseconds& timeout = NoTimeout) {
        return Write(&value, sizeof(value), timeout);
    }

    template <typename T>
    bool Write(std::span<const T> buffer,
               const std::chrono::nanoseconds& timeout = NoTimeout) {
        return Write(buffer.data(), buffer.size_bytes(), timeout);
    }

    bool Write(const void* buffer, size_t size,
               const std::chrono::nanoseconds& timeout = NoTimeout);

private:
    Handle fd{0};
    TrafficFunc traffic_callback;
};

} // namespace Network::Socket
