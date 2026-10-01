#pragma once

#include "core/socket_utils.hpp"
#include "core/types.hpp"
#include "generator/distribution.hpp"
#include "protocol/framing.hpp"
#include "stats/stats_collector.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace loadgen {
namespace generator {

using Clock = std::chrono::steady_clock;

class Worker {
public:
    Worker(int worker_id,
           GeneratorConfig config,
           uint64_t request_offset,
           uint64_t num_requests,
           double worker_rate,
           int num_connections)
        : worker_id_(worker_id),
          config_(std::move(config)),
          request_offset_(request_offset),
          num_requests_(num_requests),
          worker_rate_(worker_rate),
          num_connections_(num_connections > 0 ? num_connections : 1) {}

    ~Worker() {
        close_connections();
    }

    bool init_connections() {
        close_connections();
        connection_fds_.reserve(num_connections_);
        for (int i = 0; i < num_connections_; ++i) {
            int fd = socket_utils::create_tcp_client(
                config_.host, config_.port, config_.tcp_nodelay, config_.timeout_ms);
            if (fd < 0) {
                close_connections();
                return false;
            }
            connection_fds_.push_back(fd);
        }
        return true;
    }

    void run(Clock::time_point benchmark_start_time) {
        if (connection_fds_.empty()) {
            if (!init_connections()) {
                for (uint64_t i = 0; i < num_requests_; ++i) {
                    collector_.record_error();
                }
                return;
            }
        }

        auto dist = create_distribution(
            config_.distribution, worker_rate_, static_cast<uint64_t>(1337 + worker_id_ * 37));

        std::vector<uint8_t> payload_data(config_.payload_size, static_cast<uint8_t>('A' + (worker_id_ % 26)));
        std::vector<uint8_t> recv_buf(
            config_.use_binary_protocol ? (sizeof(protocol::FrameHeader) + config_.payload_size)
                                       : config_.payload_size);

        auto scheduled_time = benchmark_start_time;
        std::size_t conn_idx = 0;

        for (uint64_t req_idx = 0; req_idx < num_requests_; ++req_idx) {
            uint64_t global_seq = request_offset_ + req_idx;
            bool is_warmup = (global_seq < config_.warmup_requests);

            scheduled_time += dist->next_interval();

            // High-precision open-loop wait
            precise_sleep_until(scheduled_time);

            auto t_sent = Clock::now();
            uint64_t scheduled_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                scheduled_time.time_since_epoch()).count();
            uint64_t sent_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                t_sent.time_since_epoch()).count();

            int fd = connection_fds_[conn_idx % connection_fds_.size()];
            conn_idx++;

            bool success = false;
            if (config_.use_binary_protocol) {
                auto frame = protocol::build_frame(global_seq, sent_ns, payload_data);
                if (socket_utils::send_exact(fd, frame.data(), frame.size())) {
                    if (socket_utils::recv_exact(fd, recv_buf.data(), recv_buf.size())) {
                        const auto* hdr = reinterpret_cast<const protocol::FrameHeader*>(recv_buf.data());
                        if (protocol::validate_header(*hdr) && hdr->sequence_id == global_seq) {
                            success = true;
                        }
                    }
                }
            } else {
                // Raw byte echo
                if (socket_utils::send_exact(fd, payload_data.data(), payload_data.size())) {
                    if (socket_utils::recv_exact(fd, recv_buf.data(), recv_buf.size())) {
                        if (recv_buf == payload_data) {
                            success = true;
                        }
                    }
                }
            }

            auto t_recv = Clock::now();
            uint64_t recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                t_recv.time_since_epoch()).count();

            if (success) {
                LatencySample sample;
                sample.sequence_id = global_seq;
                sample.scheduled_time_ns = scheduled_ns;
                sample.sent_time_ns = sent_ns;
                sample.received_time_ns = recv_ns;

                double service_us = std::chrono::duration<double, std::micro>(t_recv - t_sent).count();
                double schedule_delay_us = (sent_ns > scheduled_ns)
                    ? static_cast<double>(sent_ns - scheduled_ns) / 1000.0
                    : 0.0;
                double total_latency_us = (recv_ns > scheduled_ns)
                    ? static_cast<double>(recv_ns - scheduled_ns) / 1000.0
                    : service_us;

                sample.service_time_us = service_us;
                sample.schedule_delay_us = schedule_delay_us;
                sample.total_latency_us = total_latency_us;

                collector_.add_sample(sample, is_warmup);
            } else {
                collector_.record_error();
                // Attempt to reconnect this socket if broken
                close(fd);
                int new_fd = socket_utils::create_tcp_client(
                    config_.host, config_.port, config_.tcp_nodelay, config_.timeout_ms);
                connection_fds_[(conn_idx - 1) % connection_fds_.size()] = new_fd;
            }
        }
    }

    const stats::StatsCollector& collector() const { return collector_; }

private:
    void close_connections() {
        for (int fd : connection_fds_) {
            if (fd >= 0) close(fd);
        }
        connection_fds_.clear();
    }

    // Hybrid sleep: kernel sleep until 50 microseconds before deadline, then spin-wait
    static void precise_sleep_until(Clock::time_point target) {
        auto now = Clock::now();
        if (now >= target) return;

        constexpr auto spin_margin = std::chrono::microseconds(50);
        if (target - now > spin_margin) {
            std::this_thread::sleep_until(target - spin_margin);
        }
        while (Clock::now() < target) {
#if defined(__x86_64__) || defined(_M_X64)
            __builtin_ia32_pause();
#elif defined(__aarch64__)
            asm volatile("yield");
#else
            std::this_thread::yield();
#endif
        }
    }

    int worker_id_;
    GeneratorConfig config_;
    uint64_t request_offset_;
    uint64_t num_requests_;
    double worker_rate_;
    int num_connections_;
    std::vector<int> connection_fds_;
    stats::StatsCollector collector_;
};

} // namespace generator
} // namespace loadgen
