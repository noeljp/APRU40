idf_component_register(
    SRCS "ads1119.c"
    INCLUDE_DIRS "include"
    REQUIRES driver i2c_bus freertos
)
