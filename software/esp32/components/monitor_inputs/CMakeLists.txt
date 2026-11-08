idf_component_register(SRCS "monitor_inputs.cpp" INCLUDE_DIRS "include"
    REQUIRES safety_core monitor_safety esp_adc esp_driver_gpio esp_rom)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_14)
