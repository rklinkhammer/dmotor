#include "timing_cycle.hpp"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <algorithm>
#include <cstdio>

#if CONFIG_FREERTOS_HZ != 100 && CONFIG_FREERTOS_HZ != 1000
#error "Timing comparison supports 100 Hz or 1000 Hz"
#endif
#ifndef CONFIG_DMOTOR_TIMING_IPROPI
#define CONFIG_DMOTOR_TIMING_IPROPI 0
#endif
#ifndef CONFIG_DMOTOR_RAW_TIMEOUT_PROBE
#define CONFIG_DMOTOR_RAW_TIMEOUT_PROBE 0
#endif
namespace {
using namespace dmotor_monitor;
constexpr Us period=monitor_period_us;
struct ClockImpl : Clock { Us now() override { return esp_timer_get_time(); } } clock_us;
struct ReadTrace {
    Us start=0, end=0;
    int error=0, wait_ms=0, ticks=0, raw=0;
    unsigned slot=0, reg=0;
};
struct Record {
    Us release=0;
    dmotor_timing::Cycle cycle{};
    ReadTrace ina[12]{};
    ReadTrace adc[CONFIG_DMOTOR_ADC_SAMPLES * (CONFIG_DMOTOR_TIMING_IPROPI ? 6 : 2)]{};
    unsigned ina_n=0, adc_n=0, skipped=0;
};
// Static internal RAM. No PSRAM, heap, USB or formatting in measured cycle.
Record records[CONFIG_DMOTOR_TIMING_CYCLES];
unsigned selected_address=0;
static_assert(sizeof(records) <= 160 * 1024,
    "Capture exceeds 160 KiB internal RAM budget: reduce cycles or ADC samples");
class BusImpl : public Bus {
public:
    i2c_master_dev_handle_t device=nullptr;
    Record* record=nullptr;
    bool read(unsigned slot, std::uint8_t reg, int wait, std::uint16_t& value) override {
        if (!record || record->ina_n>=12 || slot>=4 || wait<=0 || wait>2) return false;
        auto& t=record->ina[record->ina_n++];
        t.slot=slot; t.reg=reg; t.wait_ms=wait; t.ticks=static_cast<int>(pdMS_TO_TICKS(wait));
        // Only explicit raw-probe builds allow zero ticks.
        if (!CONFIG_DMOTOR_RAW_TIMEOUT_PROBE && t.ticks==0) {
            t.error=ESP_ERR_INVALID_ARG; return false;
        }
        std::uint8_t bytes[2]{};
        t.start=clock_us.now();
        const esp_err_t error=i2c_master_transmit_receive(device,&reg,1,bytes,2,wait);
        t.end=clock_us.now(); t.error=error;
        value=static_cast<std::uint16_t>((static_cast<unsigned>(bytes[0])<<8)|bytes[1]);
        t.raw=value;
        return error==ESP_OK;
    }
} bus;
class AdcImpl : public dmotor_timing::Adc {
public:
    adc_oneshot_unit_handle_t unit=nullptr;
    Record* record=nullptr;
    bool read(unsigned channel, int& raw) override {
        constexpr adc_channel_t map[]={ADC_CHANNEL_0,ADC_CHANNEL_1,
            ADC_CHANNEL_3,ADC_CHANNEL_4,ADC_CHANNEL_5,ADC_CHANNEL_6};
        if (!record || record->adc_n>=sizeof(record->adc)/sizeof(record->adc[0]) || channel>=6) return false;
        auto& t=record->adc[record->adc_n++]; t.slot=channel;
        t.start=clock_us.now();
        const auto error=adc_oneshot_read(unit,map[channel],&raw);
        t.end=clock_us.now(); t.error=error; t.raw=raw;
        return error==ESP_OK;
    }
} adc;
TaskHandle_t owner=nullptr;
void release_task(void*) { xTaskNotifyGive(owner); } // esp_timer TASK dispatch, not ISR
void wait_until(esp_timer_handle_t timer, Us release) {
    // No accumulated notifications from a previous wait.
    (void)ulTaskNotifyTake(pdTRUE,0);
    const Us remaining=release-clock_us.now();
    if (remaining<=0) return;
    ESP_ERROR_CHECK(esp_timer_start_once(timer,static_cast<std::uint64_t>(remaining)));
    (void)ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
}
const char* status(Status s) {
    switch(s) {
    case Status::complete: return "transport_complete";
    case Status::config: return "config";
    case Status::deadline: return "deadline";
    case Status::unrepresentable: return "zero_tick_rejected";
    case Status::io: return "io";
    }
    return "unknown";
}
void inhibit() {
    gpio_config_t cfg{};
    for (auto pin : {GPIO_NUM_8,GPIO_NUM_9,GPIO_NUM_10,GPIO_NUM_11,GPIO_NUM_21}) {
        ESP_ERROR_CHECK(gpio_set_level(pin,0));
        cfg.pin_bit_mask|=1ULL<<pin;
    }
    cfg.mode=GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&cfg));
}
bool initialize() {
    adc_oneshot_unit_init_cfg_t ac{}; ac.unit_id=ADC_UNIT_1;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&ac,&adc.unit));
    adc_oneshot_chan_cfg_t cc{}; cc.atten=ADC_ATTEN_DB_12; cc.bitwidth=ADC_BITWIDTH_12;
    const adc_channel_t channels[]={ADC_CHANNEL_0,ADC_CHANNEL_1,
        ADC_CHANNEL_3,ADC_CHANNEL_4,ADC_CHANNEL_5,ADC_CHANNEL_6};
    for (unsigned i=0;i<(CONFIG_DMOTOR_TIMING_IPROPI ? 6u : 2u);++i)
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc.unit,channels[i],&cc));
    i2c_master_bus_config_t bc{};
    bc.i2c_port=I2C_NUM_0; bc.sda_io_num=GPIO_NUM_15; bc.scl_io_num=GPIO_NUM_16;
    bc.clk_source=I2C_CLK_SRC_DEFAULT; bc.glitch_ignore_cnt=7;
    // External pull-ups required. Queue depth zero: synchronous, one owner.
    i2c_master_bus_handle_t handle=nullptr;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bc,&handle));
    // Discovery is outside the measured workload. 100 ms remains representable
    // for both supported FreeRTOS tick profiles; no register writes are issued.
    unsigned found=0;
    std::printf("# I2C scan SDA=15 SCL=16 addresses=0x40..0x4f timeout_ms=100\n");
    for (unsigned address=0x40;address<=0x4f;++address) {
        const esp_err_t error=i2c_master_probe(handle,address,100);
        std::printf("# probe address=0x%02x result=%s error=%d\n",
            address,esp_err_to_name(error),static_cast<int>(error));
        if (error==ESP_OK) {
            ++found;
            if (!selected_address || address==CONFIG_DMOTOR_INA_ADDRESS)
                selected_address=address;
        }
    }
    if (!found) {
        std::printf("# No I2C responder at 0x40..0x4f; timing test skipped. Permits/HB LOW. Reset to retry.\n");
        std::fflush(stdout);
        return false;
    }
    std::printf("# I2C found=%u selected_address=0x%02x (preferred=0x%02x; otherwise lowest ACK). Device identity not verified.\n",
        found,selected_address,CONFIG_DMOTOR_INA_ADDRESS);
    std::fflush(stdout);
    i2c_device_config_t dc{}; dc.dev_addr_length=I2C_ADDR_BIT_LEN_7;
    dc.device_address=selected_address; dc.scl_speed_hz=100000;
    dc.scl_wait_us=1000; // test parameter, not a qualified protection bound
    ESP_ERROR_CHECK(i2c_master_bus_add_device(handle,&dc,&bus.device));
    // No register writes: preserve installed configuration and ALERT thresholds.
    return true;
}
void dump() {
    std::printf("# monitor_period_us=%lld adc_budget_us=%lld ina_budget_us=%lld work_budget_us=%lld reserved_margin_us=%lld\n",
        (long long)monitor_period_us,(long long)adc_budget_us,(long long)ina_budget_us,
        (long long)monitor_work_us,(long long)monitor_margin_us);
    std::printf("# idf=%s tick_hz=%d address=0x%x scl_hz=100000 scl_wait_us=1000 samples_per_adc=%d ipropi=%d raw_probe=%d\n",
        esp_get_idf_version(),CONFIG_FREERTOS_HZ,selected_address,
        CONFIG_DMOTOR_ADC_SAMPLES,CONFIG_DMOTOR_TIMING_IPROPI,CONFIG_DMOTOR_RAW_TIMEOUT_PROBE);
    std::printf("# Cycle timestamps are monotonic us; raw ADC codes are NOT calibrated pack voltage.\n");
    std::printf("cycle,index,release_us,start_us,adc_end_us,end_us,status,adc_attempts,ina_attempts,ina_completed,skipped_releases\n");
    unsigned completed=0; Us max_total=0,max_adc=0,max_ina=0,max_late=0;
    for (unsigned i=0;i<CONFIG_DMOTOR_TIMING_CYCLES;++i) {
        const auto& r=records[i]; const auto& c=r.cycle;
        if (c.ina.status==Status::complete && c.adc_complete) ++completed;
        max_total=std::max(max_total,c.ended-r.release);
        max_late=std::max(max_late,c.started-r.release);
        if (c.adc_complete) max_adc=std::max(max_adc,c.adc_end-c.started);
        if (r.ina_n) max_ina=std::max(max_ina,c.ended-c.adc_end);
        std::printf("cycle,%u,%lld,%lld,%lld,%lld,%s,%u,%u,%u,%u\n",i,
            (long long)r.release,(long long)c.started,(long long)c.adc_end,(long long)c.ended,
            status(c.ina.status),r.adc_n,r.ina_n,c.ina.reads,r.skipped);
    }
    std::printf("read,cycle,kind,ordinal,logical_slot_or_adc_channel,register,start_us,end_us,duration_us,timeout_ms,ticks,error,raw\n");
    for (unsigned i=0;i<CONFIG_DMOTOR_TIMING_CYCLES;++i) {
        const auto& r=records[i];
        for (unsigned kind=0;kind<2;++kind) {
            const unsigned n=kind ? r.ina_n : r.adc_n;
            for (unsigned j=0;j<n;++j) {
                const auto& t=kind ? r.ina[j] : r.adc[j];
                std::printf("read,%u,%s,%u,%u,%u,%lld,%lld,%lld,%d,%d,%d,%d\n",i,
                    kind ? "ina" : "adc",j,t.slot,t.reg,(long long)t.start,(long long)t.end,
                    (long long)(t.end-t.start),t.wait_ms,t.ticks,t.error,t.raw);
            }
        }
    }
    std::printf("# summary cycles=%d transport_complete=%u max_release_to_end_us=%lld max_complete_adc_us=%lld max_ina_attempt_window_us=%lld max_release_lateness_us=%lld\n",
        CONFIG_DMOTOR_TIMING_CYCLES,completed,(long long)max_total,(long long)max_adc,
        (long long)max_ina,(long long)max_late);
    std::printf("# DONE: permits/HB LOW. No freshness, calibration, protection or ARM qualification. Reset for another capture.\n");
    std::fflush(stdout);
}
} // namespace
extern "C" void app_main() {
    inhibit();
    if (!initialize()) {
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    // Avoid formatting/USB/logging during capture, including noisy I2C errors.
    // This profiles a deliberately log-silent configuration, not default error logging.
    esp_log_level_set("*",ESP_LOG_NONE);
    owner=xTaskGetCurrentTaskHandle();
    esp_timer_create_args_t args{};
    args.callback=release_task; args.dispatch_method=ESP_TIMER_TASK; args.name="ina_release";
    esp_timer_handle_t timer=nullptr;
    ESP_ERROR_CHECK(esp_timer_create(&args,&timer));
    Us release=clock_us.now()+100000;
    for (unsigned i=0;i<CONFIG_DMOTOR_TIMING_CYCLES;++i) {
        wait_until(timer,release);
        auto& record=records[i]; record.release=release;
        bus.record=&record; adc.record=&record;
        record.cycle=dmotor_timing::capture(clock_us,bus,adc,release,CONFIG_FREERTOS_HZ,
            CONFIG_DMOTOR_ADC_SAMPLES,CONFIG_DMOTOR_TIMING_IPROPI,CONFIG_DMOTOR_RAW_TIMEOUT_PROBE);
        release+=period;
        const Us now=clock_us.now();
        if (release<=now) {
            const Us missed=(now-release)/period+1;
            record.skipped=static_cast<unsigned>(missed);
            release+=missed*period;
        }
    }
    ESP_ERROR_CHECK(esp_timer_delete(timer));
    dump();
    // No I2C/ADC retries after the finite run. No flashing/NVS/calibration writes.
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
