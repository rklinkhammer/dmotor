#pragma once
#include <cstdint>
#include <string>
namespace dmotor_sw {
namespace recovery {
// Inhibited commissioning only: create a previously absent store in an existing
// private directory. Never invoke automatically because a store is missing.
void provision_endpoint_sessions(const std::string& directory, bool inhibited);
// Reserve durably before returning an ID. Throws on ambiguity; no rollback or
// repair. `inhibited` is a caller obligation, not a hardware readback by this API.
uint64_t allocate_endpoint_session(const std::string& directory, bool inhibited);
} // namespace recovery
} // namespace dmotor_sw
