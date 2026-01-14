idf_component_register(
    SRCS "src/network_service.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES system
    PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash
)
