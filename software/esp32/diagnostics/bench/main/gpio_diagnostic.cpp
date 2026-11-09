#include "gpio_diagnostic.hpp"
#include "heartbeat_pin.hpp"
#include "hb_indicator.hpp"
#include "dmotor_diag/gpio.hpp"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include <cstdio>
#include <cstdarg>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <initializer_list>
#include <atomic>
using namespace dmotor_diag;
static_assert(configTICK_RATE_HZ == 1000,
              "U1 requires CONFIG_FREERTOS_HZ=1000; update sdkconfig with menuconfig");
namespace {
const gpio_num_t inputs[] = {GPIO_NUM_41, GPIO_NUM_40, GPIO_NUM_39, GPIO_NUM_12,
                             GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_42, GPIO_NUM_47};
struct Request {
    bool start;
    uint64_t requested;
};
struct Sample {
    uint32_t levels;
    uint32_t remaining_ms;
    HbEnd reason;
    bool high;
    uint32_t rejected_starts;
};
QueueHandle_t commands, samples;
dmotor_monitor::HeartbeatIndicator indicator;
std::atomic<bool> failed{false};
const char* reason_name(HbEnd value) {
    switch (value) {
        case HbEnd::idle:
            return "idle";
        case HbEnd::running:
            return "SYNTHETIC";
        case HbEnd::stopped:
            return "stopped";
        case HbEnd::expired:
            return "expired";
        case HbEnd::disconnected:
            return "USB-disconnected";
        case HbEnd::late:
            return "missed-deadline";
    }
    return "invalid";
}
void control(void*) {
    BenchHeartbeat heartbeat;
    uint32_t rejected = 0;
    auto wake = xTaskGetTickCount();
    for (;;) {
        const auto now = static_cast<uint64_t>(esp_timer_get_time());
        const bool connected = usb_serial_jtag_is_connected();
        Request request{};
        if (xQueueReceive(commands, &request, 0) == pdTRUE) {
            if (!request.start) {
                heartbeat.stop();
            } else if (!heartbeat.start(request.requested, now, connected)) {
                ++rejected;
            }
        }
        heartbeat.step(now, connected);
        if (failed.load()) {
            heartbeat.stop();
        }
        // This task never logs, waits for I/O, asserts a permit, or operates UART1.
        for (auto pin : {GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10, GPIO_NUM_11}) {
            ESP_ERROR_CHECK(gpio_set_level(pin, 0));
        }
        ESP_ERROR_CHECK(gpio_set_level(bench_heartbeat_pin, heartbeat.high() ? 1 : 0));
        indicator.observe(heartbeat.high());
        Sample sample{};
        for (unsigned i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
            sample.levels |= static_cast<uint32_t>(gpio_get_level(inputs[i])) << i;
        }
        sample.high = heartbeat.high();
        sample.reason = heartbeat.reason();
        sample.remaining_ms = static_cast<uint32_t>(heartbeat.remaining(now) / 1000);
        sample.rejected_starts = rejected;
        xQueueOverwrite(samples, &sample); // Diagnostics are lossy, never block HB ownership.
        vTaskDelayUntil(&wake, 1);
    }
}
// Best-effort bounded USB output; a full/unplugged console may lose a report.
void report(const char* format, ...) {
    char text[512];
    va_list args;
    va_start(args, format);
    const int size = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (size > 0) {
        const size_t count = static_cast<size_t>(size) < sizeof(text) - 2
                                 ? static_cast<size_t>(size)
                                 : sizeof(text) - 2;
        text[count] = '\r';
        text[count + 1] = '\n';
        usb_serial_jtag_write_bytes(text, count + 2, pdMS_TO_TICKS(20));
    }
}
void help() {
    report(
        "DIAGNOSTIC ONLY: permits always LOW; no ARM, no sensor health. USB commands + Enter: gpio, quiet, hb10, stop, help");
    report(
        "hb10 = SYNTHETIC 50 Hz GPIO%d for at most 10 s; motor drive power must be disconnected. quiet does NOT stop HB; use stop.", static_cast<int>(bench_heartbeat_pin));
}
void console(void*) {
    BenchConsole parser;
    bool reporting = false;
    int64_t next_report = 0;
    help();
    for (;;) {
        char bytes[32];
        const int n = usb_serial_jtag_read_bytes(bytes, sizeof(bytes), 0);
        if (!usb_serial_jtag_is_connected()) {
            parser = {}; // Discard partial commands across USB disconnection.
            vTaskDelay(1);
            continue;
        }
        for (int i = 0; i < n; ++i) {
            const auto command = parser.push(bytes[i]);
            if (command == BenchCommand::gpio) {
                reporting = true;
                next_report = 0;
            } else if (command == BenchCommand::quiet) {
                reporting = false;
            } else if (command == BenchCommand::hb10 || command == BenchCommand::stop) {
                Request request{command == BenchCommand::hb10,
                                static_cast<uint64_t>(esp_timer_get_time())};
                if (!request.start) {
                    xQueueOverwrite(commands, &request); // Stop replaces any pending start.
                } else if (xQueueSend(commands, &request, 0) != pdTRUE) {
                    report("Command busy; HB start not queued");
                }
                reporting = true;
                next_report = 0;
            } else if (command == BenchCommand::help || command == BenchCommand::invalid) {
                help();
            }
        }
        const auto now = esp_timer_get_time();
        if (reporting && now >= next_report) {
            Sample sample{};
            if (xQueuePeek(samples, &sample, 0) == pdTRUE) {
                report(
                    "RAW ARM41=%u STOP40=%u HALT39=%u nFAULT12/13/14/42=%u/%u/%u/%u ALERT47=%u; permits commanded LOW; HB=%s level=%u remaining_ms=%lu rejected_starts=%lu",
                    unsigned(sample.levels & 1), unsigned((sample.levels >> 1) & 1),
                    unsigned((sample.levels >> 2) & 1), unsigned((sample.levels >> 3) & 1),
                    unsigned((sample.levels >> 4) & 1), unsigned((sample.levels >> 5) & 1),
                    unsigned((sample.levels >> 6) & 1), unsigned((sample.levels >> 7) & 1),
                    reason_name(sample.reason), unsigned(sample.high),
                    static_cast<unsigned long>(sample.remaining_ms),
                    static_cast<unsigned long>(sample.rejected_starts));
            }
            next_report = now + 1000000;
        }
        vTaskDelay(1);
    }
}
} // namespace
void start_gpio_diagnostic() {
    (void)indicator.start();
    gpio_config_t config{};
    for (auto pin : inputs) {
        if (pin != GPIO_NUM_39)
            config.pin_bit_mask |= 1ULL << pin;
    }
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
    config.pin_bit_mask = 1ULL << GPIO_NUM_39;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
    usb_serial_jtag_driver_config_t usb = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    usb_serial_jtag_vfs_use_driver();
    commands = xQueueCreate(1, sizeof(Request));
    samples = xQueueCreate(1, sizeof(Sample));
    ESP_ERROR_CHECK(commands && samples ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(control, "u1_gpio", 3072, nullptr, 5, nullptr) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(console, "u1_usb", 4096, nullptr, 1, nullptr) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
}

void fail_gpio_diagnostic() {
    failed.store(true);
}
