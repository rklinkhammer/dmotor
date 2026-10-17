#include "dmotor_sw/version.hpp"
#include "dmotor_sw/linux_inspection.hpp"
#include "dmotor_sw/encoder_session.hpp"
#include "dmotor_sw/protocol.hpp"
#include <csignal>
#include <chrono>
#include <thread>
#include <iostream>
#include <limits>
#include <string>
namespace {
volatile std::sig_atomic_t interrupted = 0;
void stop(int) { interrupted = 1; }
constexpr auto sample_period = std::chrono::milliseconds(250);
}
int main(int argc, char** argv) {
    if (dmotor_sw::startup_version("dmotor-encoder", argc, argv)) return 0;
    if (argc != 3 || std::string(argv[1]) != "--externally-inhibited") {
        std::cerr << "Usage: dmotor-encoder --externally-inhibited UIO_INDEX\n"
                     "Disconnect motor drive power; keep encoder powered at safe FPGA logic levels.\n"
                     "Do not reload/unbind FPGA while running. Ctrl-C restores core reset.\n";
        return 2;
    }
    try {
        const std::string input = argv[2];
        if (input.empty() || input.find_first_not_of("0123456789") != std::string::npos)
            throw dmotor_sw::IoError("Expected numeric UIO index");
        const auto index = std::stoul(input);
        if (index > std::numeric_limits<unsigned>::max()) throw dmotor_sw::IoError("UIO index too large");
        if (std::signal(SIGINT, stop) == SIG_ERR || std::signal(SIGTERM, stop) == SIG_ERR)
            throw dmotor_sw::IoError("Cannot install exit handlers");
        auto device = dmotor_sw::LinuxInspection::open_uio(static_cast<unsigned>(index),
                                                         dmotor_sw::LinuxInspection::Mode::encoder);
        dmotor_sw::EncoderSession session(*device);
        dmotor_sw::Protocol protocol(*device);
        std::cout << "Encoder reset released; motors disabled and unconfigured. Turn shaft by hand.\n";
        while (!interrupted) {
            dmotor_sw::TelemetrySnapshot snapshot;
            if (!protocol.capture(snapshot)) throw dmotor_sw::IoError("Incoherent snapshot");
            std::cout << "capture=" << snapshot.capture_sequence;
            for (unsigned i = 0; i < snapshot.motors.size(); ++i)
                std::cout << " motor" << i << "=" << snapshot.motors[i].position_counts;
            std::cout << std::endl;
            std::this_thread::sleep_for(sample_period);
        }
        session.close();
        std::cout << "Core reset restored; motors remain disabled.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
