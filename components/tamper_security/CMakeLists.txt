idf_component_register(
    SRCS "tamper_security.c"
    INCLUDE_DIRS "include"
    REQUIRES driver nvs_flash
)
