#include "dmotor_diag/uart.hpp"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
using namespace dmotor_diag;
extern "C" void app_main() {
    // Set output latches LOW before enabling outputs. External pulldowns cover reset/boot.
    const gpio_num_t inhibit[] = {GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10, GPIO_NUM_11, GPIO_NUM_21};
    for (auto pin : inhibit)
        ESP_ERROR_CHECK(gpio_set_level(pin, 0));
    gpio_config_t pins{};
    for (auto pin : inhibit)
        pins.pin_bit_mask |= 1ULL << pin;
    pins.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&pins));
    uart_config_t config{};
    config.baud_rate = 115200;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 17, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 512, 512, 0, nullptr, 0));
    ESP_LOGW("bench",
             "UART DIAGNOSTIC v1 only: permits LOW, HB LOW, no ARM. UART1 TX17 RX18 115200 8N1");
    Decoder decoder;
    uint64_t run = 0;
    uint32_t expected = 1, received = 0, sent = 0, rejected = 0, mismatch = 0;
    int64_t last = 0;
    bool active = false;
    for (;;) {
        uint8_t bytes[256];
        const int n = uart_read_bytes(UART_NUM_1, bytes, sizeof(bytes), pdMS_TO_TICKS(10));
        for (int i = 0; i < n; ++i) {
            Packet p;
            auto status = decoder.push(bytes[i], p);
            if (status == Decode::rejected)
                ++rejected;
            if (status != Decode::packet)
                continue;
            if (p.kind != 1 || !pattern(p)) {
                ++mismatch;
                continue;
            }
            if (p.run != run) {
                if (p.sequence != 1) {
                    ++mismatch;
                    continue;
                }
                run = p.run;
                expected = 1;
                // Keep lifetime error counters; never silently erase observed failures.
                ESP_LOGI("bench", "new run=%llu", static_cast<unsigned long long>(run));
            }
            if (p.sequence != expected || !expected) {
                ++mismatch;
                continue;
            }
            ++received;
            ++expected;
            last = esp_timer_get_time();
            active = true;
            std::array<uint8_t, max_wire> reply{};
            const auto size = encode(make(2, run, p.sequence), reply);
            // One response at a time; bounded frame fits the TX ring, bounded drain.
            const int wrote = uart_write_bytes(UART_NUM_1, reply.data(), size);
            if (wrote != static_cast<int>(size) ||
                uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(100)) != ESP_OK) {
                ESP_LOGE("bench", "TX failure; reboot required");
                for (;;)
                    vTaskDelay(pdMS_TO_TICKS(1000));
            }
            ++sent;
        }
        if (active && esp_timer_get_time() - last > 1500000) {
            ESP_LOGI("bench",
                     "run=%llu lifetime_received=%lu sent=%lu rejected=%lu mismatched=%lu; idle",
                     static_cast<unsigned long long>(run), static_cast<unsigned long>(received),
                     static_cast<unsigned long>(sent), static_cast<unsigned long>(rejected),
                     static_cast<unsigned long>(mismatch));
            active = false;
        }
    }
}
