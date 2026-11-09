idf_component_register(INCLUDE_DIRS "include" REQUIRES esp_driver_rmt esp_timer)
target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_14)
