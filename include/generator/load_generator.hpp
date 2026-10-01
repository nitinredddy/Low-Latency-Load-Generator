#pragma once

#include "core/types.hpp"
#include "generator/worker.hpp"
#include "stats/stats_collector.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace loadgen {
namespace generator {

class LoadGenerator {
public:
    explicit LoadGenerator(GeneratorConfig config)
        : config_(std::move(config)) {
        if (config_.threads <= 0) config_.threads = 1;
        if (config_.connections < config_.threads) config_.connections = config_.threads;
    }

    SummaryStats run() {
        print_banner();

        int num_threads = config_.threads;
        uint64_t total_reqs = config_.requests;
        uint64_t reqs_per_thread = total_reqs / num_threads;
        uint64_t remainder = total_reqs % num_threads;
        int conns_per_thread = config_.connections / num_threads;
        int conn_remainder = config_.connections % num_threads;

        std::vector<std::unique_ptr<Worker>> workers;
        workers.reserve(num_threads);

        uint64_t current_offset = 0;
        double rate_per_thread = config_.rate / num_threads;

        for (int i = 0; i < num_threads; ++i) {
            uint64_t t_reqs = reqs_per_thread + (i < static_cast<int>(remainder) ? 1 : 0);
            int t_conns = conns_per_thread + (i < conn_remainder ? 1 : 0);
            workers.push_back(std::make_unique<Worker>(
                i, config_, current_offset, t_reqs, rate_per_thread, t_conns
            ));
            current_offset += t_reqs;
        }

        // Initialize connections
        std::cout << "[*] Establishing " << config_.connections << " TCP connection(s) to "
                  << config_.host << ":" << config_.port << " across " << num_threads << " thread(s)..." << std::endl;

        for (auto& w : workers) {
            if (!w->init_connections()) {
                std::cerr << "[-] Error: Failed to connect to server at "
                          << config_.host << ":" << config_.port << "\n";
                stats::StatsCollector empty_coll;
                empty_coll.record_error();
                return empty_coll.compute_summary(total_reqs, 0.0, config_.payload_size);
            }
        }

        std::cout << "[*] Benchmark started. Dispatching " << total_reqs << " requests at "
                  << config_.rate << " target RPS (" << to_string(config_.distribution) << " distribution)...\n"
                  << std::endl;

        auto benchmark_start = Clock::now();
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&workers, i, benchmark_start]() {
                workers[i]->run(benchmark_start);
            });
        }

        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }

        auto benchmark_end = Clock::now();
        double elapsed_sec = std::chrono::duration<double>(benchmark_end - benchmark_start).count();

        // Merge stats from all workers
        stats::StatsCollector global_collector;
        for (const auto& w : workers) {
            global_collector.merge(w->collector());
        }

        SummaryStats stats = global_collector.compute_summary(total_reqs, elapsed_sec, config_.payload_size);
        print_results(stats);

        if (!config_.csv_output.empty()) {
            if (global_collector.write_csv(config_.csv_output)) {
                std::cout << "[+] Per-request timeseries exported to: " << config_.csv_output << std::endl;
            } else {
                std::cerr << "[-] Failed to write CSV output to: " << config_.csv_output << std::endl;
            }
        }

        if (!config_.json_output.empty()) {
            if (global_collector.write_json(config_.json_output, stats, config_)) {
                std::cout << "[+] Benchmark summary JSON exported to: " << config_.json_output << std::endl;
            } else {
                std::cerr << "[-] Failed to write JSON output to: " << config_.json_output << std::endl;
            }
        }

        return stats;
    }

