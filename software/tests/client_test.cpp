#include "dmotor_sw/client.hpp"
#include <climits>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace dmotor_sw;
namespace {
void require(bool value) {
    if (!value)
        throw std::runtime_error("Client assertion failed");
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 3);
        const std::string mode = argv[2];
        if (mode == "connect-fail" || mode == "refuse") {
            try {
                Client client(argv[1], {100, 1000});
            } catch (const ClientError&) {
                return 0;
            }
            throw std::runtime_error("Connection should fail");
        }
        Client client(argv[1], {100, 1000});
        if (mode == "drop")
            return 0;
        if (mode != "exercise") {
            bool failed = false;
            try {
                client.status();
            } catch (const ClientError&) {
                failed = true;
            }
            require(failed && !client.connected());
            failed = false;
            try {
                client.ping();
            } catch (const ClientError&) {
                failed = true;
            }
            require(failed);
            return 0;
        }
        unsigned invalid = 0;
        try {
            client.enable(0);
        } catch (const std::invalid_argument&) {
            ++invalid;
        }
        try {
            client.velocity(16, {});
        } catch (const std::invalid_argument&) {
            ++invalid;
        }
        try {
            client.duty(INT32_MIN);
        } catch (const std::invalid_argument&) {
            ++invalid;
        }
        require(invalid == 3 && client.connected());
        auto result = client.status();
        require(result.okay && result.applied == 42 && result.status == 0x1234);
        require(result.position_counts[0] == INT64_MIN && result.position_counts[1] == -1 &&
                result.position_counts[2] == INT64_MAX && result.position_counts[3] == 0);
        require(result.measured_mrpm[0] == -1234);
        require(client.prepare().okay); // Server waits longer than ordinary deadline.
        require(client.enable(3).okay);
        require(client.velocity(3, {{1000, -2000, 0, 0}}).okay);
        require(client.stop(3).okay);
        require(client.disable(3).okay);
        require(client.halt().okay);
        require(client.duty(-32768).okay);
        require(client.inject_target_timeout().okay);
        result = client.inject_watchdog_timeout();
        require(!result.okay && result.message == "Rejected" && client.connected());
        result = client.ping();
        require(result.okay && result.position_counts[0] == 0 && result.measured_mrpm[0] == 0);
        require(client.quit().okay && !client.connected());
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
