#pragma once

#include "core/types.hpp"

#include <chrono>
#include <cmath>
#include <memory>
#include <random>

namespace loadgen {
namespace generator {

class InterArrivalDistribution {
public:
    virtual ~InterArrivalDistribution() = default;
    virtual std::chrono::nanoseconds next_interval() = 0;
};

class ConstantDistribution : public InterArrivalDistribution {
public:
    explicit ConstantDistribution(double rate_per_second) {
        if (rate_per_second <= 0.0) rate_per_second = 1.0;
        double interval_sec = 1.0 / rate_per_second;
        interval_ns_ = std::chrono::nanoseconds(static_cast<int64_t>(interval_sec * 1e9));
    }

    std::chrono::nanoseconds next_interval() override {
        return interval_ns_;
    }

private:
    std::chrono::nanoseconds interval_ns_{0};
};

class PoissonDistribution : public InterArrivalDistribution {
public:
    PoissonDistribution(double rate_per_second, uint64_t seed)
        : rng_(seed), exp_dist_(rate_per_second > 0.0 ? rate_per_second : 1.0) {}

    std::chrono::nanoseconds next_interval() override {
        double interval_sec = exp_dist_(rng_);
        return std::chrono::nanoseconds(static_cast<int64_t>(interval_sec * 1e9));
    }

private:
    std::mt19937_64 rng_;
    std::exponential_distribution<double> exp_dist_;
};

class BurstyDistribution : public InterArrivalDistribution {
public:
    BurstyDistribution(double base_rate_per_second, uint64_t seed)
        : rng_(seed), uniform_dist_(0.0, 1.0),
          normal_interval_ns_(static_cast<int64_t>((1.0 / base_rate_per_second) * 1e9)),
          burst_interval_ns_(static_cast<int64_t>((1.0 / (base_rate_per_second * 8.0)) * 1e9)) {}

    std::chrono::nanoseconds next_interval() override {
        // 15% probability of initiating or being in a high-speed microburst
        bool is_burst = uniform_dist_(rng_) < 0.15;
        return is_burst ? burst_interval_ns_ : normal_interval_ns_;
    }

private:
    std::mt19937_64 rng_;
    std::uniform_real_distribution<double> uniform_dist_;
    std::chrono::nanoseconds normal_interval_ns_;
    std::chrono::nanoseconds burst_interval_ns_;
};

inline std::unique_ptr<InterArrivalDistribution> create_distribution(
    DistributionType type, double rate, uint64_t seed = 42) {
    switch (type) {
        case DistributionType::Poisson:
            return std::make_unique<PoissonDistribution>(rate, seed);
        case DistributionType::Bursty:
            return std::make_unique<BurstyDistribution>(rate, seed);
        case DistributionType::Constant:
        default:
            return std::make_unique<ConstantDistribution>(rate);
    }
}

} // namespace generator
} // namespace loadgen
