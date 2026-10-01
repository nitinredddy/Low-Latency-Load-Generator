#pragma once

#include "core/socket_utils.hpp"
#include "core/types.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace loadgen {
namespace server {

struct ConnectionState {
    int fd{-1};
    std::vector<uint8_t> out_buffer;
};

class EchoServer {
public:
    explicit EchoServer(ServerConfig config)
        : config_(std::move(config)) {}

    ~EchoServer() {
        stop();
    }

    void stop() {
        running_ = false;
    }

    int run() {
        int listener = socket_utils::create_tcp_listener(
            config_.bind_address, config_.port, config_.backlog, config_.tcp_nodelay);
        if (listener < 0) {
            std::cerr << "[-] Error: Failed to bind/listen on port " << config_.port << ": "
                      << std::strerror(errno) << std::endl;
            return 1;
        }

        std::cout << "===================================================================\n"
                  << "           LOW-LATENCY TCP ECHO SERVER (Event-Driven)              \n"
                  << "===================================================================\n"
                  << " Address:        " << config_.bind_address << ":" << config_.port << "\n"
                  << " Socket Options: TCP_NODELAY=" << (config_.tcp_nodelay ? "ON" : "OFF")
                  << ", Backlog=" << config_.backlog << "\n"
                  << " Buffer Size:    " << config_.buffer_size_kb << " KB\n"
                  << "===================================================================\n"
                  << "LISTENING " << config_.port << std::endl;

        std::vector<pollfd> fds;
        fds.push_back({listener, POLLIN, 0});

        std::vector<uint8_t> read_buffer(static_cast<std::size_t>(config_.buffer_size_kb * 1024));
        std::unordered_map<int, ConnectionState> connections;

        uint64_t total_requests = 0;
        uint64_t total_bytes = 0;
        running_ = true;

        while (running_) {
            int rc = poll(fds.data(), static_cast<nfds_t>(fds.size()), 500);
            if (rc < 0) {
                if (errno == EINTR) continue;
                perror("poll");
                break;
            }
            if (rc == 0) continue;

            for (std::size_t i = 0; i < fds.size(); ++i) {
                if (fds[i].revents == 0) continue;

                // Handle error or hangup on socket
                if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    if (fds[i].fd == listener) {
                        running_ = false;
                        break;
                    }
                    close_client(fds[i].fd, connections);
                    fds.erase(fds.begin() + static_cast<long>(i));
                    --i;
                    continue;
                }

                // Handle incoming connection on listener
                if (fds[i].fd == listener && (fds[i].revents & POLLIN)) {
                    sockaddr_in client_addr{};
                    socklen_t addr_len = sizeof(client_addr);
                    int client_fd = accept(listener, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
                    if (client_fd >= 0) {
                        socket_utils::set_nonblocking(client_fd, true);
                        if (config_.tcp_nodelay) {
                            socket_utils::set_tcp_nodelay(client_fd, true);
                        }
                        fds.push_back({client_fd, POLLIN, 0});
                        connections[client_fd] = ConnectionState{client_fd, {}};
                    }
                    continue;
                }

                int client_fd = fds[i].fd;
                auto conn_it = connections.find(client_fd);
                if (conn_it == connections.end()) continue;

                // Handle write ready (if queued outgoing data exists)
                if (fds[i].revents & POLLOUT) {
                    auto& out_buf = conn_it->second.out_buffer;
                    if (!out_buf.empty()) {
#ifdef MSG_NOSIGNAL
                        ssize_t w = send(client_fd, out_buf.data(), out_buf.size(), MSG_NOSIGNAL);
#else
                        ssize_t w = send(client_fd, out_buf.data(), out_buf.size(), 0);
#endif
                        if (w > 0) {
                            out_buf.erase(out_buf.begin(), out_buf.begin() + w);
                        } else if (w < 0 && (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                            close_client(client_fd, connections);
                            fds.erase(fds.begin() + static_cast<long>(i));
                            --i;
                            continue;
                        }
                    }
                    if (out_buf.empty()) {
                        fds[i].events = POLLIN; // No more pending writes
                    }
                }

                // Handle read ready
                if (fds[i].revents & POLLIN) {
                    ssize_t n = recv(client_fd, read_buffer.data(), read_buffer.size(), 0);
                    if (n <= 0) {
                        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                            continue;
                        }
                        close_client(client_fd, connections);
                        fds.erase(fds.begin() + static_cast<long>(i));
                        --i;
                        continue;
                    }

                    total_requests++;
                    total_bytes += static_cast<uint64_t>(n);

                    // Echo back immediately or buffer if blocked
                    auto& out_buf = conn_it->second.out_buffer;
                    if (out_buf.empty()) {
#ifdef MSG_NOSIGNAL
                        ssize_t w = send(client_fd, read_buffer.data(), static_cast<size_t>(n), MSG_NOSIGNAL);
#else
                        ssize_t w = send(client_fd, read_buffer.data(), static_cast<size_t>(n), 0);
#endif
                        if (w > 0 && w < n) {
                            out_buf.insert(out_buf.end(), read_buffer.begin() + w, read_buffer.begin() + n);
                            fds[i].events |= POLLOUT;
                        } else if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                            out_buf.insert(out_buf.end(), read_buffer.begin(), read_buffer.begin() + n);
                            fds[i].events |= POLLOUT;
                        } else if (w < 0) {
                            close_client(client_fd, connections);
                            fds.erase(fds.begin() + static_cast<long>(i));
                            --i;
                            continue;
                        }
                    } else {
                        out_buf.insert(out_buf.end(), read_buffer.begin(), read_buffer.begin() + n);
                        fds[i].events |= POLLOUT;
                    }
                }
            }
        }

        for (const auto& p : connections) {
            close(p.first);
        }
        close(listener);
        std::cout << "\n[*] Server shutdown gracefully. Processed " << total_requests
                  << " requests (" << (total_bytes / (1024.0 * 1024.0)) << " MB transferred).\n";
        return 0;
    }

private:
    void close_client(int fd, std::unordered_map<int, ConnectionState>& conns) {
        conns.erase(fd);
        close(fd);
    }

    ServerConfig config_;
    std::atomic<bool> running_{false};
};

} // namespace server
} // namespace loadgen
