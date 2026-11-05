#include "dmotor_sw/monitor_protocol.hpp"
#include <cstdlib>
#include <iostream>
using namespace dmotor_sw::monitor;
static void check(bool good) {
    if (!good)
        std::abort();
}
static Decode feed(Decoder& d, const uint8_t* p, std::size_t n, Frame& f) {
    Decode result = Decode::incomplete;
    for (std::size_t i = 0; i < n; ++i)
        result = d.push(p[i], f);
    return result;
}
int main() {
    const uint8_t text[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    check(crc16(text, 9) == 0x29b1);
    Decoder d;
    Frame output;
    std::array<uint8_t, max_encoded> wire{};
    // Independent golden HELLO vector, CRC computed with Python binascii.crc_hqx.
    const uint8_t golden[] = {0x7e, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    0,    0,   0,
                              0,    0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0x31, 0x8f, 0x7e};
    Frame hello;
    hello.source = 1;
    hello.sequence = 1;
    auto n = encode(hello, wire);
    check(n == sizeof(golden));
    for (std::size_t i = 0; i < n; ++i)
        check(wire[i] == golden[i]);
    check(feed(d, wire.data(), n, output) == Decode::frame && output.source == 1);
    for (unsigned type = 1; type <= 10; ++type) {
        Frame f;
        f.source = 0x7e7d;
        f.peer = type == 1 ? 0 : 0x7d7e;
        f.sequence = 5;
        f.type = static_cast<Type>(type);
        f.length = static_cast<uint8_t>(payload_size(f.type));
        for (unsigned i = 0; i < f.length; ++i)
            f.payload[i] = i % 2 ? 0x7d : 0x7e;
        n = encode(f, wire);
        check(n > 0 && n <= max_encoded);
        check(feed(d, wire.data(), n, output) == Decode::frame);
        check(output.source == f.source && output.peer == f.peer && output.payload == f.payload);
        wire[n - 3] ^= 1;
        check(feed(d, wire.data(), n, output) == Decode::rejected);
    }
    n = encode(hello, wire);
    // Oversize/no delimiter, malformed escape, truncated frame, resynchronization.
    d.push(0x7e, output);
    for (unsigned i = 0; i < 500; ++i)
        d.push(1, output);
    check(d.push(0x7e, output) == Decode::rejected);
    check(feed(d, wire.data(), n, output) == Decode::frame);
    const uint8_t bad[] = {0x7e, 0x7d, 0, 0x7e};
    check(feed(d, bad, sizeof(bad), output) == Decode::rejected);
    feed(d, wire.data(), 12, output);
    check(d.push(0x7e, output) == Decode::rejected);
    check(feed(d, wire.data(), n, output) == Decode::frame);
    hello.sequence = 0;
    check(encode(hello, wire) == 0);
    hello.sequence = 1;
    hello.length = 65;
    check(encode(hello, wire) == 0);
    hello.length = 0;
    hello.type = static_cast<Type>(255);
    check(encode(hello, wire) == 0);
    std::cout << "monitor codec PASS\n";
}
