#include "physical_acquisition.hpp"
#include "health_capture.hpp"
#include "release_wait.hpp"
#include "esp_rom_sys.h"
#include "monitor_i2c.hpp"
#include "monitor_safety.hpp"
#include "esp_app_desc.h"
#include "sdkconfig.h"
#include <cstdio>
#include <algorithm>
using namespace dmotor_monitor;
static MonitorSafety safety;
static HealthCapture<100> capture;
class DiagnosticBus final : public Bus {
    i2c_master_dev_handle_t device_;
public:
    unsigned attempts=0, error_reg=0;
    int32_t error=0;
    explicit DiagnosticBus(i2c_master_dev_handle_t device):device_(device) {}
    void reset() { attempts=0; error_reg=0; error=0; }
    bool read(unsigned device,uint8_t reg,int wait,uint16_t& value) override {
        ++attempts;
        uint8_t bytes[2]{};
        const esp_err_t rc=(device!=0 || wait<=0 || wait>2 || pdMS_TO_TICKS(wait)==0)
            ? ESP_ERR_INVALID_ARG
            : i2c_master_transmit_receive(device_,&reg,1,bytes,2,wait);
        if(rc!=ESP_OK) { if(!error) { error=rc; error_reg=reg; } return false; }
        value=static_cast<uint16_t>((unsigned(bytes[0])<<8)|bytes[1]); return true;
    }
};
static void print_record(const char* kind,const HealthRecord& r) {
    std::printf("%s,%lu,%u,%lld,%lld,%lld,%lld,%ld,%u,%u,%ld,0x%02x,%u\n",
        kind,static_cast<unsigned long>(r.cycle),static_cast<unsigned>(r.health),
        static_cast<long long>(r.release),static_cast<long long>(r.lateness),
        static_cast<long long>(r.duration),static_cast<long long>(r.age),
        static_cast<long>(r.shunt),unsigned(r.bus),r.attempts,
        static_cast<long>(r.bus_error),r.error_reg,unsigned(r.complete));
}
extern "C" void app_main() {
    ESP_ERROR_CHECK(safety.start()?ESP_OK:ESP_FAIL);
    char sha[65]{}; esp_app_get_elf_sha256(sha,sizeof sha);
    std::printf("# physical-health ELF=%s address=0x%02x config=0x%04x conversion_bound_us=%d\n",
        sha,CONFIG_DMOTOR_HEALTH_ADDRESS,CONFIG_DMOTOR_HEALTH_CONFIG,CONFIG_DMOTOR_CONVERSION_BOUND_US);
    std::printf("# INHIBITED: one physical INA, no ADC calibration/policy/ARM qualification; no UART protocol change\n");
    i2c_master_bus_config_t bc{};
    bc.i2c_port=I2C_NUM_0; bc.sda_io_num=GPIO_NUM_15; bc.scl_io_num=GPIO_NUM_16;
    bc.clk_source=I2C_CLK_SRC_DEFAULT; bc.glitch_ignore_cnt=7;
    i2c_master_bus_handle_t handle=nullptr;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bc,&handle));
    i2c_device_config_t dc{}; dc.dev_addr_length=I2C_ADDR_BIT_LEN_7;
    dc.device_address=CONFIG_DMOTOR_HEALTH_ADDRESS; dc.scl_speed_hz=100000;
    std::array<i2c_master_dev_handle_t,4> devices{};
    ESP_ERROR_CHECK(i2c_master_bus_add_device(handle,&dc,&devices[0]));
    SensorConfig cfg; cfg.population=1; cfg.address[0]=CONFIG_DMOTOR_HEALTH_ADDRESS;
    cfg.configuration[0]=CONFIG_DMOTOR_HEALTH_CONFIG;
    cfg.conversion_bound_us[0]=CONFIG_DMOTOR_CONVERSION_BOUND_US;
    ESP_ERROR_CHECK(cfg.valid()?ESP_OK:ESP_ERR_INVALID_ARG);
    EspClock clock; DiagnosticBus bus(devices[0]); PhysicalAcquisition acquisition(clock,bus,cfg);
    std::printf("# capture_v2 cycles=%d period_us=16000 retained_tail=100 age_limit_us=25000\n",CONFIG_DMOTOR_HEALTH_CYCLES);
    std::printf("# Capturing without console output until completion; outputs remain inhibited\n");
    const Us base=clock.now();
    for(unsigned i=0;i<CONFIG_DMOTOR_HEALTH_CYCLES;++i) {
        const Us release=base+static_cast<Us>(i)*monitor_period_us;
        const bool released=wait_for_release(clock,release,[](Us remaining) {
            // A one-tick delay may end before one millisecond has elapsed.
            // Recheck the microsecond clock, then finish only the sub-tick tail.
            if(remaining>=1000) vTaskDelay(1);
            else esp_rom_delay_us(static_cast<uint32_t>(remaining));
        });
        if(!released) { safety.fault(); ESP_ERROR_CHECK(ESP_FAIL); }
        HealthRecord r; r.cycle=i; r.release=release; r.lateness=clock.now()-release;
        auto token=safety.begin(release);
        bus.reset();
        const auto result=acquisition.run(release);
        const Us ended=clock.now();
        const auto& m=result.device[0];
        r.duration=ended-release; r.health=m.health; r.shunt=m.shunt_raw; r.bus=m.bus_raw;
        r.age=m.health==Health::good ? ended-m.oldest_possible_us : -1;
        r.complete=result.complete; r.attempts=bus.attempts; r.bus_error=bus.error; r.error_reg=bus.error_reg;
        capture.add(r);
        // Missing battery calibration, policy and recovery proofs keep the
        // candidate supervisor latched inhibited; never manufacture these proofs.
        SafetyProof p; p.population=1; p.valid_sensors=result.valid;
        p.oldest_conversion=result.device[0].oldest_possible_us;
        safety.finish(token,p);
    }
    std::printf("# health enum: good=0 config=1 identity=2 io=3 warming=4 not_ready=5 stale=6 overflow=7 clock=8\n");
    std::printf("# CSV v2: kind,cycle,health,release_us,lateness_us,duration_us,age_upper_us,shunt,bus,attempts,esp_error,error_reg,transport_complete\n");
    if(capture.has_bad) print_record("first_bad",capture.first_bad);
    for(unsigned i=0;i<capture.retained();++i) print_record("health",capture.at(i));
    for(unsigned i=0;i<capture.health_counts.size();++i)
        std::printf("# health_count status=%u count=%lu\n",i,static_cast<unsigned long>(capture.health_counts[i]));
    std::printf("# physical_health cycles=%lu good=%lu max_release_to_end_us=%lld max_release_lateness_us=%lld max_good_age_upper_us=%lld bus_error_cycles=%lu guarded_cycles=%lu late_releases=%lu acquisition_deadline_misses=%lu permits/HB LOW\n",
        static_cast<unsigned long>(capture.cycles),static_cast<unsigned long>(capture.health_counts[0]),
        static_cast<long long>(capture.max_duration),static_cast<long long>(capture.max_lateness),
        static_cast<long long>(capture.max_age),static_cast<unsigned long>(capture.bus_errors),
        static_cast<unsigned long>(capture.guarded),static_cast<unsigned long>(capture.late_releases),
        static_cast<unsigned long>(capture.deadline_misses));
}
