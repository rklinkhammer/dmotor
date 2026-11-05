#include "dmotor_sw/monitor_uart.hpp"
#include "dmotor_sw/device.hpp"
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <iostream>
using namespace dmotor_sw::monitor;
static void check(bool good) {
    if (!good)
        std::abort();
}
int main() {
    const int master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    check(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
    const std::string name = ptsname(master);
    {
        Uart uart(name);
        Frame f;
        f.source = 1;
        f.sequence = 1;
        Frame received;
        uart.queue(f, 1000);
        uart.poll(1, received);
        check(!uart.transmitting());
        uint8_t bytes[max_encoded]{};
        ssize_t count = -1;
        for (unsigned i = 0; i < 100 && count < 0; ++i) {
            count = read(master, bytes, sizeof(bytes));
            if (count < 0)
                usleep(1000);
        }
        check(count > 0);
        Decoder decoder;
        for (ssize_t i = 0; i < count; ++i)
            decoder.push(bytes[i], received);
        check(received.source == 1 && received.sequence == 1);
        // Fragmentation and adjacent frames must preserve both complete frames.
        check(write(master, bytes, 4) == 4);
        check(!uart.poll(2, received));
        check(write(master, bytes + 4, static_cast<std::size_t>(count - 4)) == count - 4);
        check(write(master, bytes, static_cast<std::size_t>(count)) == count);
        unsigned got = 0;
        for (unsigned i = 0; i < 100 && got < 2; ++i) {
            if (uart.poll(3, received))
                ++got;
            else
                usleep(1000);
        }
        check(got == 2);
        uart.queue(f, 10);
        bool busy = false;
        try {
            uart.queue(f, 20);
        } catch (const dmotor_sw::IoError&) {
            busy = true;
        }
        check(busy);
        bool expired = false;
        try {
            uart.poll(10, received);
        } catch (const dmotor_sw::IoError&) {
            expired = true;
        }
        check(expired);
        close(master);
        bool lost = false;
        try {
            uart.poll(11, received);
        } catch (const dmotor_sw::IoError&) {
            lost = true;
        }
        check(lost);
    }
    std::cout << "monitor UART PASS\n";
}
