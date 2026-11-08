#include "monitor_inputs.hpp"
namespace dmotor_monitor {
bool EspMonitorInputs::start() {
    if(!population_ || population_>15 || unit_) return false;
    adc_oneshot_unit_init_cfg_t cfg{}; cfg.unit_id=ADC_UNIT_1;
    if(adc_oneshot_new_unit(&cfg,&unit_)!=ESP_OK) return false;
    constexpr adc_channel_t map[]={ADC_CHANNEL_0,ADC_CHANNEL_1,ADC_CHANNEL_3,
        ADC_CHANNEL_4,ADC_CHANNEL_5,ADC_CHANNEL_6};
    adc_oneshot_chan_cfg_t channel{}; channel.bitwidth=ADC_BITWIDTH_12;
    channel.atten=ADC_ATTEN_DB_12;
    for(unsigned i=0;i<6;++i) if(i<2 || (population_&(1u<<(i-2))))
        if(adc_oneshot_config_channel(unit_,map[i],&channel)!=ESP_OK) return false;
    gpio_config_t gpio{}; gpio.mode=GPIO_MODE_INPUT;
    gpio.pin_bit_mask=(1ULL<<40)|(1ULL<<39)|(1ULL<<47);
    constexpr unsigned nf[]={12,13,14,42};
    for(unsigned i=0;i<4;++i) if(population_&(1u<<i)) gpio.pin_bit_mask|=1ULL<<nf[i];
    return gpio_config(&gpio)==ESP_OK;
}
bool EspMonitorInputs::adc(unsigned channel,int& raw) {
    constexpr adc_channel_t map[]={ADC_CHANNEL_0,ADC_CHANNEL_1,ADC_CHANNEL_3,
        ADC_CHANNEL_4,ADC_CHANNEL_5,ADC_CHANNEL_6};
    return unit_ && channel<6 && (channel<2 || (population_&(1u<<(channel-2)))) &&
        adc_oneshot_read(unit_,map[channel],&raw)==ESP_OK;
}
uint32_t EspMonitorInputs::faults() {
    uint32_t flags=0;
    if(gpio_get_level(GPIO_NUM_40)!=0) flags|=1; // STOP healthy solid-low.
    if(gpio_get_level(GPIO_NUM_39)!=0) flags|=2; // HALT active high.
    if(gpio_get_level(GPIO_NUM_47)==0) flags|=4; // Shared ALERT active low.
    constexpr gpio_num_t nf[]={GPIO_NUM_12,GPIO_NUM_13,GPIO_NUM_14,GPIO_NUM_42};
    for(unsigned i=0;i<4;++i) if((population_&(1u<<i)) && gpio_get_level(nf[i])==0)
        flags|=1u<<(i+3);
    return flags;
}
} // namespace dmotor_monitor
