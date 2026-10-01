#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace loadgen {
namespace protocol {

constexpr uint8_t PROTOCOL_MAGIC = 0x5A;
constexpr uint8_t PROTOCOL_VERSION = 1;

#pragma pack(push, 1)
struct FrameHeader {
    uint8_t magic{PROTOCOL_MAGIC};
    uint8_t version{PROTOCOL_VERSION};
    uint8_t flags{0};
    uint8_t reserved{0};
    uint64_t sequence_id{0};
    uint64_t client_timestamp_ns{0};
    uint32_t payload_len{0};
    uint32_t checksum{0};
};
#pragma pack(pop)

static_assert(sizeof(FrameHeader) == 28, "FrameHeader must be exactly 28 bytes");

inline uint32_t calculate_checksum(const FrameHeader& hdr, const uint8_t* payload, std::size_t payload_len) {
    uint32_t sum = hdr.magic ^ hdr.version ^ hdr.flags ^ static_cast<uint32_t>(hdr.sequence_id & 0xFFFFFFFF);
    sum += static_cast<uint32_t>(hdr.client_timestamp_ns & 0xFFFFFFFF);
    for (std::size_t i = 0; i < payload_len; ++i) {
        sum = (sum << 5) - sum + payload[i];
    }
    return sum;
}

inline bool validate_header(const FrameHeader& hdr) {
    return hdr.magic == PROTOCOL_MAGIC && hdr.version == PROTOCOL_VERSION;
}

inline std::vector<uint8_t> build_frame(uint64_t seq, uint64_t timestamp_ns, const std::vector<uint8_t>& payload) {
    FrameHeader hdr;
    hdr.magic = PROTOCOL_MAGIC;
    hdr.version = PROTOCOL_VERSION;
    hdr.flags = 0;
    hdr.reserved = 0;
    hdr.sequence_id = seq;
    hdr.client_timestamp_ns = timestamp_ns;
    hdr.payload_len = static_cast<uint32_t>(payload.size());
    hdr.checksum = calculate_checksum(hdr, payload.data(), payload.size());

    std::vector<uint8_t> frame(sizeof(FrameHeader) + payload.size());
    std::memcpy(frame.data(), &hdr, sizeof(FrameHeader));
    if (!payload.empty()) {
        std::memcpy(frame.data() + sizeof(FrameHeader), payload.data(), payload.size());
    }
    return frame;
}

} // namespace protocol
} // namespace loadgen
