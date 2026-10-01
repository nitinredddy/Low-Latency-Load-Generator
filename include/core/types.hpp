#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace loadgen {

enum class DistributionType {
    Constant,
    Poisson,
    Bursty
};

inline std::string to_string(DistributionType dist) {
    switch (dist) {
        case DistributionType::Constant: return "constant";
        case DistributionType::Poisson:  return "poisson";
        case DistributionType::Bursty:   return "bursty";
    }
    return "unknown";
}

inline DistributionType parse_distribution(const std::string& str) {
    if (str == "poisson") return DistributionType::Poisson;
    if (str == "bursty" || str == "burst") return DistributionType::Bursty;
    return DistributionType::Constant;
}

struct GeneratorConfig {
    std::string host = "127.0.0.1";
    int port = 9000;
    uint64_t requests = 1000;
    double rate = 1000.0;                 // Target requests per second
    int threads = 1;                      // Worker threads
    int connections = 1;                  // Total TCP connections
    std::size_t payload_size = 64;        // Payload size in bytes
    DistributionType distribution = DistributionType::Constant;
    uint64_t warmup_requests = 0;         // Number of warmup requests to discard
    bool use_binary_protocol = true;      // Framed binary protocol with timestamps
    bool tcp_nodelay = true;              // Disable Nagle's algorithm
    int timeout_ms = 5000;                // Socket timeout
    std::string csv_output;               // Path to per-request CSV output
    std::string json_output;              // Path to summary JSON output
    bool verbose = false;
};

struct ServerConfig {
    int port = 9000;
    std::string bind_address = "127.0.0.1";
    int backlog = 1024;
    bool tcp_nodelay = true;
    int buffer_size_kb = 64;
    int workers = 1;
};

struct LatencySample {
    uint64_t sequence_id{0};
    uint64_t scheduled_time_ns{0};
    uint64_t sent_time_ns{0};
    uint64_t received_time_ns{0};
    double service_time_us{0.0};  // sent -> recv
    double schedule_delay_us{0.0};// scheduled -> sent
    double total_latency_us{0.0}; // scheduled -> recv (coordinated-omission corrected)
};

struct SummaryStats {
    uint64_t total_scheduled{0};
    uint64_t completed{0};
    uint64_t warmup_completed{0};
    uint64_t errors{0};
    uint64_t timeouts{0};
    double total_elapsed_sec{0.0};
    double throughput_rps{0.0};
    double throughput_mbps{0.0};

    // Service latency percentiles (microseconds)
    double p50_us{0.0};
    double p75_us{0.0};
    double p90_us{0.0};
    double p95_us{0.0};
    double p99_us{0.0};
    double p999_us{0.0};
    double min_us{0.0};
    double max_us{0.0};
    double mean_us{0.0};
    double stddev_us{0.0};

    // Corrected latency percentiles (including schedule delay)
    double corrected_p50_us{0.0};
    double corrected_p95_us{0.0};
    double corrected_p99_us{0.0};
    double corrected_max_us{0.0};
    double mean_schedule_delay_us{0.0};
    double max_schedule_delay_us{0.0};
};

} // namespace loadgen
