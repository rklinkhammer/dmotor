#include <dmotor_sw/client.hpp>
#include <stdexcept>
int main() {
    // Link/use the installed API without daemon internals or hardware access.
    try {
        dmotor_sw::Client client("");
    } catch (const std::invalid_argument&) {
        return 0;
    }
    return 1;
}
