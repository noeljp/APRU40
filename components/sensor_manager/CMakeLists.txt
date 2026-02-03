idf_component_register(
    SRCS "sensor_manager.c"
    INCLUDE_DIRS "include" "${CMAKE_SOURCE_DIR}/include"
    REQUIRES 
        freertos 
        esp_common 
        i2c_bus 
        ads7128 
        ads1119
)
