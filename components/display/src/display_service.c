#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ft5x06.h"
#include "esp_lvgl_port.h"

#include "display_service.h"
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "ui_topbar.h"
#include "ui_mainmenu.h"
#include "ui_button.h"
#include "ui_styles.h"

static const char *TAG = "display_service";

static system_service_id_t display_service_id = 0;
static system_event_type_t display_events[8];
static bool initialized = false;
static uint8_t current_brightness = 80;
static bool screen_on_state = true;
static display_orientation_t current_orientation = DISPLAY_ORIENTATION_0;

// Menu event types
static system_event_type_t menu_audio_event = SYSTEM_EVENT_TYPE_INVALID;
static system_event_type_t menu_network_event = SYSTEM_EVENT_TYPE_INVALID;
static system_event_type_t menu_bluetooth_event = SYSTEM_EVENT_TYPE_INVALID;
static system_event_type_t menu_display_event = SYSTEM_EVENT_TYPE_INVALID;
static system_event_type_t menu_apps_event = SYSTEM_EVENT_TYPE_INVALID;
static system_event_type_t menu_back_event = SYSTEM_EVENT_TYPE_INVALID;

// LCD handles
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_display_t *lvgl_disp = NULL;
// Touch handles
static esp_lcd_panel_io_handle_t touch_io_handle = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;

// Navigation stack for screen management
#define NAV_STACK_MAX 5
static lv_obj_t *main_screen = NULL;
static lv_obj_t *nav_stack[NAV_STACK_MAX] = {NULL};
static int nav_stack_top = -1;

// Helper to get current content
static lv_obj_t* get_current_content(void) {
    if (nav_stack_top >= 0 && nav_stack_top < NAV_STACK_MAX) {
        return nav_stack[nav_stack_top];
    }
    return NULL;
}

