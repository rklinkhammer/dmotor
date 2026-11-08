#pragma once
#include "safety_core.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/gpio.h"
namespace dmotor_monitor {
// Candidate task adapter. Deliberately hard-inhibited for pre-PCB integration.
// No ARM or firmware option can enable pins in this build. A high-priority task
// observes deadlines without taking the I2C owner's lock. This is not an ISR
// timing guarantee; electrical latency and priority interference remain open.
class MonitorSafety {
    portMUX_TYPE lock_=portMUX_INITIALIZER_UNLOCKED;
    SafetyCore core_;
    bool started_=false;
    static void task(void* arg) {
        auto& self=*static_cast<MonitorSafety*>(arg);
        TickType_t wake=xTaskGetTickCount();
        for(;;) {
            self.observe();
            vTaskDelayUntil(&wake,1);
        }
    }
    void outputs() {
        for(auto pin:{GPIO_NUM_8,GPIO_NUM_9,GPIO_NUM_10,GPIO_NUM_11,GPIO_NUM_21})
            gpio_set_level(pin,0);
    }
public:
    bool start() {
        if(started_) return false;
        gpio_config_t cfg{};
        cfg.pin_bit_mask=(1ULL<<8)|(1ULL<<9)|(1ULL<<10)|(1ULL<<11)|(1ULL<<21);
        cfg.mode=GPIO_MODE_OUTPUT;
        outputs();
        if(gpio_config(&cfg)!=ESP_OK) return false;
        outputs(); started_=true;
        if(configTICK_RATE_HZ!=1000 ||
           xTaskCreate(task,"monitor_safety",3072,this,configMAX_PRIORITIES-2,nullptr)!=pdPASS) {
            fault(); return false;
        }
        return true;
    }
    WorkToken begin(Us release) {
        portENTER_CRITICAL(&lock_);
        auto token=core_.begin(release,esp_timer_get_time()); outputs();
        portEXIT_CRITICAL(&lock_); return token;
    }
    bool finish(WorkToken token,const SafetyProof& proof) {
        portENTER_CRITICAL(&lock_);
        bool ok=core_.finish(token,esp_timer_get_time(),proof); outputs();
        portEXIT_CRITICAL(&lock_); return ok;
    }
    void observe() {
        portENTER_CRITICAL(&lock_);
        core_.observe(esp_timer_get_time()); outputs();
        portEXIT_CRITICAL(&lock_);
    }
    void fault() {
        portENTER_CRITICAL(&lock_); core_.trip(); outputs(); portEXIT_CRITICAL(&lock_);
    }
};
} // namespace dmotor_monitor
