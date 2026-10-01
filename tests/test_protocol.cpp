#include "protocol/framing.hpp"

#include <cassert>
#include <iostream>
#include <vector>

void test_frame_encoding_and_validation() {
    uint64_t seq = 4242;
    uint64_t ts = 1234567890ULL;
    std::vector<uint8_t> payload = {'H', 'E', 'L', 'L', 'O', '1', '2', '3'};

    auto frame = loadgen::protocol::build_frame(seq, ts, payload);
    assert(frame.size() == sizeof(loadgen::protocol::FrameHeader) + payload.size());

    const auto* hdr = reinterpret_cast<const loadgen::protocol::FrameHeader*>(frame.data());
    assert(loadgen::protocol::validate_header(*hdr));
    assert(hdr->sequence_id == seq);
    assert(hdr->client_timestamp_ns == ts);
    assert(hdr->payload_len == payload.size());

    // Verify payload bytes
    const uint8_t* payload_ptr = frame.data() + sizeof(loadgen::protocol::FrameHeader);
    for (std::size_t i = 0; i < payload.size(); ++i) {
        assert(payload_ptr[i] == payload[i]);
    }

    // Verify checksum validation
    uint32_t expected_chk = loadgen::protocol::calculate_checksum(*hdr, payload.data(), payload.size());
    assert(hdr->checksum == expected_chk);

    std::cout << "[PASS] test_frame_encoding_and_validation\n";
}

void test_corrupt_header() {
    loadgen::protocol::FrameHeader hdr;
    hdr.magic = 0xFF; // Invalid magic
    hdr.version = loadgen::protocol::PROTOCOL_VERSION;
    assert(!loadgen::protocol::validate_header(hdr));

    std::cout << "[PASS] test_corrupt_header\n";
}

int main() {
    std::cout << "Running Wire Protocol unit tests...\n";
    test_frame_encoding_and_validation();
    test_corrupt_header();
    std::cout << "All protocol tests passed successfully!\n";
    return 0;
}