// Push new content to navigation stack
static esp_err_t nav_push(lv_obj_t *content) {
    if (content == NULL) return ESP_ERR_INVALID_ARG;
    if (nav_stack_top >= NAV_STACK_MAX - 1) {
        ESP_LOGE(TAG, "Navigation stack full");
        return ESP_ERR_NO_MEM;
    }
    
    // Hide current content if exists
    lv_obj_t *current = get_current_content();
    if (current && lv_obj_is_valid(current)) {
        lv_obj_add_flag(current, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Push new content
    nav_stack_top++;
    nav_stack[nav_stack_top] = content;
    lv_obj_clear_flag(content, LV_OBJ_FLAG_HIDDEN);
    
    ESP_LOGI(TAG, "Pushed screen to nav stack (level %d)", nav_stack_top);
    return ESP_OK;
}

// Pop content from navigation stack
static esp_err_t nav_pop(void) {
    if (nav_stack_top < 0) {
        ESP_LOGW(TAG, "Navigation stack empty");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Delete current content
    lv_obj_t *current = nav_stack[nav_stack_top];
    if (current && lv_obj_is_valid(current)) {
        lv_obj_del(current);
    }
    nav_stack[nav_stack_top] = NULL;
    nav_stack_top--;
    
    // Show previous content
    lv_obj_t *previous = get_current_content();
    if (previous && lv_obj_is_valid(previous)) {
        lv_obj_clear_flag(previous, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Popped to nav stack level %d", nav_stack_top);
    } else if (nav_stack_top < 0) {
        ESP_LOGI(TAG, "Navigation stack now empty");
    }
    
    return ESP_OK;
}

// Display configuration structure
typedef struct {
    spi_host_device_t host;
    int dma_channel;
    int pin_mosi;
    int pin_miso;
    int pin_sclk;
    int pin_cs;
    int pin_dc;
    int pin_rst;
    int pin_bl;
    // Touch I2C pins
    i2c_port_t i2c_port;
    int pin_touch_sda;
    int pin_touch_scl;
    int pin_touch_int;
    int pin_touch_rst;
    uint16_t hor_res;
    uint16_t ver_res;
} board_display_config_t;

static const board_display_config_t s_display_config = {
    .host = SPI2_HOST,
    .dma_channel = SPI_DMA_CH_AUTO,
    .pin_mosi = 11,      // IO11 - LCD SPI MOSI
    .pin_miso = 13,      // IO13 - LCD SPI MISO  
    .pin_sclk = 12,      // IO12 - LCD SPI Clock
    .pin_cs = 10,        // IO10 - LCD Chip Select (active low)
    .pin_dc = 46,        // IO46 - LCD Data/Command select
    .pin_rst = -1,       // RST - Shared with ESP32-S3 reset (hardware controlled)
    .pin_bl = 45,        // IO45 - LCD Backlight control
    // Touch I2C configuration
    .i2c_port = I2C_NUM_0,
    .pin_touch_sda = 16, // IO16 - Touch I2C SDA
    .pin_touch_scl = 15, // IO15 - Touch I2C SCL
    .pin_touch_int = 17, // IO17 - Touch interrupt (active low)
    .pin_touch_rst = 18, // IO18 - Touch reset (active low)
    .hor_res = 240,
    .ver_res = 320,
};

// Menu item callbacks
static void menu_audio_clicked(void)
{
    ESP_LOGI(TAG, "Audio menu clicked");
    if (menu_audio_event != SYSTEM_EVENT_TYPE_INVALID) {
        system_event_post(display_service_id, menu_audio_event, NULL, 0, SYSTEM_EVENT_PRIORITY_NORMAL);
    }
}

static void menu_network_clicked(void)
{
    ESP_LOGI(TAG, "Network menu clicked");
    if (menu_network_event != SYSTEM_EVENT_TYPE_INVALID) {
        system_event_post(display_service_id, menu_network_event, NULL, 0, SYSTEM_EVENT_PRIORITY_NORMAL);
    }
}

static void menu_bluetooth_clicked(void)
{
    ESP_LOGI(TAG, "Bluetooth menu clicked");
    if (menu_bluetooth_event != SYSTEM_EVENT_TYPE_INVALID) {
        system_event_post(display_service_id, menu_bluetooth_event, NULL, 0, SYSTEM_EVENT_PRIORITY_NORMAL);
    }
}

static void menu_display_clicked(void)
{
    ESP_LOGI(TAG, "Display menu clicked");
    if (menu_display_event != SYSTEM_EVENT_TYPE_INVALID) {
        system_event_post(display_service_id, menu_display_event, NULL, 0, SYSTEM_EVENT_PRIORITY_NORMAL);
    }
}

static void menu_apps_clicked(void)
{
    ESP_LOGI(TAG, "Apps menu clicked");
    if (menu_apps_event != SYSTEM_EVENT_TYPE_INVALID) {
        system_event_post(display_service_id, menu_apps_event, NULL, 0, SYSTEM_EVENT_PRIORITY_NORMAL);
    }
}

static esp_err_t init_ui(void)
{
    ESP_LOGI(TAG, "Initializing UI...");
    
    // Lock LVGL mutex
    if (lvgl_port_lock(0)) {
        main_screen = lv_scr_act();
        
        // Set screen background to black
        lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x000000), 0);
        
        // Create top bar with NULL config (use defaults)
        lv_obj_t *topbar = ui_topbar_create(main_screen, NULL);
        if (topbar == NULL) {
            ESP_LOGE(TAG, "Failed to create top bar");
            lvgl_port_unlock();
            return ESP_FAIL;
        }
        
        // Initialize status icons
        ui_topbar_update_time(12, 0);
        ui_topbar_update_wifi(false, 0);
        ui_topbar_update_bluetooth(false);
        ui_topbar_update_battery(85, false);
        
        // Create main menu items
        static const ui_menu_item_t menu_items[] = {
            { .label = "Audio",      .icon = LV_SYMBOL_AUDIO,      .callback = menu_audio_clicked },
            { .label = "Network",    .icon = LV_SYMBOL_WIFI,       .callback = menu_network_clicked },
            { .label = "Bluetooth",  .icon = LV_SYMBOL_BLUETOOTH,  .callback = menu_bluetooth_clicked },
            { .label = "Display",    .icon = LV_SYMBOL_IMAGE,      .callback = menu_display_clicked },
            { .label = "Apps",       .icon = LV_SYMBOL_LIST,       .callback = menu_apps_clicked },
        };
        
        // Create main menu
        uint16_t topbar_height = ui_topbar_get_height();
        lv_obj_t *main_menu = ui_mainmenu_create(main_screen, topbar_height, menu_items, 
                                            sizeof(menu_items) / sizeof(menu_items[0]), NULL);
        if (main_menu == NULL) {
            ESP_LOGE(TAG, "Failed to create main menu");
            lvgl_port_unlock();
            return ESP_FAIL;
        }
        
        // Push main menu to navigation stack as first screen
        nav_push(main_menu);
        
        ESP_LOGI(TAG, "✓ UI created successfully");
        
        // Unlock LVGL mutex
        lvgl_port_unlock();
        
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to lock LVGL mutex");
    return ESP_FAIL;
}

/* Configure touch controller */
static esp_err_t config_touch_controller(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Configuring touch controller...");
    
    // 1. Initialize I2C bus
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = s_display_config.pin_touch_sda,
        .scl_io_num = s_display_config.pin_touch_scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,  // 400kHz
    };
    
    ret = i2c_param_config(s_display_config.i2c_port, &i2c_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure I2C: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(s_display_config.i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2C driver: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ I2C bus initialized (SDA=%d, SCL=%d)", 
             s_display_config.pin_touch_sda, s_display_config.pin_touch_scl);
    
    // 2. Configure FT6336G touch controller
    const esp_lcd_touch_config_t touch_cfg = {
        .x_max = s_display_config.hor_res,
        .y_max = s_display_config.ver_res,
        .rst_gpio_num = s_display_config.pin_touch_rst,
        .int_gpio_num = s_display_config.pin_touch_int,
        .levels = {
            .reset = 0,      // Active low reset
            .interrupt = 0,  // Active low interrupt
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    
    ret = esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)s_display_config.i2c_port, 
                                    &tp_io_config, 
                                    &touch_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create touch panel I/O: %s", esp_err_to_name(ret));
        i2c_driver_delete(s_display_config.i2c_port);
        return ret;
    }
    
    ret = esp_lcd_touch_new_i2c_ft5x06(touch_io_handle, &touch_cfg, &touch_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create FT6336G touch: %s", esp_err_to_name(ret));
        i2c_driver_delete(s_display_config.i2c_port);
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ FT6336G touch controller initialized");
    ESP_LOGI(TAG, "  Touch resolution: %dx%d", s_display_config.hor_res, s_display_config.ver_res);
    ESP_LOGI(TAG, "  INT pin: %d, RST pin: %d", 
             s_display_config.pin_touch_int, s_display_config.pin_touch_rst);
    
    return ESP_OK;
}
static esp_err_t config_lcd_display(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Configuring LCD display...");
    
    // 1. Initialize SPI bus
    const spi_bus_config_t bus_config = {
        .sclk_io_num = s_display_config.pin_sclk,
        .mosi_io_num = s_display_config.pin_mosi,
        .miso_io_num = s_display_config.pin_miso,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = s_display_config.hor_res * 80 * sizeof(uint16_t),
    };

    ret = spi_bus_initialize(s_display_config.host, &bus_config, s_display_config.dma_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ SPI bus initialized");

    // 2. Configure LCD panel I/O (SPI interface)
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = s_display_config.pin_dc,
        .cs_gpio_num = s_display_config.pin_cs,
        .pclk_hz = 40 * 1000 * 1000,  // 40 MHz for stable operation
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)s_display_config.host, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LCD panel I/O: %s", esp_err_to_name(ret));
        spi_bus_free(s_display_config.host);
        return ret;
    }
    ESP_LOGI(TAG, "✓ LCD panel I/O created");

    // 3. Configure ILI9341 panel (basic config without vendor override)
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = s_display_config.pin_rst,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    
    ret = esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ILI9341 panel: %s", esp_err_to_name(ret));
        esp_lcd_panel_io_del(io_handle);
        spi_bus_free(s_display_config.host);
        return ret;
    }
    ESP_LOGI(TAG, "✓ ILI9341 panel created");

    // 4. Reset and initialize panel
    ret = esp_lcd_panel_reset(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset panel: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    ret = esp_lcd_panel_init(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init panel: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    ESP_LOGI(TAG, "✓ Panel reset and initialized");

    // 5. Set panel orientation for vertical mode
    ret = esp_lcd_panel_swap_xy(panel_handle, false);  // No swap for portrait
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set swap_xy: %s", esp_err_to_name(ret));
    }
    
    ret = esp_lcd_panel_mirror(panel_handle, true, false);  // Mirror X to fix horizontal mirroring
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set mirror: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✓ Panel orientation configured (vertical mode)");
    }

    // 6. Turn on display
    ret = esp_lcd_panel_disp_on_off(panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn on display: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    ESP_LOGI(TAG, "✓ Display turned ON");

    // 7. Initialize backlight (simple GPIO control)
    if (s_display_config.pin_bl >= 0) {
        gpio_config_t bl_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << s_display_config.pin_bl
        };
        gpio_config(&bl_gpio_config);
        gpio_set_level(s_display_config.pin_bl, 1);  // Turn on backlight
        ESP_LOGI(TAG, "✓ Backlight enabled (pin %d)", s_display_config.pin_bl);
    }

    // 8. Initialize LVGL port
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = configMAX_PRIORITIES - 3,
        .task_stack = 6144,
        .task_affinity = 1,
        .timer_period_ms = 5,
    };
    
    ret = lvgl_port_init(&lvgl_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LVGL port: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    ESP_LOGI(TAG, "✓ LVGL port initialized");

    // 9. Create LVGL display with reduced buffer size for SRAM savings
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = s_display_config.hor_res * 20,  // Reduced from 50 to 20 lines
        .double_buffer = true,
        .hres = s_display_config.hor_res,
        .vres = s_display_config.ver_res,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .mirror_x = 1,  // Enable X mirroring at LVGL level
            .mirror_y = 0,
            .swap_xy = 0,
        },
        .flags = {
            .swap_bytes = 1,
            .buff_dma = 0,  // Don't use DMA buffers (saves SRAM)
            .buff_spiram = 1,  // Use SPIRAM for buffers
        },
    };

    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    if (lvgl_disp == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        ret = ESP_FAIL;
        goto cleanup;
    }
    ESP_LOGI(TAG, "✓ LVGL display created (%dx%d)", s_display_config.hor_res, s_display_config.ver_res);
    
    // 10. Initialize touch controller
    ret = config_touch_controller();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch controller initialization failed, continuing without touch");
        // Don't fail - display can work without touch
    } else {
        // Register touch input with LVGL
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lvgl_disp,
            .handle = touch_handle,
        };
        
        if (lvgl_port_add_touch(&touch_cfg) == NULL) {
            ESP_LOGW(TAG, "Failed to add touch to LVGL");
        } else {
            ESP_LOGI(TAG, "✓ Touch input registered with LVGL");
        }
    }
    
    // Set display as default
    if (lvgl_port_lock(pdMS_TO_TICKS(1000))) {
        lv_disp_set_default(lvgl_disp);
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "✓ LCD display configuration complete!");
    return ESP_OK;

