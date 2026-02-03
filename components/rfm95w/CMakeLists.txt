idf_component_register(
    SRCS "rfm95w.c"
    INCLUDE_DIRS "include"
    REQUIRES driver spi_bus
)
