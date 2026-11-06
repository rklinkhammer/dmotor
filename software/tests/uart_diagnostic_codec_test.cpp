#include "dmotor_diag/uart.hpp"
#include "dmotor_sw/monitor_protocol.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __LINE__ << ": " #x "\n";                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
int main() {
    using namespace dmotor_diag;
    const uint8_t vector[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK(crc(vector, 9) == 0x29b1);
    for (uint8_t kind : {uint8_t{1}, uint8_t{2}})
        for (uint32_t seq = 1; seq <= 10; ++seq) {
            auto p = make(kind, 0x7e7d1234, seq);
            std::array<uint8_t, max_wire> bytes{};
            auto n = encode(p, bytes);
            CHECK(n && n <= max_wire);
            Decoder d;
            Packet out;
            Decode result = Decode::incomplete;
            dmotor_sw::monitor::Decoder operational;
            dmotor_sw::monitor::Frame frame;
            for (std::size_t i = 0; i < n; ++i) {
                result = d.push(bytes[i], out);
                CHECK(operational.push(bytes[i], frame) != dmotor_sw::monitor::Decode::frame);
            }
            CHECK(result == Decode::packet && pattern(out) && out.run == p.run && out.kind == kind);
            d.push(0x7e, out);
            for (unsigned i = 0; i < 300; ++i)
                d.push(1, out);
            CHECK(d.push(0x7e, out) == Decode::rejected);
            for (std::size_t i = 0; i < n; ++i)
                result = d.push(bytes[i], out);
            CHECK(result == Decode::packet);
        }
    std::cout
        << "diagnostic codec PASS: CRC golden vector, payload sizes, escaping, oversize/resync and operational rejection\n";
}