cleanup:
    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);
        panel_handle = NULL;
    }
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);
        io_handle = NULL;
    }
    spi_bus_free(s_display_config.host);
    return ret;
} 

/* Callback for back button click */
static void back_button_clicked(void)
{
    ESP_LOGI(TAG, "Back button clicked");
    if (menu_back_event != SYSTEM_EVENT_TYPE_INVALID) {
        system_event_post(display_service_id, menu_back_event, NULL, 0, SYSTEM_EVENT_PRIORITY_NORMAL);
    }
}


/* Event handler for back button */
static void display_back_event_handler(const system_event_t *event, void *user_data)
{
    if (event->event_type == menu_back_event) {
        ESP_LOGI(TAG, "Back button clicked - returning to previous screen");
        
        if (!lvgl_port_lock(0)) {
            ESP_LOGE(TAG, "Failed to lock LVGL mutex");
            return;
        }
        
        // Simply pop the current screen to return to previous
        nav_pop();
        
        lvgl_port_unlock();
    }
}

esp_err_t display_service_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Display service already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing display service...");
    
    // Register with system service
    ret = system_service_register("display_service", NULL, &display_service_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register with system service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Registered with system service (ID: %d)", display_service_id);
    
    // Register event types
    const char *event_names[] = {
        "display.registered",
        "display.started",
        "display.stopped",
        "display.brightness_changed",
        "display.screen_on",
        "display.screen_off",
        "display.orientation_changed",
        "display.error"
    };
    
    for (int i = 0; i < 8; i++) {
        ret = system_event_register_type(event_names[i], &display_events[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register event type '%s'", event_names[i]);
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "✓ Registered %d event types", 8);
    
    // Register menu event types
    system_event_register_type("menu.audio_clicked", &menu_audio_event);
    system_event_register_type("menu.network_clicked", &menu_network_event);
    system_event_register_type("menu.bluetooth_clicked", &menu_bluetooth_event);
    system_event_register_type("menu.display_clicked", &menu_display_event);
    system_event_register_type("menu.apps_clicked", &menu_apps_event);
    system_event_register_type("menu.back_clicked", &menu_back_event);
    ESP_LOGI(TAG, "✓ Registered menu event types");
    
    // Subscribe to menu events
    ret = system_event_subscribe(display_service_id, menu_back_event, display_back_event_handler, NULL);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Subscribed to menu.back_clicked event");
    }
    
    // Note: network_service handles its own UI, so we don't subscribe to network clicks here
    
    // Initialize LCD hardware
    ret = config_lcd_display();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LCD display");
        system_service_unregister(display_service_id);
        return ret;
    }
    
    // Initialize UI (topbar + main menu)
    ret = init_ui();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize UI");
        // Continue anyway - display is functional
    }
    
    // Set service state
    system_service_set_state(display_service_id, SYSTEM_SERVICE_STATE_REGISTERED);
    
    initialized = true;
    
    // Post registration event
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_REGISTERED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Display service initialized successfully");
    ESP_LOGI(TAG, "  → Posted DISPLAY_EVENT_REGISTERED");
    
    return ESP_OK;
}

