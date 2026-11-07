idf_component_register(SRCS "main.cpp" INCLUDE_DIRS "."
    REQUIRES acquisition esp_driver_i2c esp_driver_gpio esp_adc esp_timer)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_14)
