#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace loadgen {
namespace stats {

class LatencyHistogram {
public:
    // Tracks latencies in microseconds from 0.1 us up to 10 seconds (10,000,000 us)
    static constexpr double MIN_VAL_US = 0.1;
    static constexpr double MAX_VAL_US = 10000000.0; // 10s
    static constexpr std::size_t NUM_BUCKETS = 10000;

    LatencyHistogram() {
        reset();
    }

    void reset() {
        buckets_.assign(NUM_BUCKETS + 1, 0);
        count_ = 0;
        min_us_ = 1e12;
        max_us_ = 0.0;
        sum_us_ = 0.0;
        sum_squares_ = 0.0;
    }

    void record(double latency_us) {
        if (latency_us < 0.0) latency_us = 0.0;
        
        std::size_t b = value_to_bucket(latency_us);
        buckets_[b]++;
        count_++;

        if (latency_us < min_us_) min_us_ = latency_us;
        if (latency_us > max_us_) max_us_ = latency_us;
        sum_us_ += latency_us;
        sum_squares_ += (latency_us * latency_us);
    }

    void merge(const LatencyHistogram& other) {
        if (other.count_ == 0) return;
        for (std::size_t i = 0; i <= NUM_BUCKETS; ++i) {
            buckets_[i] += other.buckets_[i];
        }
        count_ += other.count_;
        if (other.min_us_ < min_us_) min_us_ = other.min_us_;
        if (other.max_us_ > max_us_) max_us_ = other.max_us_;
        sum_us_ += other.sum_us_;
        sum_squares_ += other.sum_squares_;
    }

    uint64_t count() const { return count_; }
    double min() const { return count_ > 0 ? min_us_ : 0.0; }
    double max() const { return count_ > 0 ? max_us_ : 0.0; }
    double mean() const { return count_ > 0 ? (sum_us_ / count_) : 0.0; }

    double stddev() const {
        if (count_ <= 1) return 0.0;
        double m = mean();
        double variance = (sum_squares_ / count_) - (m * m);
        return variance > 0.0 ? std::sqrt(variance) : 0.0;
    }

    double value_at_percentile(double percentile) const {
        if (count_ == 0) return 0.0;
        if (percentile <= 0.0) return min();
        if (percentile >= 100.0) return max();

        uint64_t target_count = static_cast<uint64_t>(
            std::ceil((percentile / 100.0) * static_cast<double>(count_))
        );
        if (target_count == 0) target_count = 1;

        uint64_t accumulated = 0;
        for (std::size_t i = 0; i <= NUM_BUCKETS; ++i) {
            accumulated += buckets_[i];
            if (accumulated >= target_count) {
                return bucket_to_value(i);
            }
        }
        return max();
    }

    struct PercentilePoint {
        double percentile;
        double value_us;
        uint64_t count;
    };

    std::vector<PercentilePoint> get_percentile_distribution() const {
        std::vector<double> percentiles = {
            0.0, 25.0, 50.0, 75.0, 90.0, 95.0, 99.0, 99.9, 99.99, 100.0
        };
        std::vector<PercentilePoint> result;
        for (double p : percentiles) {
            result.push_back({p, value_at_percentile(p), count_});
        }
        return result;
    }

private:
    std::vector<uint64_t> buckets_;
    uint64_t count_{0};
    double min_us_{1e12};
    double max_us_{0.0};
    double sum_us_{0.0};
    double sum_squares_{0.0};

    // Logarithmic compression bucket mapping for wide dynamic range with high resolution
    static std::size_t value_to_bucket(double val_us) {
        if (val_us <= MIN_VAL_US) return 0;
        if (val_us >= MAX_VAL_US) return NUM_BUCKETS;

        // Map log10(val / min) to bucket range [0, NUM_BUCKETS]
        double log_min = std::log10(MIN_VAL_US);
        double log_max = std::log10(MAX_VAL_US);
        double log_val = std::log10(val_us);
        
        double norm = (log_val - log_min) / (log_max - log_min);
        std::size_t b = static_cast<std::size_t>(norm * (NUM_BUCKETS - 1));
        return std::min(b, NUM_BUCKETS - 1);
    }

    static double bucket_to_value(std::size_t bucket) {
        if (bucket == 0) return MIN_VAL_US;
        if (bucket >= NUM_BUCKETS) return MAX_VAL_US;

        double log_min = std::log10(MIN_VAL_US);
        double log_max = std::log10(MAX_VAL_US);
        double norm = static_cast<double>(bucket) / (NUM_BUCKETS - 1);
        double log_val = log_min + norm * (log_max - log_min);
        return std::pow(10.0, log_val);
    }
};

} // namespace stats
} // namespace loadgen
