idf_component_register(
    SRCS "src/bluetooth_service.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "../system/private"
    REQUIRES system esp_wifi bt nvs_flash
)
