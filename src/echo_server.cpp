#include "server/echo_server.hpp"

#include <csignal>
#include <iostream>
#include <memory>
#include <string>

static std::unique_ptr<loadgen::server::EchoServer> g_server;

static void handle_signal(int sig) {
    (void)sig;
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char** argv) {
    loadgen::ServerConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 < argc) return argv[++i];
            return "";
        };

        if (a == "--port") {
            config.port = std::stoi(next());
        } else if (a == "--bind") {
            config.bind_address = next();
        } else if (a == "--backlog") {
            config.backlog = std::stoi(next());
        } else if (a == "--buffer-size") {
            config.buffer_size_kb = std::stoi(next());
        } else if (a == "--nodelay") {
            config.tcp_nodelay = (std::stoi(next()) != 0);
        } else if (a == "--help" || a == "-h") {
            std::cout << "Usage: echo_server [OPTIONS]\n"
                      << "Options:\n"
                      << "  --port PORT           Port to listen on (default: 9000)\n"
                      << "  --bind ADDR           Address to bind (default: 127.0.0.1)\n"
                      << "  --backlog N           Listen backlog size (default: 1024)\n"
                      << "  --buffer-size KB      Socket buffer size in KB (default: 64)\n"
                      << "  --nodelay 0|1         Enable TCP_NODELAY (default: 1)\n"
                      << "  --help, -h            Show this help message\n";
            return 0;
        }
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    g_server = std::make_unique<loadgen::server::EchoServer>(config);
    return g_server->run();
}
