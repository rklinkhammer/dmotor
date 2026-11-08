#include "physical_acquisition.hpp"
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
struct Record { PhysicalResult result; Us duration=0; };
static Record records[100];
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
    EspClock clock; EspBus bus(devices); PhysicalAcquisition acquisition(clock,bus,cfg);
    const Us base=clock.now();
    for(unsigned i=0;i<100;++i) {
        const Us release=base+static_cast<Us>(i)*monitor_period_us;
        const bool released=wait_for_release(clock,release,[](Us remaining) {
            // A one-tick delay may end before one millisecond has elapsed.
            // Recheck the microsecond clock, then finish only the sub-tick tail.
            if(remaining>=1000) vTaskDelay(1);
            else esp_rom_delay_us(static_cast<uint32_t>(remaining));
        });
        if(!released) { safety.fault(); ESP_ERROR_CHECK(ESP_FAIL); }
        auto token=safety.begin(release);
        records[i].result=acquisition.run(release);
        records[i].duration=clock.now()-release;
        // Missing battery calibration, policy and recovery proofs keep the
        // candidate supervisor latched inhibited; never manufacture these proofs.
        SafetyProof p; p.population=1; p.valid_sensors=records[i].result.valid;
        p.oldest_conversion=records[i].result.device[0].oldest_possible_us;
        safety.finish(token,p);
    }
    Us maximum=0; unsigned good=0;
    std::printf("# health enum: good=0 config=1 identity=2 io=3 warming=4 not_ready=5 stale=6 overflow=7 clock=8\n");
    for(unsigned i=0;i<100;++i) {
        const auto& r=records[i]; const auto& m=r.result.device[0];
        maximum=std::max(maximum,r.duration); if(m.health==Health::good) ++good;
        std::printf("health,%u,%d,%lld,%ld,%u,%lld\n",i,static_cast<int>(m.health),
            static_cast<long long>(r.duration),static_cast<long>(m.shunt_raw),unsigned(m.bus_raw),
            static_cast<long long>(m.oldest_possible_us));
    }
    std::printf("# physical_health cycles=100 good=%u max_release_to_end_us=%lld permits/HB LOW\n",
        good,static_cast<long long>(maximum));
}