esp_err_t display_service_deinit(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing display service...");
    
    // Clean up LVGL
    if (lvgl_disp) {
        lvgl_port_remove_disp(lvgl_disp);
        lvgl_disp = NULL;
    }
    lvgl_port_deinit();
    
    // Turn off backlight
    if (s_display_config.pin_bl >= 0) {
        gpio_set_level(s_display_config.pin_bl, 0);
    }
    
    // Clean up LCD panel
    if (panel_handle) {
        esp_lcd_panel_disp_on_off(panel_handle, false);
        esp_lcd_panel_del(panel_handle);
        panel_handle = NULL;
    }
    
    // Clean up panel I/O
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);
        io_handle = NULL;
    }
    
    // Free SPI bus
    spi_bus_free(s_display_config.host);
    
    system_service_unregister(display_service_id);
    initialized = false;
    
    ESP_LOGI(TAG, "✓ Display service deinitialized");
    
    return ESP_OK;
}

esp_err_t display_service_start(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting display service...");
    
    system_service_set_state(display_service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Post started event
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_STARTED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Display service started");
    ESP_LOGI(TAG, "  → Posted DISPLAY_EVENT_STARTED");
    
    return ESP_OK;
}

esp_err_t display_service_stop(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping display service...");
    
    system_service_set_state(display_service_id, SYSTEM_SERVICE_STATE_STOPPING);
    
    // Post stopped event
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_STOPPED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Display service stopped");
    
    return ESP_OK;
}

esp_err_t display_set_brightness(uint8_t brightness)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (brightness > 100) {
        brightness = 100;
    }
    
    current_brightness = brightness;
    
    // Control backlight PWM or simple on/off
    // For simple GPIO control, map brightness to on/off
    if (s_display_config.pin_bl >= 0) {
        gpio_set_level(s_display_config.pin_bl, brightness > 0 ? 1 : 0);
        // TODO: Use LEDC PWM for proper brightness control
    }
    
    display_brightness_event_t event_data = {
        .brightness = brightness
    };
    
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_BRIGHTNESS_CHANGED],
                     &event_data,
                     sizeof(event_data),
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    system_service_heartbeat(display_service_id);
    
    ESP_LOGI(TAG, "Brightness changed: %d%%", brightness);
    
    return ESP_OK;
}

