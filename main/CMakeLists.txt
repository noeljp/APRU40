idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES ads7128 ads1119 tca9537 esp_now_secure bluetooth_spp bluetooth_security tamper_security i2c_bus driver esp_wifi nvs_flash bt sensor_manager
)
