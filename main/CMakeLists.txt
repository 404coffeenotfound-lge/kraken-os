idf_component_register(SRCS "kraken.c"
                   INCLUDE_DIRS "."
                   REQUIRES system audio bluetooth display network input apps nvs_flash bt app_update esp_partition spi_flash)
