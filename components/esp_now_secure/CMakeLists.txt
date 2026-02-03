idf_component_register(
    SRCS "esp_now_secure.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi nvs_flash mbedtls
)
