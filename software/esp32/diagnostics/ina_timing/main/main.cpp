#include "timing_cycle.hpp"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_rom_sys.h"
#include "esp_app_desc.h"
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
#ifndef CONFIG_DMOTOR_ACQUISITION_UART
#define CONFIG_DMOTOR_ACQUISITION_UART 0
#endif
#ifndef CONFIG_DMOTOR_SYNTHETIC_INPUTS
#define CONFIG_DMOTOR_SYNTHETIC_INPUTS 0
#endif
#ifndef CONFIG_DMOTOR_INJECT_EVERY
#define CONFIG_DMOTOR_INJECT_EVERY 0
#endif
#ifndef CONFIG_DMOTOR_INJECT_DELAY
#define CONFIG_DMOTOR_INJECT_DELAY 0
#endif
#if CONFIG_DMOTOR_ACQUISITION_UART
#include "uart_acquisition.hpp"
#if CONFIG_FREERTOS_HZ != 1000
#error "Concurrent UART acquisition requires 1000 Hz ticks"
#endif
#endif
namespace {
using namespace dmotor_monitor;
uint32_t sequence=0;
bool injected=false;
unsigned batch_index=0;
uint32_t total_complete=0,total_failed=0,total_skipped=0;
Us total_max=0,ina_max=0,adc_max=0,late_max=0;

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
        esp_err_t error=ESP_OK;
        if(injected && slot==2 && reg==6) {
            if(CONFIG_DMOTOR_INJECT_DELAY) vTaskDelay(pdMS_TO_TICKS(20));
            else error=ESP_FAIL;
        }
        if(error==ESP_OK) {
            if(CONFIG_DMOTOR_SYNTHETIC_INPUTS) { esp_rom_delay_us(450); bytes[1]=8; }
            else error=i2c_master_transmit_receive(device,&reg,1,bytes,2,wait);
        }
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
        esp_err_t error=ESP_OK;
        if(CONFIG_DMOTOR_SYNTHETIC_INPUTS) raw=1234;
        else error=adc_oneshot_read(unit,map[channel],&raw);
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
    const auto* app=esp_app_get_description();
    std::printf("# firmware version=%s compiled=%s %s elf_sha256=",app->version,app->date,app->time);
    for(auto byte : app->app_elf_sha256) std::printf("%02x",unsigned(byte));
    std::printf(" batches=%d uart=%d synthetic=%d inject_every=%d inject_delay=%d\n",
        CONFIG_DMOTOR_TIMING_BATCHES,CONFIG_DMOTOR_ACQUISITION_UART,CONFIG_DMOTOR_SYNTHETIC_INPUTS,
        CONFIG_DMOTOR_INJECT_EVERY,CONFIG_DMOTOR_INJECT_DELAY);
    if(CONFIG_DMOTOR_SYNTHETIC_INPUTS) { selected_address=0x44; return true; }
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
    // Read-only identity/configuration evidence; never change installed thresholds.
    bool identity_ok=true;
    for (uint8_t reg : {uint8_t{0xfe},uint8_t{0xff},uint8_t{0x00}}) {
        uint8_t data[2]{};
        const auto error=i2c_master_transmit_receive(bus.device,&reg,1,data,2,100);
        std::printf("# INA register=0x%02x value=0x%04x error=%d\n",unsigned(reg),
            (unsigned(data[0])<<8)|data[1],int(error));
        if(error!=ESP_OK) return false;
        const unsigned value=(unsigned(data[0])<<8)|data[1];
        if(reg==0xfe && value!=0x5449) identity_ok=false;
        if(reg==0xff && (value & 0xfff0)!=0x2260) identity_ok=false;
    }
    std::printf("# INA identity_matches=%d (not conversion freshness)\n",identity_ok);
    if(CONFIG_DMOTOR_ACQUISITION_UART && !identity_ok) return false;
    // No register writes: preserve installed configuration and ALERT thresholds.
    return true;
}
void dump() {
    std::printf("# last_batch=%u (detailed rows cover only this batch)\n",batch_index);
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
    std::printf("# aggregate cycles=%lu complete=%lu failed=%lu skipped=%lu max_release_to_end_us=%lld max_ina_us=%lld max_adc_us=%lld max_lateness_us=%lld\n",
        (unsigned long)sequence,(unsigned long)total_complete,(unsigned long)total_failed,
        (unsigned long)total_skipped,(long long)total_max,(long long)ina_max,
        (long long)adc_max,(long long)late_max);
#if CONFIG_DMOTOR_ACQUISITION_UART
    std::printf("# deadline_observer_expirations=%lu\n",(unsigned long)acquisition_link::expiry_count());
#endif
    std::printf("# DONE: permits/HB LOW. No freshness, calibration, protection or ARM qualification. Reset for another capture.\n");
    std::fflush(stdout);
}
} // namespace
extern "C" void app_main() {
    inhibit();
    if (!initialize()) {
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
#if CONFIG_DMOTOR_ACQUISITION_UART
    acquisition_link::start();
#endif
    std::printf("# READY: finite acquisition run; permits/HB LOW.\n");
    std::fflush(stdout);
    // Avoid formatting/USB/logging during capture, including noisy I2C errors.
    // This profiles a deliberately log-silent configuration, not default error logging.
    esp_log_level_set("*",ESP_LOG_NONE);
    owner=xTaskGetCurrentTaskHandle();
    esp_timer_create_args_t args{};
    args.callback=release_task; args.dispatch_method=ESP_TIMER_TASK; args.name="ina_release";
    esp_timer_handle_t timer=nullptr;
    ESP_ERROR_CHECK(esp_timer_create(&args,&timer));
    Us release=clock_us.now()+100000;
    for (batch_index=0;batch_index<CONFIG_DMOTOR_TIMING_BATCHES;++batch_index) {
    for (unsigned i=0;i<CONFIG_DMOTOR_TIMING_CYCLES;++i) {
        wait_until(timer,release);
        ++sequence;
        const unsigned inject_every=CONFIG_DMOTOR_INJECT_EVERY;
        injected=inject_every!=0 && sequence % inject_every==0;
        auto& record=records[i]; record={}; record.release=release;
#if CONFIG_DMOTOR_ACQUISITION_UART
        acquisition_link::begin(sequence,release+monitor_work_us);
#endif
        bus.record=&record; adc.record=&record;
        record.cycle=dmotor_timing::capture(clock_us,bus,adc,release,CONFIG_FREERTOS_HZ,
            CONFIG_DMOTOR_ADC_SAMPLES,CONFIG_DMOTOR_TIMING_IPROPI,CONFIG_DMOTOR_RAW_TIMEOUT_PROBE);
        const auto& c=record.cycle;
        const bool complete=c.adc_complete && c.ina.status==Status::complete;
        if(complete) ++total_complete; else ++total_failed;
        total_max=std::max(total_max,c.ended-release);
        late_max=std::max(late_max,c.started-release);
        if(c.adc_complete) adc_max=std::max(adc_max,c.adc_end-c.started);
        if(record.ina_n) ina_max=std::max(ina_max,c.ended-c.adc_end);
        release+=period;
        const Us now=clock_us.now();
        if (release<=now) {
            const Us missed=(now-release)/period+1;
            record.skipped=static_cast<unsigned>(missed);
            release+=missed*period;
            total_skipped+=record.skipped;
        }
#if CONFIG_DMOTOR_ACQUISITION_UART
        dmotor_acq_diag::Snapshot sample;
        sample.cycle=sequence; sample.duration=static_cast<uint32_t>(c.ended-record.release);
        sample.failures=total_failed; sample.skipped=total_skipped;
        sample.status=complete ? 0 : 1; sample.reads=static_cast<uint8_t>(c.ina.reads);
        sample.address=static_cast<uint8_t>(selected_address); sample.injected=static_cast<uint8_t>((injected ? 1 : 0) | (CONFIG_DMOTOR_SYNTHETIC_INPUTS ? 2 : 0));
        if(complete) {
            for(unsigned j=0;j<12;++j) sample.raw[j]=c.ina.raw[j/3][j%3];
            sample.adc0=static_cast<uint16_t>(record.adc[0].raw);
            sample.adc1=static_cast<uint16_t>(record.adc[CONFIG_DMOTOR_ADC_SAMPLES].raw);
        }
        acquisition_link::publish(sample,c.ended);
#endif
    }
    }
    --batch_index;
    ESP_ERROR_CHECK(esp_timer_delete(timer));
    dump();
    // No I2C/ADC retries after the finite run. No flashing/NVS/calibration writes.
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
