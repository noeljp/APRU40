idf_component_register(
    SRCS "ads7128.c"
    INCLUDE_DIRS "include"
    REQUIRES i2c_bus driver freertos
)
