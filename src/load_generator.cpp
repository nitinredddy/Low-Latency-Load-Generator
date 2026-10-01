#include <arpa/inet.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <algorithm>

using Clock = std::chrono::steady_clock;

struct Config {
    std::string host = "127.0.0.1";
    int port = 9000;
    int requests = 1000;
    double rate = 1000.0;
    int payload = 64;
    std::string output;
};

static bool send_all(int fd, const char* data, std::size_t len) {
    std::size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

static bool recv_all(int fd, char* data, std::size_t len) {
    std::size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, data + got, len - got, 0);
        if (n <= 0) return false;
        got += static_cast<std::size_t>(n);
    }
    return true;
}

static int connect_server(const Config& c) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(c.port));
    if (inet_pton(AF_INET, c.host.c_str(), &addr.sin_addr) != 1) {
        close(fd); return -1;
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd); return -1;
    }
    return fd;
}

static double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    double idx = (p / 100.0) * static_cast<double>(v.size() - 1);
    std::size_t lo = static_cast<std::size_t>(std::floor(idx));
    std::size_t hi = static_cast<std::size_t>(std::ceil(idx));
    if (lo == hi) return v[lo];
    return v[lo] + (idx - lo) * (v[hi] - v[lo]);
}

int main(int argc, char** argv) {
    Config c;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return argv[++i]; };
        if (a == "--host") c.host = next();
        else if (a == "--port") c.port = std::stoi(next());
        else if (a == "--requests") c.requests = std::stoi(next());
        else if (a == "--rate") c.rate = std::stod(next());
        else if (a == "--payload") c.payload = std::stoi(next());
        else if (a == "--output") c.output = next();
        else if (a == "--help") {
            std::cout << "Usage: load_generator [--host H] [--port P] "
                         "[--requests N] [--rate RPS] [--payload BYTES] [--output FILE]\n";
            return 0;
        }
    }

    if (c.requests <= 0 || c.rate <= 0 || c.payload <= 0) {
        std::cerr << "requests, rate and payload must be positive\n";
        return 2;
    }

    int fd = connect_server(c);
    if (fd < 0) {
        std::cerr << "could not connect to " << c.host << ":" << c.port << "\n";
        return 1;
    }

    std::vector<char> payload(static_cast<std::size_t>(c.payload), 'x');
    std::vector<char> response(payload.size());
    std::vector<double> latencies_us;
    latencies_us.reserve(c.requests);

    int completed = 0;
    int errors = 0;
    auto start = Clock::now();
    auto next_send = start;
    const auto interval = std::chrono::duration<double>(1.0 / c.rate);

    for (int i = 0; i < c.requests; ++i) {
        next_send += std::chrono::duration_cast<Clock::duration>(interval);
        std::this_thread::sleep_until(next_send);

        auto t0 = Clock::now();
        bool ok = send_all(fd, payload.data(), payload.size()) &&
                  recv_all(fd, response.data(), response.size());
        auto t1 = Clock::now();

        if (ok && response == payload) {
            ++completed;
            latencies_us.push_back(
                std::chrono::duration<double, std::micro>(t1 - t0).count());
        } else {
            ++errors;
        }
    }

    auto end = Clock::now();
    close(fd);

    double elapsed = std::chrono::duration<double>(end - start).count();
    double throughput = elapsed > 0 ? completed / elapsed : 0.0;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "requests=" << c.requests << "\n";
    std::cout << "completed=" << completed << "\n";
    std::cout << "errors=" << errors << "\n";
    std::cout << "throughput_rps=" << throughput << "\n";
    std::cout << "p50_us=" << percentile(latencies_us, 50) << "\n";
    std::cout << "p95_us=" << percentile(latencies_us, 95) << "\n";
    std::cout << "p99_us=" << percentile(latencies_us, 99) << "\n";
    std::cout << "max_us=" << (latencies_us.empty() ? 0.0 :
                                 *std::max_element(latencies_us.begin(), latencies_us.end())) << "\n";

    if (!c.output.empty()) {
        std::ofstream out(c.output);
        out << "request_index,latency_us\n";
        for (std::size_t i = 0; i < latencies_us.size(); ++i)
            out << i << "," << latencies_us[i] << "\n";
    }

    return errors == 0 ? 0 : 1;
}
