idf_component_register(
    SRCS "tca9537.c"
    INCLUDE_DIRS "include"
    REQUIRES driver i2c_bus
)
