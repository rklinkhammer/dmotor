#pragma once
// Called only after permit and HB outputs are initialized LOW.
void start_gpio_diagnostic();

// Terminal UART error: revoke synthetic HB until reboot.
void fail_gpio_diagnostic();
