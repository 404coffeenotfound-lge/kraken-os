idf_component_register(
    SRCS "kraken.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../components/system/private"
    REQUIRES 
        system 
        audio 
        bluetooth 
        display 
        network 
        input 
        power 
        apps
        nvs_flash 
        bt 
        app_update 
        esp_partition 
        spi_flash
)
