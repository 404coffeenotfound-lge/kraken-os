idf_component_register(
    SRCS 
        "src/display_service.c"
        "ui/ui_topbar.c"
        "ui/ui_mainmenu.c"
    INCLUDE_DIRS 
        "include"
        "ui"
    REQUIRES
        system lvgl esp_lcd esp_lvgl_port driver
)
