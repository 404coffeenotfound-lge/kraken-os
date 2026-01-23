idf_component_register(
    SRCS 
        "src/display_service.c"
        "ui/ui_topbar.c"
        "ui/ui_mainmenu.c"
        "ui_common/ui_button.c"
        "ui_common/ui_toggle.c"
        "ui_common/ui_keyboard.c"
    INCLUDE_DIRS 
        "include"
        "ui"
        "ui_common"
    REQUIRES
        system lvgl esp_lcd esp_lvgl_port driver espressif__esp_lcd_ili9341 espressif__esp_lcd_touch_ft5x06
)
