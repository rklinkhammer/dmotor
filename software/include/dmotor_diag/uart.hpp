#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace dmotor_diag {
constexpr std::size_t max_payload = 64, max_raw = 83, max_wire = 168;
struct Packet {
    uint8_t kind = 0;
    uint64_t run = 0;
    uint32_t sequence = 0;
    uint8_t length = 0;
    std::array<uint8_t, max_payload> payload{};
};
inline uint16_t crc(const uint8_t* p, std::size_t n) {
    uint16_t c = 0xffff;
    while (n--) {
        c = static_cast<uint16_t>(c ^ (static_cast<uint16_t>(*p++) << 8));
        for (unsigned b = 0; b < 8; ++b)
            c = static_cast<uint16_t>((c << 1) ^ ((c & 0x8000) ? 0x1021 : 0));
    }
    return c;
}
inline void put(uint8_t* p, uint64_t value, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
        p[i] = static_cast<uint8_t>(value >> (8 * i));
}
inline uint64_t get(const uint8_t* p, unsigned n) {
    uint64_t value = 0;
    for (unsigned i = 0; i < n; ++i)
        value |= uint64_t{p[i]} << (8 * i);
    return value;
}
inline Packet make(uint8_t kind, uint64_t run, uint32_t sequence) {
    Packet p;
    p.kind = kind;
    p.run = run;
    p.sequence = sequence;
    const uint8_t sizes[] = {0, 1, 2, 16, 64};
    p.length = sizes[(sequence - 1) % 5];
    for (unsigned i = 0; i < p.length; ++i) {
        const uint8_t special[] = {0, 255, 0x7d, 0x7e};
        p.payload[i] = i < 4 ? special[(i + kind - 1) % 4]
                             : static_cast<uint8_t>(sequence * 17U + i * 31U + kind * 73U);
    }
    return p;
}
inline bool pattern(const Packet& p) {
    if (!p.run || !p.sequence || p.sequence > 1000 || (p.kind != 1 && p.kind != 2))
        return false;
    const auto expected = make(p.kind, p.run, p.sequence);
    if (p.length != expected.length)
        return false;
    for (unsigned i = 0; i < p.length; ++i)
        if (p.payload[i] != expected.payload[i])
            return false;
    return true;
}
inline std::size_t encode(const Packet& p, std::array<uint8_t, max_wire>& out) {
    if (p.length > max_payload || (p.kind != 1 && p.kind != 2) || !p.run || !p.sequence)
        return 0;
    std::array<uint8_t, max_raw> raw{};
    raw[0] = 0xd7;
    raw[1] = 1;
    raw[2] = p.kind;
    put(raw.data() + 3, p.run, 8);
    put(raw.data() + 11, p.sequence, 4);
    raw[15] = p.length;
    for (unsigned i = 0; i < p.length; ++i)
        raw[16 + i] = p.payload[i];
    auto n = std::size_t{16} + p.length;
    const auto c = crc(raw.data(), n);
    raw[n++] = static_cast<uint8_t>(c >> 8);
    raw[n++] = static_cast<uint8_t>(c);
    std::size_t used = 0;
    out[used++] = 0x7e;
    for (std::size_t i = 0; i < n; ++i) {
        if (raw[i] == 0x7e || raw[i] == 0x7d) {
            out[used++] = 0x7d;
            out[used++] = raw[i] ^ 0x20;
        } else
            out[used++] = raw[i];
    }
    out[used++] = 0x7e;
    return used;
}
enum class Decode { incomplete, packet, rejected };
class Decoder {
  public:
    Decode push(uint8_t b, Packet& p) {
        if (b == 0x7e) {
            const bool had = used_ || bad_ || escape_;
            const bool good = started_ && !bad_ && !escape_ && unpack(p);
            started_ = true;
            used_ = 0;
            bad_ = escape_ = false;
            return good ? Decode::packet : had ? Decode::rejected : Decode::incomplete;
        }
        if (!started_ || bad_)
            return Decode::incomplete;
        if (escape_) {
            b ^= 0x20;
            escape_ = false;
        } else if (b == 0x7d) {
            escape_ = true;
            return Decode::incomplete;
        }
        if (used_ == raw_.size())
            bad_ = true;
        else
            raw_[used_++] = b;
        return Decode::incomplete;
    }

  private:
    bool unpack(Packet& p) const {
        if (used_ < 18 || raw_[0] != 0xd7 || raw_[1] != 1 || raw_[15] > max_payload ||
            used_ != std::size_t{18} + raw_[15])
            return false;
        if (crc(raw_.data(), used_ - 2) != (uint16_t(raw_[used_ - 2]) << 8 | raw_[used_ - 1]))
            return false;
        p = {};
        p.kind = raw_[2];
        p.run = get(raw_.data() + 3, 8);
        p.sequence = static_cast<uint32_t>(get(raw_.data() + 11, 4));
        p.length = raw_[15];
        for (unsigned i = 0; i < p.length; ++i)
            p.payload[i] = raw_[16 + i];
        return true;
    }
    std::array<uint8_t, max_raw> raw_{};
    std::size_t used_ = 0;
    bool started_ = false, bad_ = false, escape_ = false;
};
} // namespace dmotor_diag
