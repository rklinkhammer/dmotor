#include "dmotor_sw/monitor_protocol.hpp"
#include <algorithm>
namespace dmotor_sw {
namespace monitor {
void put_le(uint8_t* p, uint64_t value, std::size_t bytes) {
    for (std::size_t i = 0; i < bytes; ++i) {
        p[i] = static_cast<uint8_t>(value);
        value >>= 8;
    }
}
uint64_t get_le(const uint8_t* p, std::size_t bytes) {
    uint64_t value = 0;
    for (std::size_t i = 0; i < bytes; ++i)
        value |= uint64_t{p[i]} << (8 * i);
    return value;
}
uint16_t crc16(const uint8_t* p, std::size_t length) {
    uint16_t crc = 0xffff;
    for (std::size_t i = 0; i < length; ++i) {
        crc = static_cast<uint16_t>(crc ^ (uint16_t{p[i]} << 8));
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = static_cast<uint16_t>((crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
    }
    return crc;
}
bool valid(const Frame& f) {
    return f.source && f.sequence && f.length <= max_payload && f.length == payload_size(f.type) &&
           (f.type == Type::hello ? f.peer == 0 && !f.epoch && !f.transaction : f.peer != 0);
}
std::size_t encode(const Frame& f, std::array<uint8_t, max_encoded>& output) {
    if (!valid(f))
        return 0;
    std::array<uint8_t, max_decoded> raw{};
    raw[0] = version;
    put_le(&raw[1], f.source, 8);
    put_le(&raw[9], f.peer, 8);
    put_le(&raw[17], f.epoch, 4);
    put_le(&raw[21], f.transaction, 4);
    put_le(&raw[25], f.sequence, 4);
    raw[29] = static_cast<uint8_t>(f.type);
    raw[30] = f.length;
    std::copy_n(f.payload.begin(), f.length, raw.begin() + 31);
    const auto end = std::size_t{31} + f.length;
    const auto crc = crc16(raw.data(), end);
    raw[end] = static_cast<uint8_t>(crc >> 8);
    raw[end + 1] = static_cast<uint8_t>(crc);
    std::size_t n = 0;
    output[n++] = 0x7e;
    for (std::size_t i = 0; i < end + 2; ++i) {
        if (raw[i] == 0x7e || raw[i] == 0x7d) {
            output[n++] = 0x7d;
            output[n++] = raw[i] ^ 0x20U;
        } else
            output[n++] = raw[i];
    }
    output[n++] = 0x7e;
    return n;
}
void Decoder::reset() {
    used_ = 0;
    active_ = escaped_ = discard_ = false;
}
Decode Decoder::push(uint8_t b, Frame& output) {
    if (b == 0x7e) {
        Decode result = Decode::incomplete;
        if (active_ && (used_ || escaped_ || discard_)) {
            result = Decode::rejected;
            if (!escaped_ && !discard_ && used_ >= 33 && bytes_[0] == version &&
                bytes_[30] <= max_payload && used_ == std::size_t{33} + bytes_[30] &&
                crc16(bytes_.data(), used_ - 2) ==
                    ((uint16_t{bytes_[used_ - 2]} << 8) | bytes_[used_ - 1])) {
                Frame f;
                f.source = get_le(&bytes_[1], 8);
                f.peer = get_le(&bytes_[9], 8);
                f.epoch = static_cast<uint32_t>(get_le(&bytes_[17], 4));
                f.transaction = static_cast<uint32_t>(get_le(&bytes_[21], 4));
                f.sequence = static_cast<uint32_t>(get_le(&bytes_[25], 4));
                f.type = static_cast<Type>(bytes_[29]);
                f.length = bytes_[30];
                std::copy_n(bytes_.begin() + 31, f.length, f.payload.begin());
                if (valid(f)) {
                    output = f;
                    result = Decode::frame;
                }
            }
        }
        used_ = 0;
        active_ = true;
        escaped_ = discard_ = false;
        return result;
    }
    if (!active_ || discard_)
        return Decode::incomplete;
    if (escaped_) {
        escaped_ = false;
        if (b != 0x5e && b != 0x5d) {
            discard_ = true;
            return Decode::incomplete;
        }
        b ^= 0x20;
    } else if (b == 0x7d) {
        escaped_ = true;
        return Decode::incomplete;
    }
    if (used_ == bytes_.size())
        discard_ = true;
    else
        bytes_[used_++] = b;
    return Decode::incomplete;
}
} // namespace monitor
} // namespace dmotor_sw
