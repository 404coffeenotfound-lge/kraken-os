#include "network_ui.h"
#include "../../display/ui_common/ui_styles.h"
#include "../../display/ui_common/ui_button.h"
#include "../../display/ui_common/ui_toggle.h"
#include "../../display/ui_common/ui_keyboard.h"
#include "../include/network_service.h"
#include "../../system/include/system_service/system_service.h"
#include "../../system/include/system_service/event_bus.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "network_ui";

typedef struct {
    lv_obj_t *container;
    lv_obj_t *wifi_toggle;
    lv_obj_t *wifi_list;
    lv_obj_t *status_label;
    bool wifi_enabled;
    char connecting_ssid[33];
} network_ui_ctx_t;

static network_ui_ctx_t *s_ui_ctx = NULL;

static void back_button_cb(void) {
    ESP_LOGI(TAG, "Back button clicked");
    
    // Get event type and post event
    system_event_type_t menu_back_event;
    if (system_event_register_type("menu.back_clicked", &menu_back_event) == ESP_OK) {
        system_event_post(0, menu_back_event, NULL, 0, SYSTEM_EVENT_PRIORITY_NORMAL);
    }
}

static void wifi_toggle_cb(bool state) {
    if (!s_ui_ctx) return;
    s_ui_ctx->wifi_enabled = state;
    ESP_LOGI(TAG, "WiFi toggled: %s", state ? "ON" : "OFF");
    
    if (state) {
        network_scan_result_t result;
        network_scan_wifi(&result);
        lv_label_set_text(s_ui_ctx->status_label, "Scanning...");
    } else {
        lv_obj_clean(s_ui_ctx->wifi_list);
        lv_label_set_text(s_ui_ctx->status_label, "WiFi Off");
    }
}

static void wifi_password_callback(const char *password, bool cancelled) {
    if (!s_ui_ctx || cancelled || !password) {
        ESP_LOGI(TAG, "Connection cancelled");
        return;
    }
    
    ESP_LOGI(TAG, "Connecting to %s", s_ui_ctx->connecting_ssid);
    
    if (network_connect_wifi(s_ui_ctx->connecting_ssid, password) == ESP_OK) {
        lv_label_set_text(s_ui_ctx->status_label, "Connecting...");
    } else {
        lv_label_set_text(s_ui_ctx->status_label, "Connection failed");
    }
}

static void wifi_item_click_cb(lv_event_t *e) {
    if (!s_ui_ctx) return;
    lv_obj_t *item = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(item, 0);
    const char *ssid = lv_label_get_text(label);
    
    // Extract SSID (remove signal icon)
    const char *ssid_start = strchr(ssid, ' ');
    if (ssid_start) {
        ssid_start++;
        strncpy(s_ui_ctx->connecting_ssid, ssid_start, sizeof(s_ui_ctx->connecting_ssid) - 1);
        ESP_LOGI(TAG, "Selected WiFi: %s", s_ui_ctx->connecting_ssid);
        
        ui_keyboard_show(s_ui_ctx->container, "Enter WiFi Password", "", wifi_password_callback);
    }
}