esp_err_t display_get_brightness(uint8_t *brightness)
{
    if (!initialized || brightness == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    *brightness = current_brightness;
    return ESP_OK;
}

esp_err_t display_screen_on(void)
{
    if (!initialized || !panel_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Turning screen ON");
    
    // Turn on LCD panel
    esp_lcd_panel_disp_on_off(panel_handle, true);
    
    // Turn on backlight
    if (s_display_config.pin_bl >= 0) {
        gpio_set_level(s_display_config.pin_bl, 1);
    }
    
    screen_on_state = true;
    
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_SCREEN_ON],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    system_service_heartbeat(display_service_id);
    
    return ESP_OK;
}

esp_err_t display_screen_off(void)
{
    if (!initialized || !panel_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Turning screen OFF");
    
    // Turn off backlight
    if (s_display_config.pin_bl >= 0) {
        gpio_set_level(s_display_config.pin_bl, 0);
    }
    
    // Turn off LCD panel
    esp_lcd_panel_disp_on_off(panel_handle, false);
    
    screen_on_state = false;
    
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_SCREEN_OFF],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    system_service_heartbeat(display_service_id);
    
    return ESP_OK;
}

esp_err_t display_set_orientation(display_orientation_t orientation)
{
    if (!initialized || !panel_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    current_orientation = orientation;
    
    // Configure panel orientation
    switch (orientation) {
        case DISPLAY_ORIENTATION_0:
            ret = esp_lcd_panel_swap_xy(panel_handle, false);
            if (ret == ESP_OK) {
                ret = esp_lcd_panel_mirror(panel_handle, true, true);
            }
            break;
            
        case DISPLAY_ORIENTATION_90:
            ret = esp_lcd_panel_swap_xy(panel_handle, true);
            if (ret == ESP_OK) {
                ret = esp_lcd_panel_mirror(panel_handle, false, true);
            }
            break;
            
        case DISPLAY_ORIENTATION_180:
            ret = esp_lcd_panel_swap_xy(panel_handle, false);
            if (ret == ESP_OK) {
                ret = esp_lcd_panel_mirror(panel_handle, true, true);
            }
            break;
            
        case DISPLAY_ORIENTATION_270:
            ret = esp_lcd_panel_swap_xy(panel_handle, true);
            if (ret == ESP_OK) {
                ret = esp_lcd_panel_mirror(panel_handle, true, false);
            }
            break;
            
        default:
            return ESP_ERR_INVALID_ARG;
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set orientation: %s", esp_err_to_name(ret));
        return ret;
    }
    
    display_orientation_event_t event_data = {
        .orientation = orientation
    };
    
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_ORIENTATION_CHANGED],
                     &event_data,
                     sizeof(event_data),
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    system_service_heartbeat(display_service_id);
    
    ESP_LOGI(TAG, "Orientation changed: %d degrees", orientation * 90);
    
    return ESP_OK;
}

system_service_id_t display_service_get_id(void)
{
    return display_service_id;
}

/* Load new screen content using navigation stack */
esp_err_t display_service_load_screen(lv_obj_t *content)
{
    if (!main_screen || !content) {
        ESP_LOGE(TAG, "Invalid parameters for load_screen");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Failed to lock LVGL mutex");
        return ESP_FAIL;
    }
    
    // Use navigation stack for screen management
    esp_err_t ret = nav_push(content);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Screen loaded via nav_push");
    } else {
        ESP_LOGE(TAG, "Failed to push screen");
    }
    
    lvgl_port_unlock();
    return ret;
}

lv_obj_t* display_service_get_main_screen(void)
{
    return main_screen;
}
