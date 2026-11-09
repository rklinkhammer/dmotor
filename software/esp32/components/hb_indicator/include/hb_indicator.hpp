#pragma once
#include <atomic>
#include "driver/rmt_tx.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
namespace dmotor_monitor {
// Permanent object. Low-priority RMT task only observes generated GPIO21 edges.
// Failure cannot change heartbeat, permits or monitor scheduling.
class HeartbeatIndicator {
    std::atomic<uint32_t> edges_{0};
    bool previous_=false,started_=false;
    rmt_channel_handle_t channel_=nullptr;
    rmt_encoder_handle_t encoder_=nullptr;
    rmt_symbol_word_t symbols_[25]{};
    static void task(void* p) { static_cast<HeartbeatIndicator*>(p)->run(); }
    bool color(bool red,bool blue) {
        const uint32_t grb=(red?uint32_t(8)<<8:0)|(blue?8:0);
        for(unsigned i=0;i<24;++i) {
            const bool one=(grb&(1u<<(23-i)))!=0;
            symbols_[i].level0=1;symbols_[i].duration0=one?8:4;
            symbols_[i].level1=0;symbols_[i].duration1=one?4:8;
        }
        symbols_[24].level0=0;symbols_[24].duration0=1500;
        symbols_[24].level1=0;symbols_[24].duration1=1500;
        rmt_transmit_config_t tx{};
        return rmt_transmit(channel_,encoder_,symbols_,sizeof symbols_,&tx)==ESP_OK &&
               rmt_tx_wait_all_done(channel_,20)==ESP_OK;
    }
    void run() {
        uint32_t last=edges_.load(); int64_t changed=0; bool seen=false;
        int last_color=-1;
        for(;;) {
            const auto now=esp_timer_get_time(); const auto edge=edges_.load();
            if(edge!=last) { changed=now;seen=true;last=edge; }
            const bool live=seen && now-changed<50000;
            const bool phase=(now/250000)&1;
            const int c=phase?(live?2:1):0;
            if(c!=last_color) {
                if(!color(c==1,c==2)) { vTaskDelete(nullptr); return; }
                last_color=c;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
public:
    // Call from the serialized output owner, never from the LED task.
    void observe(bool level) { if(level!=previous_) { previous_=level; ++edges_; } }
    bool start() {
        if(started_ || (CONFIG_DMOTOR_HB_LED_GPIO!=38 && CONFIG_DMOTOR_HB_LED_GPIO!=48)) return false;
        started_=true;
        rmt_tx_channel_config_t cfg{};
        cfg.gpio_num=static_cast<gpio_num_t>(CONFIG_DMOTOR_HB_LED_GPIO);
        cfg.clk_src=RMT_CLK_SRC_DEFAULT;cfg.resolution_hz=10000000;
        cfg.mem_block_symbols=64;cfg.trans_queue_depth=1;
        if(rmt_new_tx_channel(&cfg,&channel_)!=ESP_OK) return false;
        rmt_copy_encoder_config_t ec{};
        if(rmt_new_copy_encoder(&ec,&encoder_)!=ESP_OK || rmt_enable(channel_)!=ESP_OK) return false;
        return xTaskCreate(task,"hb_led",3072,this,1,nullptr)==pdPASS;
    }
};
} // namespace dmotor_monitor