lv_obj_t* network_ui_create(lv_obj_t *parent) {
    if (!parent) return NULL;
    
    s_ui_ctx = (network_ui_ctx_t*)malloc(sizeof(network_ui_ctx_t));
    if (!s_ui_ctx) return NULL;
    memset(s_ui_ctx, 0, sizeof(network_ui_ctx_t));
    
    s_ui_ctx->container = lv_obj_create(parent);
    lv_obj_set_size(s_ui_ctx->container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_ui_ctx->container, UI_COLOR_BG_PRIMARY, 0);
    lv_obj_set_style_border_width(s_ui_ctx->container, 0, 0);
    lv_obj_set_style_pad_all(s_ui_ctx->container, UI_PADDING_MEDIUM, 0);
    lv_obj_set_flex_flow(s_ui_ctx->container, LV_FLEX_FLOW_COLUMN);
    
    // Back button
    lv_obj_t *back_btn = ui_button_create_back(s_ui_ctx->container, back_button_cb);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Title
    lv_obj_t *title = lv_label_create(s_ui_ctx->container);
    lv_label_set_text(title, "Network Settings");
    lv_obj_set_style_text_font(title, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_pad_top(title, UI_PADDING_LARGE, 0);
    
    // WiFi toggle
    ui_toggle_config_t toggle_cfg = {
        .label = "WiFi", 
        .initial_state = false,
        .callback = wifi_toggle_cb
    };
    s_ui_ctx->wifi_toggle = ui_toggle_create(s_ui_ctx->container, &toggle_cfg);
    
    // Status label
    s_ui_ctx->status_label = lv_label_create(s_ui_ctx->container);
    lv_label_set_text(s_ui_ctx->status_label, "WiFi Off");
    lv_obj_set_style_text_font(s_ui_ctx->status_label, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(s_ui_ctx->status_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_pad_top(s_ui_ctx->status_label, UI_PADDING_SMALL, 0);
    
    // WiFi list container
    s_ui_ctx->wifi_list = lv_obj_create(s_ui_ctx->container);
    lv_obj_set_size(s_ui_ctx->wifi_list, LV_PCT(100), LV_PCT(60));
    lv_obj_set_style_bg_color(s_ui_ctx->wifi_list, UI_COLOR_BG_SECONDARY, 0);
    lv_obj_set_style_border_width(s_ui_ctx->wifi_list, 1, 0);
    lv_obj_set_style_border_color(s_ui_ctx->wifi_list, UI_COLOR_BORDER, 0);
    lv_obj_set_style_pad_all(s_ui_ctx->wifi_list, UI_PADDING_SMALL, 0);
    lv_obj_set_flex_flow(s_ui_ctx->wifi_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_ui_ctx->wifi_list, UI_PADDING_SMALL, 0);
    
    ESP_LOGI(TAG, "Network UI created");
    return s_ui_ctx->container;
}

void network_ui_destroy(lv_obj_t *ui) {
    if (s_ui_ctx) {
        free(s_ui_ctx);
        s_ui_ctx = NULL;
    }
    if (ui) lv_obj_del_async(ui);
    ESP_LOGI(TAG, "Network UI destroyed");
}

void network_ui_update_wifi_list(lv_obj_t *ui) {
    if (!s_ui_ctx || !s_ui_ctx->wifi_enabled) return;
    
    lv_obj_clean(s_ui_ctx->wifi_list);
    
    network_scan_result_t scan_result;
    if (network_scan_wifi(&scan_result) != ESP_OK) {
        lv_label_set_text(s_ui_ctx->status_label, "Scan failed");
        return;
    }
    
    lv_label_set_text_fmt(s_ui_ctx->status_label, "Found %d networks", scan_result.count);
    
    for (int i = 0; i < scan_result.count && i < 20; i++) {
        lv_obj_t *item = lv_obj_create(s_ui_ctx->wifi_list);
        lv_obj_set_size(item, LV_PCT(100), UI_LIST_ITEM_HEIGHT);
        lv_obj_set_style_bg_color(item, UI_COLOR_BG_PRIMARY, 0);
        lv_obj_set_style_bg_color(item, UI_COLOR_BG_SELECTED, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_radius(item, UI_RADIUS_SMALL, 0);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        
        // Signal strength icon
        const char *signal_icon = scan_result.networks[i].rssi > -50 ? LV_SYMBOL_WIFI : 
                                  scan_result.networks[i].rssi > -70 ? LV_SYMBOL_WARNING : 
                                  LV_SYMBOL_WARNING;
        
        lv_obj_t *label = lv_label_create(item);
        lv_label_set_text_fmt(label, "%s %s", signal_icon, scan_result.networks[i].ssid);
        lv_obj_set_style_text_font(label, UI_FONT_MEDIUM, 0);
        lv_obj_set_style_text_color(label, UI_COLOR_TEXT_PRIMARY, 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, UI_PADDING_MEDIUM, 0);
        
        // Lock icon for secured networks
        if (scan_result.networks[i].auth_mode != NETWORK_AUTH_OPEN) {
            lv_obj_t *lock_icon = lv_label_create(item);
            lv_label_set_text(lock_icon, LV_SYMBOL_SETTINGS);
            lv_obj_set_style_text_font(lock_icon, UI_FONT_SMALL, 0);
            lv_obj_set_style_text_color(lock_icon, UI_COLOR_TEXT_SECONDARY, 0);
            lv_obj_align(lock_icon, LV_ALIGN_RIGHT_MID, -UI_PADDING_MEDIUM, 0);
        }
        
        lv_obj_add_event_cb(item, wifi_item_click_cb, LV_EVENT_CLICKED, NULL);
    }
    
    ESP_LOGI(TAG, "WiFi list updated with %d networks", scan_result.count);
}
