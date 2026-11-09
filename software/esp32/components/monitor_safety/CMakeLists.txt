idf_component_register(INCLUDE_DIRS "include" REQUIRES safety_core hb_indicator esp_driver_gpio esp_timer)
target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_14)
