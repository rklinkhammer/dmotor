#include "dmotor/peripheral.hpp"
#include "dmotor/ring_config.hpp"
#include "dmotor/command_engine.hpp"
#include "dmotor/registers.hpp"
#include "dmotor/configuration.hpp"
extern "C" void dmotor_peripheral(
    bool reset, bool bus_valid, bool write, uint32_t address, uint32_t data, uint8_t strobes,
    uint64_t now, bool executor_ready, int64_t position, int32_t max_speed_mrpm,
    bool upstream_drained, uint32_t generation, bool cancel_valid, uint32_t cancel_cause,
    bool completion_valid, uint32_t completion_epoch, uint32_t completion_sequence,
    uint32_t completion_detail, uint32_t& read_data, bool& bus_error, uint32_t& engine_status,
    bool& activated, uint32_t& active_epoch, uint32_t& active_sequence, bool& disable_request,
    bool& stop_request, uint32_t safety_state, uint32_t safety_faults, uint32_t live_faults,
    uint32_t expected_refresh, uint32_t& safety_action, uint32_t& safety_argument,
    uint32_t& refresh_session_out, uint64_t& timeout_cycles, bool& configuration_valid) {
    using namespace dmotor::reg;
    static dmotor::Configuration config;
    static uint32_t last_status = 0, last_producer = 0, last_consumer = 0, last_epoch = 0;
    safety_action = 0;
    safety_argument = 0;
    const bool config_address = address == ENABLE || address == ACK_LOCAL ||
                                address == ACK_EXTERNAL || address == FAULTS ||
                                (address >= WATCHDOG_US && address <= SAFETY_STATUS);
    bool configuration_access = false;
    uint32_t config_read = 0;
    (void)max_speed_mrpm;
    (void)generation;
    static uint32_t publish_session = 0, completion_consumer = 0;
    uint32_t request[4] = {99, 0, 0, 0}; // Non-poll: register access cannot activate work.
    uint32_t reply[16] = {};
    bool action = false, read_word = false;
    bool set_session = false;
    bus_error = bus_valid && ((address & 3) != 0 || (write && strobes != 15));
    read_data = 0;
    disable_request = false;
    stop_request = false;
    bool cancel = cancel_valid;
    uint32_t cause = cancel_cause;
    if (reset) {
        publish_session = 0;
        completion_consumer = 0;
        config = dmotor::Configuration{};
        last_status = 0;
        last_producer = 0;
        last_consumer = 0;
        last_epoch = 0;
    } else if (!bus_valid) {
        request[0] = 0;
    } else if (!bus_error && config_address) {
        configuration_access = true;
        if (cancel_valid && write && address != ACK_LOCAL && address != ACK_EXTERNAL) {
            bus_error = true;
        } else {
            bus_error =
                !config.access(write, address, data, safety_state, safety_faults, live_faults,
                               expected_refresh, last_status, last_producer, last_consumer,
                               last_epoch, config_read, safety_action, safety_argument);
        }
    } else if (!bus_error) {
        if (write) {
            action = true;
            if (address >= SUBMISSION_OFFSET &&
                address < SUBMISSION_OFFSET + SUBMISSION_SLOTS * SUBMISSION_STRIDE) {
                request[0] = 1;
                request[1] = (address - SUBMISSION_OFFSET) / SUBMISSION_STRIDE;
                request[2] = ((address - SUBMISSION_OFFSET) % SUBMISSION_STRIDE) / 4;
                request[3] = data;
            } else {
                switch (address) {
                    case PUBLISH_SESSION:
                        set_session = true;
                        action = false;
                        break;
                    case PUBLISH:
                        request[0] = 2;
                        request[1] = data;
                        request[2] = publish_session;
                        break;
                    case REOPEN:
                        request[0] = 3;
                        bus_error = data != 1 || !config.valid() || (safety_state & 1);
                        break;
                    case QUIESCE:
                        request[0] = 4;
                        bus_error = data != 1;
                        break;
                    case COMPLETION_ACK:
                        request[0] = 5;
                        request[1] = data;
                        break;
                    case CANCEL_ACK:
                        request[0] = 6;
                        bus_error = data != 1;
                        break;
                    case DISABLE:
                    case STOP:
                        bus_error = data != 1;
                        if (!bus_error && !cancel_valid) {
                            cancel = true;
                            cause = address == DISABLE ? 0x80000000U : 0x40000000U;
                            disable_request = address == DISABLE;
                            stop_request = address == STOP;
                            action = false;
                        }
                        break;
                    default:
                        bus_error = true;
                        break;
                }
            }
        } else if (address >= COMPLETION_OFFSET &&
                   address < COMPLETION_OFFSET + COMPLETION_SLOTS * COMPLETION_STRIDE) {
            request[0] = 7;
            const uint32_t slot = (address - COMPLETION_OFFSET) / COMPLETION_STRIDE;
            request[1] = completion_consumer +
                         ((slot - (completion_consumer & dmotor::ring_mask)) & dmotor::ring_mask);
            request[2] = ((address - COMPLETION_OFFSET) % COMPLETION_STRIDE) / 4;
            action = read_word = true;
        } else if (address >= ACTIVE_OFFSET && address < ACTIVE_OFFSET + ACTIVE_STRIDE) {
            request[0] = 8;
            request[1] = (address - ACTIVE_OFFSET) / 4;
            action = read_word = true;
        }
    }
    if (bus_error) {
        request[0] = 99;
        action = false;
    }
    dmotor_command_engine(
        reset, request, now, executor_ready && (safety_state & 1) && config.valid(), position,
        static_cast<int32_t>(config.speed()), upstream_drained, config.generation(), cancel, cause,
        completion_valid, completion_epoch, completion_sequence, completion_detail, reply);
    if (!reset && bus_valid) {
        // Protective cancellation rejects any simultaneous bus write.
        if (cancel_valid && write && !configuration_access) {
            bus_error = true;
        }
        if (action && !reply[0]) {
            bus_error = true;
        }
        if (!bus_error && set_session) {
            publish_session = data;
        }
        if (!bus_error && write && address == COMPLETION_ACK) {
            completion_consumer = data;
        }
        if (!bus_error && write && address == REOPEN) {
            completion_consumer = 0;
        }
        if (!write && !bus_error) {
            if (configuration_access) {
                read_data = config_read;
            } else if (read_word) {
                read_data = reply[7];
            } else if (address >= CANCELLATION_OFFSET &&
                       address < CANCELLATION_OFFSET + CANCELLATION_STRIDE) {
                read_data = reply[8 + (address - CANCELLATION_OFFSET) / 4];
            } else {
                switch (address) {
                    case IDENTITY:
                        read_data = 0x444d4f54;
                        break;
                    case SCHEMA_VERSION:
                        read_data = ABI_SCHEMA;
                        break;
                    case CAPABILITIES:
                        read_data = 3;
                        break;
                    case CLOCK_HZ:
                        read_data = FABRIC_HZ;
                        break;
                    case STATUS:
                        read_data = reply[1];
                        break;
                    case QUEUE_EPOCH:
                        read_data = reply[5];
                        break;
                    case QUEUE_PRODUCER:
                        read_data = reply[2];
                        break;
                    case QUEUE_CONSUMER:
                        read_data = reply[3];
                        break;
                    case COMPLETION_PRODUCER:
                        read_data = reply[4];
                        break;
                    case COMPLETION_CONSUMER:
                        read_data = completion_consumer;
                        break;
                    case PUBLISH_SESSION:
                        read_data = publish_session;
                        break;
                    default:
                        break; // Reserved/unimplemented and action reads are zero.
                }
            }
        }
    }
    last_status = reply[1];
    last_producer = reply[2];
    last_consumer = reply[3];
    last_epoch = reply[5];
    timeout_cycles = config.timeout_cycles();
    configuration_valid = config.valid();
    refresh_session_out = config.refresh_session();
    engine_status = reply[1];
    activated = reply[6] != 0 && !cancel && !reset;
    active_epoch = reply[5];
    active_sequence = reply[3] - 1;
}
