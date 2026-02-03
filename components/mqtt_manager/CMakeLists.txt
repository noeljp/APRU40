idf_component_register(
    SRCS "mqtt_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt esp_eth nvs_flash fatfs sdmmc
)