private:
    void print_banner() const {
        std::cout << "===================================================================\n"
                  << "        LOW-LATENCY LOAD GENERATOR (C++17 Open-Loop Engine)        \n"
                  << "===================================================================\n"
                  << " Target:         " << config_.host << ":" << config_.port << "\n"
                  << " Planned Load:   " << config_.requests << " requests @ " << config_.rate << " RPS\n"
                  << " Workers:        " << config_.threads << " thread(s) with "
                  << config_.connections << " total connection(s)\n"
                  << " Arrival Model:  " << to_string(config_.distribution) << "\n"
                  << " Payload:        " << config_.payload_size << " bytes per request\n"
                  << " Socket Options: TCP_NODELAY=" << (config_.tcp_nodelay ? "ON" : "OFF") << "\n"
                  << " Protocol:       " << (config_.use_binary_protocol ? "Binary Framed (Timestamped)" : "Raw Echo") << "\n"
                  << "===================================================================\n"
                  << std::endl;
    }

    void print_results(const SummaryStats& s) const {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n======================== BENCHMARK RESULTS ========================\n"
                  << " Total Scheduled:   " << s.total_scheduled << "\n"
                  << " Completed:         " << s.completed << " (Warmup discarded: " << s.warmup_completed << ")\n"
                  << " Errors:            " << s.errors << "\n"
                  << " Elapsed Time:      " << s.total_elapsed_sec << " s\n"
                  << " Actual Throughput: " << s.throughput_rps << " req/s (" << s.throughput_mbps << " Mbit/s)\n"
                  << "-------------------------------------------------------------------\n"
                  << " Latency Percentiles (Service Time: Send -> Recv):\n"
                  << "   Min:             " << std::setw(10) << s.min_us << " us (" << (s.min_us / 1000.0) << " ms)\n"
                  << "   p50 (Median):    " << std::setw(10) << s.p50_us << " us (" << (s.p50_us / 1000.0) << " ms)\n"
                  << "   p75:             " << std::setw(10) << s.p75_us << " us (" << (s.p75_us / 1000.0) << " ms)\n"
                  << "   p90:             " << std::setw(10) << s.p90_us << " us (" << (s.p90_us / 1000.0) << " ms)\n"
                  << "   p95:             " << std::setw(10) << s.p95_us << " us (" << (s.p95_us / 1000.0) << " ms)\n"
                  << "   p99:             " << std::setw(10) << s.p99_us << " us (" << (s.p99_us / 1000.0) << " ms)\n"
                  << "   p99.9:           " << std::setw(10) << s.p999_us << " us (" << (s.p999_us / 1000.0) << " ms)\n"
                  << "   Max:             " << std::setw(10) << s.max_us << " us (" << (s.max_us / 1000.0) << " ms)\n"
                  << "   Mean:            " << std::setw(10) << s.mean_us << " us\n"
                  << "   Std Dev:         " << std::setw(10) << s.stddev_us << " us\n";

        if (s.max_schedule_delay_us > 100.0) {
            std::cout << "-------------------------------------------------------------------\n"
                      << " [!] Coordinated Omission Corrected Latency (Schedule -> Recv):\n"
                      << "   Mean Sched Delay:" << std::setw(10) << s.mean_schedule_delay_us << " us\n"
                      << "   Max Sched Delay: " << std::setw(10) << s.max_schedule_delay_us << " us\n"
                      << "   Corrected p50:   " << std::setw(10) << s.corrected_p50_us << " us\n"
                      << "   Corrected p95:   " << std::setw(10) << s.corrected_p95_us << " us\n"
                      << "   Corrected p99:   " << std::setw(10) << s.corrected_p99_us << " us\n"
                      << "   Corrected Max:   " << std::setw(10) << s.corrected_max_us << " us\n";
        }

        std::cout << "===================================================================\n";

        // Machine-parsable key-values for backwards compatibility with existing smoke test scripts
        std::cout << "\n# Raw Metrics (Machine-readable):\n"
                  << "requests=" << s.total_scheduled << "\n"
                  << "completed=" << s.completed << "\n"
                  << "errors=" << s.errors << "\n"
                  << "throughput_rps=" << s.throughput_rps << "\n"
                  << "p50_us=" << s.p50_us << "\n"
                  << "p95_us=" << s.p95_us << "\n"
                  << "p99_us=" << s.p99_us << "\n"
                  << "max_us=" << s.max_us << "\n";
    }

    GeneratorConfig config_;
};

} // namespace generator
} // namespace loadgen
