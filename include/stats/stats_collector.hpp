#pragma once

#include "core/types.hpp"
#include "stats/hdr_histogram.hpp"

#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace loadgen {
namespace stats {

class StatsCollector {
public:
    StatsCollector() = default;

    void add_sample(const LatencySample& sample, bool is_warmup = false) {
        if (is_warmup) {
            warmup_count_++;
            return;
        }

        service_histogram_.record(sample.service_time_us);
        corrected_histogram_.record(sample.total_latency_us);
        schedule_delay_histogram_.record(sample.schedule_delay_us);

        // Keep raw samples for fine-grained export
        samples_.push_back(sample);
    }

    void record_error() {
        error_count_++;
    }

    void record_timeout() {
        timeout_count_++;
    }

    void merge(const StatsCollector& other) {
        service_histogram_.merge(other.service_histogram_);
        corrected_histogram_.merge(other.corrected_histogram_);
        schedule_delay_histogram_.merge(other.schedule_delay_histogram_);

        warmup_count_ += other.warmup_count_;
        error_count_ += other.error_count_;
        timeout_count_ += other.timeout_count_;

        samples_.insert(samples_.end(), other.samples_.begin(), other.samples_.end());
    }

    SummaryStats compute_summary(uint64_t total_scheduled, double elapsed_seconds, std::size_t payload_bytes) const {
        SummaryStats s;
        s.total_scheduled = total_scheduled;
        s.completed = service_histogram_.count();
        s.warmup_completed = warmup_count_;
        s.errors = error_count_;
        s.timeouts = timeout_count_;
        s.total_elapsed_sec = elapsed_seconds;

        if (elapsed_seconds > 0.0) {
            s.throughput_rps = static_cast<double>(s.completed) / elapsed_seconds;
            double total_bytes = static_cast<double>(s.completed) * static_cast<double>(payload_bytes);
            s.throughput_mbps = (total_bytes * 8.0) / (elapsed_seconds * 1e6);
        }

        s.min_us = service_histogram_.min();
        s.max_us = service_histogram_.max();
        s.mean_us = service_histogram_.mean();
        s.stddev_us = service_histogram_.stddev();

        s.p50_us = service_histogram_.value_at_percentile(50.0);
        s.p75_us = service_histogram_.value_at_percentile(75.0);
        s.p90_us = service_histogram_.value_at_percentile(90.0);
        s.p95_us = service_histogram_.value_at_percentile(95.0);
        s.p99_us = service_histogram_.value_at_percentile(99.0);
        s.p999_us = service_histogram_.value_at_percentile(99.9);

        s.corrected_p50_us = corrected_histogram_.value_at_percentile(50.0);
        s.corrected_p95_us = corrected_histogram_.value_at_percentile(95.0);
        s.corrected_p99_us = corrected_histogram_.value_at_percentile(99.0);
        s.corrected_max_us = corrected_histogram_.max();

        s.mean_schedule_delay_us = schedule_delay_histogram_.mean();
        s.max_schedule_delay_us = schedule_delay_histogram_.max();

        return s;
    }

    bool write_csv(const std::string& path) const {
        std::ofstream out(path);
        if (!out.is_open()) return false;

        out << "seq,scheduled_ns,sent_ns,received_ns,service_time_us,schedule_delay_us,total_latency_us\n";
        for (const auto& sample : samples_) {
            out << sample.sequence_id << ","
                << sample.scheduled_time_ns << ","
                << sample.sent_time_ns << ","
                << sample.received_time_ns << ","
                << std::fixed << std::setprecision(3)
                << sample.service_time_us << ","
                << sample.schedule_delay_us << ","
                << sample.total_latency_us << "\n";
        }
        return true;
    }

    bool write_json(const std::string& path, const SummaryStats& s, const GeneratorConfig& cfg) const {
        std::ofstream out(path);
        if (!out.is_open()) return false;

        out << std::fixed << std::setprecision(3);
        out << "{\n"
            << "  \"config\": {\n"
            << "    \"host\": \"" << cfg.host << "\",\n"
            << "    \"port\": " << cfg.port << ",\n"
            << "    \"target_requests\": " << cfg.requests << ",\n"
            << "    \"target_rate_rps\": " << cfg.rate << ",\n"
            << "    \"threads\": " << cfg.threads << ",\n"
            << "    \"connections\": " << cfg.connections << ",\n"
            << "    \"payload_bytes\": " << cfg.payload_size << ",\n"
            << "    \"distribution\": \"" << to_string(cfg.distribution) << "\",\n"
            << "    \"tcp_nodelay\": " << (cfg.tcp_nodelay ? "true" : "false") << "\n"
            << "  },\n"
            << "  \"results\": {\n"
            << "    \"completed\": " << s.completed << ",\n"
            << "    \"errors\": " << s.errors << ",\n"
            << "    \"timeouts\": " << s.timeouts << ",\n"
            << "    \"elapsed_seconds\": " << s.total_elapsed_sec << ",\n"
            << "    \"throughput_rps\": " << s.throughput_rps << ",\n"
            << "    \"throughput_mbps\": " << s.throughput_mbps << ",\n"
            << "    \"service_latency_us\": {\n"
            << "      \"min\": " << s.min_us << ",\n"
            << "      \"mean\": " << s.mean_us << ",\n"
            << "      \"stddev\": " << s.stddev_us << ",\n"
            << "      \"p50\": " << s.p50_us << ",\n"
            << "      \"p75\": " << s.p75_us << ",\n"
            << "      \"p90\": " << s.p90_us << ",\n"
            << "      \"p95\": " << s.p95_us << ",\n"
            << "      \"p99\": " << s.p99_us << ",\n"
            << "      \"p999\": " << s.p999_us << ",\n"
            << "      \"max\": " << s.max_us << "\n"
            << "    },\n"
            << "    \"coordinated_omission_corrected_us\": {\n"
            << "      \"p50\": " << s.corrected_p50_us << ",\n"
            << "      \"p95\": " << s.corrected_p95_us << ",\n"
            << "      \"p99\": " << s.corrected_p99_us << ",\n"
            << "      \"max\": " << s.corrected_max_us << ",\n"
            << "      \"mean_schedule_delay_us\": " << s.mean_schedule_delay_us << ",\n"
            << "      \"max_schedule_delay_us\": " << s.max_schedule_delay_us << "\n"
            << "    }\n"
            << "  }\n"
            << "}\n";
        return true;
    }

    const std::vector<LatencySample>& samples() const { return samples_; }
    const LatencyHistogram& service_histogram() const { return service_histogram_; }

private:
    LatencyHistogram service_histogram_;
    LatencyHistogram corrected_histogram_;
    LatencyHistogram schedule_delay_histogram_;
    std::vector<LatencySample> samples_;
    uint64_t warmup_count_{0};
    uint64_t error_count_{0};
    uint64_t timeout_count_{0};
};

} // namespace stats
} // namespace loadgen
