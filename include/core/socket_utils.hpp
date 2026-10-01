#pragma once

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace loadgen {
namespace socket_utils {

inline bool set_tcp_nodelay(int fd, bool enable) {
    int flag = enable ? 1 : 0;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) == 0;
}

inline bool set_reuse_addr(int fd, bool enable) {
    int flag = enable ? 1 : 0;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == 0;
}

inline bool set_reuse_port(int fd, bool enable) {
#ifdef SO_REUSEPORT
    int flag = enable ? 1 : 0;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag)) == 0;
#else
    (void)fd;
    (void)enable;
    return false;
#endif
}

inline bool set_nonblocking(int fd, bool enable) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(fd, F_SETFL, flags) == 0;
}

inline bool set_buffer_sizes(int fd, int rcvbuf, int sndbuf) {
    bool ok = true;
    if (rcvbuf > 0) {
        if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) != 0) ok = false;
    }
    if (sndbuf > 0) {
        if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) != 0) ok = false;
    }
    return ok;
}

inline bool set_socket_timeout(int fd, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    bool ok1 = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
    bool ok2 = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
    return ok1 && ok2;
}

inline int create_tcp_listener(const std::string& host, int port, int backlog = 1024, bool nodelay = true) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    set_reuse_addr(fd, true);
    set_reuse_port(fd, true);
    if (nodelay) {
        set_tcp_nodelay(fd, true);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (host.empty() || host == "0.0.0.0") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (host == "127.0.0.1" || host == "localhost") {
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else {
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
            close(fd);
            return -1;
        }
    }

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, backlog) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

inline int create_tcp_client(const std::string& host, int port, bool nodelay = true, int timeout_ms = 5000) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    if (nodelay) {
        set_tcp_nodelay(fd, true);
    }
    if (timeout_ms > 0) {
        set_socket_timeout(fd, timeout_ms);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (host == "localhost" || host == "127.0.0.1") {
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else {
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
            close(fd);
            return -1;
        }
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

inline bool send_exact(int fd, const void* buffer, std::size_t len) {
    const char* ptr = static_cast<const char*>(buffer);
    std::size_t remaining = len;
    while (remaining > 0) {
#ifdef MSG_NOSIGNAL
        ssize_t n = send(fd, ptr, remaining, MSG_NOSIGNAL);
#else
        ssize_t n = send(fd, ptr, remaining, 0);
#endif
        if (n <= 0) {
            if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue;
            }
            return false;
        }
        ptr += n;
        remaining -= static_cast<std::size_t>(n);
    }
    return true;
}

inline bool recv_exact(int fd, void* buffer, std::size_t len) {
    char* ptr = static_cast<char*>(buffer);
    std::size_t remaining = len;
    while (remaining > 0) {
        ssize_t n = recv(fd, ptr, remaining, 0);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue;
            }
            return false;
        }
        ptr += n;
        remaining -= static_cast<std::size_t>(n);
    }
    return true;
}

} // namespace socket_utils
} // namespace loadgen
