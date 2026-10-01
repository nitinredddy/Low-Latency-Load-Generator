#include "core/types.hpp"
#include "generator/load_generator.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    loadgen::GeneratorConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 < argc) return argv[++i];
            return "";
        };

        if (a == "--host") {
            config.host = next();
        } else if (a == "--port") {
            config.port = std::stoi(next());
        } else if (a == "--requests") {
            config.requests = std::stoull(next());
        } else if (a == "--rate") {
            config.rate = std::stod(next());
        } else if (a == "--threads" || a == "-t") {
            config.threads = std::stoi(next());
        } else if (a == "--connections" || a == "-c") {
            config.connections = std::stoi(next());
        } else if (a == "--payload") {
            config.payload_size = std::stoul(next());
        } else if (a == "--distribution" || a == "-d") {
            config.distribution = loadgen::parse_distribution(next());
        } else if (a == "--warmup") {
            config.warmup_requests = std::stoull(next());
        } else if (a == "--raw") {
            config.use_binary_protocol = false;
        } else if (a == "--output" || a == "-o") {
            config.csv_output = next();
        } else if (a == "--json") {
            config.json_output = next();
        } else if (a == "--nodelay") {
            config.tcp_nodelay = (std::stoi(next()) != 0);
        } else if (a == "--timeout") {
            config.timeout_ms = std::stoi(next());
        } else if (a == "--verbose" || a == "-v") {
            config.verbose = true;
        } else if (a == "--help" || a == "-h") {
            std::cout << "Usage: load_generator [OPTIONS]\n\n"
                      << "Benchmark Options:\n"
                      << "  --host HOST             Target server host (default: 127.0.0.1)\n"
                      << "  --port PORT             Target server port (default: 9000)\n"
                      << "  --requests N            Total number of requests (default: 1000)\n"
                      << "  --rate RPS              Target arrival rate in req/s (default: 1000)\n"
                      << "  --threads, -t T         Worker thread count (default: 1)\n"
                      << "  --connections, -c C     Total concurrent TCP connections (default: threads)\n"
                      << "  --payload BYTES         Payload size per request in bytes (default: 64)\n"
                      << "  --distribution, -d DIST Arrival distribution: constant | poisson | burst (default: constant)\n"
                      << "  --warmup N              Number of initial requests discarded as warmup (default: 0)\n"
                      << "  --raw                   Send raw byte payload without binary framing\n"
                      << "  --output, -o FILE.csv   Export per-request CSV timeseries\n"
                      << "  --json FILE.json        Export summary metrics to JSON file\n"
                      << "  --nodelay 0|1           Enable TCP_NODELAY (default: 1)\n"
                      << "  --timeout MS            Per-request timeout in milliseconds (default: 5000)\n"
                      << "  --help, -h              Display this help message\n";
            return 0;
        } else {
            std::cerr << "[-] Unknown argument: " << a << " (use --help for options)\n";
            return 2;
        }
    }

    if (config.requests == 0 || config.rate <= 0.0 || config.payload_size == 0) {
        std::cerr << "[-] Error: requests, rate, and payload must be positive\n";
        return 2;
    }

    if (config.threads <= 0) config.threads = 1;
    if (config.connections <= 0) config.connections = config.threads;

    loadgen::generator::LoadGenerator generator(config);
    auto summary = generator.run();

    return summary.errors == 0 ? 0 : 1;
}
