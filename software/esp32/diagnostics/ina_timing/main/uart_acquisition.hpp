#pragma once
#include "dmotor_diag/acquisition.hpp"
#include "driver/uart.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
namespace acquisition_link {
using namespace dmotor_acq_diag;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static Snapshot latest;
static int64_t completed_at=0;
static DeadlineGate gate;
static uint32_t expirations=0;
static esp_timer_handle_t supervisor=nullptr;
static void expire(void*) {
    bool tripped=false;
    portENTER_CRITICAL(&lock);
    if(gate.check(static_cast<uint64_t>(esp_timer_get_time()))) {
        ++expirations; latest.status=2; latest.expired=expirations; tripped=true;
    }
    portEXIT_CRITICAL(&lock);
    if(tripped) {
        // Always inhibited, including before the observer runs.
        for(auto pin : {GPIO_NUM_8,GPIO_NUM_9,GPIO_NUM_10,GPIO_NUM_11,GPIO_NUM_21})
            gpio_set_level(pin,0);
    }
}
static void begin(uint32_t seq,int64_t deadline) {
    esp_timer_stop(supervisor);
    portENTER_CRITICAL(&lock);
    gate.begin(seq,static_cast<uint64_t>(deadline));
    portEXIT_CRITICAL(&lock);
    const int64_t left=deadline-esp_timer_get_time();
    if(left<=0) expire(nullptr);
    else ESP_ERROR_CHECK(esp_timer_start_once(supervisor,static_cast<uint64_t>(left)));
}
static void publish(Snapshot s,int64_t ended) {
    portENTER_CRITICAL(&lock);
    const bool timed_out=gate.check(static_cast<uint64_t>(esp_timer_get_time()));
    if(timed_out) ++expirations;
    const bool on_time=gate.finish(s.cycle,static_cast<uint64_t>(esp_timer_get_time()));
    if(!on_time) s.status=2;
    s.boot=latest.boot; s.expired=expirations;
    latest=s; completed_at=ended;
    portEXIT_CRITICAL(&lock);
}
static uint32_t expiry_count() {
    portENTER_CRITICAL(&lock); const auto count=expirations; portEXIT_CRITICAL(&lock);
    return count;
}
static void serve(void*) {
    dmotor_diag::Decoder decoder(magic);
    for(;;) {
        uint8_t bytes[128];
        int n=uart_read_bytes(UART_NUM_1,bytes,sizeof(bytes),pdMS_TO_TICKS(10));
        for(int i=0;i<n;++i) {
            Packet q;
            if(decoder.push(bytes[i],q)!=dmotor_diag::Decode::packet ||
               q.kind!=1 || q.length || !q.run || !q.sequence) continue;
            Snapshot s;
            portENTER_CRITICAL(&lock);
            s=latest;
            const int64_t age=esp_timer_get_time()-completed_at;
            s.age=age<0 || age>UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(age);
            portEXIT_CRITICAL(&lock);
            std::array<uint8_t,dmotor_diag::max_wire> wire{};
            const auto size=dmotor_diag::encode(response(q,s),wire,magic);
            // This separate task may block, but never holds the snapshot lock or
            // I2C owner. A stalled receiver cannot authorize outputs.
            if(uart_write_bytes(UART_NUM_1,wire.data(),size)!=static_cast<int>(size) ||
               uart_wait_tx_done(UART_NUM_1,pdMS_TO_TICKS(100))!=ESP_OK) {
                portENTER_CRITICAL(&lock); latest.status=3; portEXIT_CRITICAL(&lock);
                vTaskDelete(nullptr);
            }
        }
    }
}
static void start() {
    uart_config_t cfg{}; cfg.baud_rate=115200; cfg.data_bits=UART_DATA_8_BITS;
    cfg.parity=UART_PARITY_DISABLE; cfg.stop_bits=UART_STOP_BITS_1;
    cfg.flow_ctrl=UART_HW_FLOWCTRL_DISABLE; cfg.source_clk=UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1,&cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1,17,18,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1,512,512,0,nullptr,0));
    latest.boot=(uint64_t{esp_random()}<<32)|esp_random();
    if(!latest.boot) latest.boot=1; // diagnostic nonce, not durable recovery identity
    esp_timer_create_args_t args{}; args.callback=expire; args.name="diag_deadline";
    ESP_ERROR_CHECK(esp_timer_create(&args,&supervisor));
    configASSERT(xTaskCreate(serve,"acq_uart",4096,nullptr,2,nullptr)==pdPASS);
}
} // namespace acquisition_link
