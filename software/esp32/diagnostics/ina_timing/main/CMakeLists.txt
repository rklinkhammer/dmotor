idf_component_register(SRCS "main.cpp" INCLUDE_DIRS "." "../../../../include"
    REQUIRES acquisition esp_driver_i2c esp_driver_gpio esp_adc esp_timer esp_driver_uart esp_hw_support esp_app_format esp_rom)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_14)
