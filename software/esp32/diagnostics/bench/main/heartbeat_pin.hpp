#pragma once
#include "driver/gpio.h"
#include "sdkconfig.h"
// Diagnostic-only selection; operational monitor pin mapping is unchanged.
#if defined(CONFIG_DMOTOR_PREPCB_HB_GPIO3) && CONFIG_DMOTOR_PREPCB_HB_GPIO3
constexpr gpio_num_t bench_heartbeat_pin = GPIO_NUM_3;
#else
constexpr gpio_num_t bench_heartbeat_pin = GPIO_NUM_21;
#endif
