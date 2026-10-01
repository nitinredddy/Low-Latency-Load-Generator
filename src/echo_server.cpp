#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

static int make_listener(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind"); close(fd); return -1;
    }
    if (listen(fd, 256) < 0) {
        perror("listen"); close(fd); return -1;
    }
    return fd;
}

int main(int argc, char** argv) {
    int port = 9000;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (a == "--help") {
            std::cout << "Usage: echo_server [--port PORT]\n";
            return 0;
        }
    }

    int listener = make_listener(port);
    if (listener < 0) return 1;

    std::cout << "LISTENING " << port << std::endl;

    std::vector<pollfd> fds;
    fds.push_back({listener, POLLIN, 0});
    std::vector<char> buffer(64 * 1024);

    while (true) {
        int rc = poll(fds.data(), static_cast<nfds_t>(fds.size()), 1000);
        if (rc < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        if (rc == 0) continue;

        for (std::size_t i = 0; i < fds.size(); ++i) {
            if (!(fds[i].revents & POLLIN)) continue;

            if (fds[i].fd == listener) {
                int client = accept(listener, nullptr, nullptr);
                if (client >= 0) fds.push_back({client, POLLIN, 0});
                continue;
            }

            ssize_t n = recv(fds[i].fd, buffer.data(), buffer.size(), 0);
            if (n <= 0) {
                close(fds[i].fd);
                fds.erase(fds.begin() + static_cast<long>(i));
                --i;
                continue;
            }

            ssize_t sent = 0;
            while (sent < n) {
                ssize_t w = send(fds[i].fd, buffer.data() + sent,
                                 static_cast<size_t>(n - sent), MSG_NOSIGNAL);
                if (w <= 0) break;
                sent += w;
            }
        }
    }

    close(listener);
    return 0;
}
