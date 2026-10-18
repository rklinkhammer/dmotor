#include "dmotor_sw/version.hpp"
#include "dmotor_sw/linux_inspection.hpp"
#include "dmotor_sw/protocol.hpp"
#include <iostream>
#include <limits>
#include <string>
int main(int argc, char** argv) {
    if (dmotor_sw::startup_version("dmotor-inspect", argc, argv)) return 0;
    if (argc != 3 || std::string(argv[1]) != "--externally-inhibited") {
        std::cerr << "Usage: dmotor-inspect --externally-inhibited UIO_INDEX\n"
                     "Use only with externally inhibited outputs and no FPGA reload/unbind.\n";
        return 2;
    }
    try {
        const std::string input = argv[2];
        if (input.empty() || input.find_first_not_of("0123456789") != std::string::npos)
            throw dmotor_sw::IoError("Expected numeric UIO index");
        const auto index = std::stoul(input);
        if (index > std::numeric_limits<unsigned>::max()) throw dmotor_sw::IoError("UIO index too large");
        auto device = dmotor_sw::LinuxInspection::open_uio(static_cast<unsigned>(index));
        dmotor_sw::Protocol protocol(*device);
        const auto status = protocol.status();
        dmotor_sw::TelemetrySnapshot snapshot;
        if (!protocol.capture(snapshot)) throw dmotor_sw::IoError("Snapshot changed; no coherent result");
        std::cout << "DMI2 status=0x" << std::hex << status.raw << std::dec
                  << " capture=" << snapshot.capture_sequence << '\n';
        for (unsigned i = 0; i < snapshot.motors.size(); ++i) {
            const auto& motor = snapshot.motors[i];
            std::cout << "motor " << i << " position=" << motor.position_counts
                      << " measured_mrpm=" << motor.measured_mrpm << " valid=" << motor.velocity_valid
                      << " engine_tick=" << motor.sample_tick << " faults=" << motor.engine_faults
                      << " protection=" << motor.protection_faults << '\n';
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
