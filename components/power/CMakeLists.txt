idf_component_register(
    SRCS
        "src/power_service.c"
    INCLUDE_DIRS
        "include"
    REQUIRES system display esp_adc
    PRIV_REQUIRES nvs_flash
)