idf_component_register(
    SRCS "bluetooth_security.c"
    INCLUDE_DIRS "include"
    REQUIRES bluetooth_spp nvs_flash
)
