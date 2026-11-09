idf_component_register(SRCS "main.cpp" "gpio_diagnostic.cpp" INCLUDE_DIRS "../../../../include" REQUIRES hb_indicator esp_driver_gpio esp_driver_uart esp_driver_usb_serial_jtag esp_timer)
