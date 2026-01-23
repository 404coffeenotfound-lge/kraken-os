idf_component_register(
    SRCS 
        "src/network_service.c"
        "ui/network_ui.c"
    INCLUDE_DIRS 
        "include"
        "ui"
    PRIV_INCLUDE_DIRS "."
    REQUIRES system display
    PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash lvgl
)
