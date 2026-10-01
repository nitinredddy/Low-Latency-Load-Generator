#include "stats/hdr_histogram.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

void test_basic_percentiles() {
    loadgen::stats::LatencyHistogram hist;
    
    // Insert 1000 samples linearly from 100 us to 1099 us
    for (int i = 0; i < 1000; ++i) {
        hist.record(100.0 + i);
    }

    assert(hist.count() == 1000);
    assert(std::abs(hist.min() - 100.0) < 5.0);
    assert(std::abs(hist.max() - 1099.0) < 15.0);

    // p50 should be around ~600 us (allow small log-bin quantization tolerance)
    double p50 = hist.value_at_percentile(50.0);
    assert(p50 >= 550.0 && p50 <= 650.0);

    // p90 should be around ~1000 us
    double p90 = hist.value_at_percentile(90.0);
    assert(p90 >= 950.0 && p90 <= 1050.0);

    std::cout << "[PASS] test_basic_percentiles (p50=" << p50 << " us, p90=" << p90 << " us)\n";
}

void test_merge() {
    loadgen::stats::LatencyHistogram h1;
    loadgen::stats::LatencyHistogram h2;

    for (int i = 0; i < 500; ++i) {
        h1.record(200.0);
    }
    for (int i = 0; i < 500; ++i) {
        h2.record(800.0);
    }

    h1.merge(h2);
    assert(h1.count() == 1000);

    double p50 = h1.value_at_percentile(50.0);
    assert(p50 >= 190.0 && p50 <= 810.0);

    std::cout << "[PASS] test_merge\n";
}

int main() {
    std::cout << "Running LatencyHistogram unit tests...\n";
    test_basic_percentiles();
    test_merge();
    std::cout << "All histogram tests passed successfully!\n";
    return 0;
}
