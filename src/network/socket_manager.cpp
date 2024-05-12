// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include "network/socket_manager.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define WSAEAGAIN WSAEWOULDBLOCK
#define WSAEMULTIHOP -1 // Invalid dummy value
#define ERRNO(x) WSA##x
#define GET_ERRNO WSAGetLastError()
#define poll(x, y, z) WSAPoll(x, y, z);
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#else
#define ERRNO(x) x
#define GET_ERRNO errno
#define closesocket(x) close(x)
#endif

namespace Network::Socket {

static std::atomic_uint32_t count = 0;

void EnableSockets() {
    if (count++ == 0) {
#ifdef _WIN32
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
#endif
    }
}

void DisableSockets() {
    if (--count == 0) {
#ifdef _WIN32
        WSACleanup();
#endif
    }
}

Handle Open() {
    return ::socket(AF_INET, SOCK_STREAM, 0);
}

void Close(Handle fd) {
    if (fd) {
        shutdown(fd, SHUT_RDWR);
        closesocket(fd);
    }
}

u32 GetAddress(std::string_view address) {
    struct addrinfo hints, *addrinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;

    if (getaddrinfo(address.data(), NULL, &hints, &addrinfo) != 0) {
        return -1;
    }

    const auto* ai_addr = reinterpret_cast<sockaddr_in*>(addrinfo->ai_addr);
    const u32 addr = ai_addr->sin_addr.s_addr;
    freeaddrinfo(addrinfo);
    return addr;
}

bool Socket::SetNonBlock(bool non_blocking) {
    bool blocking = !non_blocking;
#ifdef _WIN32
    unsigned long nonblocking = blocking ? 0 : 1;
    int ret = ::ioctlsocket(fd, FIONBIO, &nonblocking);
    if (ret == -1) {
        return false;
    }
#else
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }

    flags &= ~O_NONBLOCK;
    if (non_blocking) { // O_NONBLOCK
        flags |= O_NONBLOCK;
    }

    const int ret = ::fcntl(fd, F_SETFL, flags);
    if (ret == -1) {
        return false;
    }
#endif
    return true;
}

bool Socket::ConnectWithTimeout(u16 port, u32 addr,
                                std::chrono::seconds timeout) {
    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = static_cast<decltype(servaddr.sin_addr.s_addr)>(addr);
    servaddr.sin_port = htons(port);

    int res = ::connect(fd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if (res == -1 && ((GET_ERRNO == ERRNO(EINPROGRESS) || GET_ERRNO == ERRNO(EWOULDBLOCK)))) {
        struct timeval tv;
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);

        tv.tv_sec = timeout.count();
        tv.tv_usec = 0;
        int select_res = ::select(static_cast<int>(fd + 1), NULL, &fdset, NULL, &tv);
#ifdef _WIN32
        if (select_res == 0) {
            return false;
        }
#else
        bool select_good = false;
        if (select_res == 1) {
            int so_error;
            socklen_t len = sizeof so_error;

            getsockopt(sockFD, SOL_SOCKET, SO_ERROR, &so_error, &len);

            if (so_error == 0) {
                select_good = true;
            }
        }
        if (!select_good) {
            return false;
        }
#endif // _WIN32

    } else if (res == -1) {
        return false;
    }
    return true;
}

bool Socket::Read(void* buffer, size_t size,
                 const std::chrono::nanoseconds& timeout) {
    size_t read_bytes = 0;
    auto before = std::chrono::steady_clock::now();
    while (read_bytes != size) {
        char* buf = reinterpret_cast<char*>(uintptr_t(buffer) + read_bytes);
        const int len = static_cast<int>(size - read_bytes);
        int new_read = ::recv(fd, buf, len, 0);
        if (new_read < 0) {
            if (GET_ERRNO == ERRNO(EWOULDBLOCK) &&
                (timeout == std::chrono::nanoseconds(0) ||
                 std::chrono::steady_clock::now() - before < timeout)) {
                continue;
            }
            read_bytes = 0;
            break;
        }
        if (traffic_callback && new_read) {
            traffic_callback(new_read);
        }
        read_bytes += new_read;
    }
    return read_bytes == size;
}

bool Socket::Write(const void* buffer, size_t size,
                   const std::chrono::nanoseconds& timeout) {
    size_t write_bytes = 0;
    auto before = std::chrono::steady_clock::now();
    while (write_bytes != size) {
        int new_written = ::send(fd, (const char*)((uintptr_t)buffer + write_bytes),
                                 (int)(size - write_bytes), 0);
        if (new_written < 0) {
            if (GET_ERRNO == ERRNO(EWOULDBLOCK) &&
                (timeout == NoTimeout ||
                 std::chrono::steady_clock::now() - before < timeout)) {
                continue;
            }
            write_bytes = 0;
            break;
        }
        if (traffic_callback && new_written) {
            traffic_callback(new_written);
        }
        write_bytes += new_written;
    }
    return write_bytes == size;
}

} // namespace Network::Socket
