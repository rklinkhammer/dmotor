idf_component_register(SRCS "monitor_i2c.cpp" INCLUDE_DIRS "include"
                       REQUIRES acquisition esp_driver_i2c esp_timer)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_14)
