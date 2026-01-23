#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "power_service.h"
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"

static const char *TAG = "power_service";

// ADC Configuration
#define BATTERY_ADC_UNIT        ADC_UNIT_1
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_8  // GPIO 9
#define BATTERY_ADC_ATTEN       ADC_ATTEN_DB_12  // 0-3.3V range
#define BATTERY_ADC_WIDTH       ADC_BITWIDTH_12  // 12-bit resolution (0-4095)

// Battery voltage thresholds (in mV)
// These are the ACTUAL battery voltages (after voltage divider calculation)
#define BATTERY_VOLTAGE_MAX     4200  // 4.2V in mV (100%)
#define BATTERY_VOLTAGE_MIN     3000  // 3.0V in mV (0%)
#define BATTERY_VOLTAGE_NOMINAL 3700  // 3.7V in mV (50%)

// Voltage divider ratio
// Board has 2:1 voltage divider: ADC reads half of battery voltage
// Example: Battery 4.2V → ADC reads 2.1V → multiply by 2.0 → 4.2V
#define VOLTAGE_DIVIDER_RATIO   2.0f  // 2:1 divider

// Monitoring configuration
#define BATTERY_CHECK_INTERVAL_MS   5000  // Check every 5 seconds
#define ADC_SAMPLE_COUNT            10    // Average 10 samples for accuracy
#define CHARGING_THRESHOLD_MV       50    // Voltage increase threshold for charging detection

// Service state
static system_service_id_t power_service_id = 0;
static system_event_type_t power_events[6];
static bool initialized = false;
static bool running = false;

// ADC handles
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;

// Battery monitoring task
static TaskHandle_t battery_monitor_task_handle = NULL;

// Battery state
static uint8_t last_battery_level = 0;
static bool last_charging_state = false;
static int last_voltage_mv = 0;

/* Initialize ADC for battery monitoring */
static esp_err_t init_battery_adc(void)
{
    esp_err_t ret;
    
    // Configure ADC oneshot mode
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = BATTERY_ADC_WIDTH,
        .atten = BATTERY_ADC_ATTEN,
    };
    
    ret = adc_oneshot_config_channel(adc_handle, BATTERY_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config ADC channel: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(adc_handle);
        return ret;
    }
    
    // Initialize ADC calibration
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = BATTERY_ADC_WIDTH,
    };
    
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration not available, using raw values");
        adc_cali_handle = NULL;
    }
    
    ESP_LOGI(TAG, "✓ Battery ADC initialized (GPIO 9, Channel 8)");
    return ESP_OK;
}

/* Read battery voltage in millivolts */
static int read_battery_voltage_mv(void)
{
    if (!adc_handle) {
        return 0;
    }
    
    int total_voltage = 0;
    int valid_samples = 0;
    
    // Take multiple samples and average
    for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
        int adc_raw = 0;
        esp_err_t ret = adc_oneshot_read(adc_handle, BATTERY_ADC_CHANNEL, &adc_raw);
        
        if (ret == ESP_OK) {
            int voltage_mv = 0;
            
            if (adc_cali_handle) {
                // Use calibrated value
                adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage_mv);
            } else {
                // Estimate voltage from raw value (12-bit ADC, 3.3V reference)
                voltage_mv = (adc_raw * 3300) / 4095;
            }
            
            total_voltage += voltage_mv;
            valid_samples++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // Small delay between samples
    }
    
    if (valid_samples == 0) {
        return 0;
    }
    
    // Average voltage
    int avg_voltage_mv = total_voltage / valid_samples;
    
    // Floating GPIO detection disabled - causing false positives
    // The voltage readings around 2.0V are actually valid battery voltages
    // (battery voltage after ADC attenuation, not floating GPIO)
    
    // Apply voltage divider ratio to get actual battery voltage
    int battery_voltage_mv = (int)(avg_voltage_mv * VOLTAGE_DIVIDER_RATIO);
    
    return battery_voltage_mv;
}

/* Convert voltage to battery percentage */
static uint8_t voltage_to_percentage(int voltage_mv)
{
    if (voltage_mv >= BATTERY_VOLTAGE_MAX) {
        return 100;
    }
    if (voltage_mv <= BATTERY_VOLTAGE_MIN) {
        return 0;
    }
    
    // Linear interpolation between min and max
    int range = BATTERY_VOLTAGE_MAX - BATTERY_VOLTAGE_MIN;
    int value = voltage_mv - BATTERY_VOLTAGE_MIN;
    uint8_t raw_percentage = (value * 100) / range;
    
    // Apply hysteresis to reduce fluctuations
    // Only update if percentage changes by more than 2%
    static uint8_t last_reported_percentage = 0;
    static bool first_reading = true;
    
    if (first_reading) {
        last_reported_percentage = raw_percentage;
        first_reading = false;
        return raw_percentage;
    }
    
    // Only update if change is significant (>2%)
    if (abs(raw_percentage - last_reported_percentage) >= 2) {
        last_reported_percentage = raw_percentage;
    }
    
    return last_reported_percentage;
}

/* Detect if battery is charging based on voltage trend */
static bool detect_charging(int current_voltage_mv)
{
    static int voltage_history[5] = {0};  // Increased to 5 samples for better trend
    static int history_index = 0;
    static int sample_count = 0;
    
    // Store voltage in history
    voltage_history[history_index] = current_voltage_mv;
    history_index = (history_index + 1) % 5;
    sample_count++;
    
    // Need at least 4 samples to detect trend reliably
    if (sample_count < 4) {
        return false;
    }
    
    // Calculate average voltage change over last 4 samples
    int total_change = 0;
    int valid_changes = 0;
    
    for (int i = 0; i < 4; i++) {
        int curr_idx = (history_index + i) % 5;
        int next_idx = (history_index + i + 1) % 5;
        
        if (voltage_history[curr_idx] > 0 && voltage_history[next_idx] > 0) {
            int change = voltage_history[next_idx] - voltage_history[curr_idx];
            total_change += change;
            valid_changes++;
        }
    }
    
    if (valid_changes == 0) {
        return false;
    }
    
    // Average voltage change per sample
    int avg_change_mv = total_change / valid_changes;
    
    // Adaptive threshold based on battery level
    // At high battery levels (>80%), charging is slower, so use lower threshold
    int threshold_mv;
    if (current_voltage_mv >= 4000) {  // >95% battery
        threshold_mv = 2;   // Very sensitive for trickle charge (lowered from 5)
    } else if (current_voltage_mv >= 3900) {  // >80% battery  
        threshold_mv = 5;   // Moderately sensitive (lowered from 10)
    } else {
        threshold_mv = 10;  // Normal threshold for fast charging (lowered from 20)
    }
    
    // Charging if average voltage is increasing above threshold
    bool is_charging = (avg_change_mv > threshold_mv);
    
    // Debug logging
    ESP_LOGI(TAG, "Charging detection: voltage=%dmV, avg_change=%dmV, threshold=%dmV, charging=%d",
             current_voltage_mv, avg_change_mv, threshold_mv, is_charging);
    
    // Check if battery is actually connected
    // If voltage is very low (<2.5V = 1.25V at ADC), battery is likely disconnected
    if (current_voltage_mv < 2500) {
        // No battery or battery critically low - not charging
        ESP_LOGI(TAG, "Battery voltage too low - assuming disconnected");
        return false;
    }
    
    // Special case: If voltage is stable at high level (>4.0V) and not decreasing,
    // assume charging since battery shouldn't be stable at high voltage on battery power alone
    if (current_voltage_mv >= 4000 && avg_change_mv >= -2) {
        // Voltage stable or slightly increasing at high level - likely charging/maintaining
        ESP_LOGI(TAG, "Battery at high voltage and stable - assuming charging");
        is_charging = true;
    }
    
    // Also check if voltage is stable at very high level (>4.15V), 
    // likely at full charge and maintaining
    if (current_voltage_mv >= 4150 && avg_change_mv >= -5 && avg_change_mv <= 5) {
        // Voltage stable at very high level - likely on charger maintaining full charge
        ESP_LOGI(TAG, "Battery at full charge - assuming charging/maintaining");
        is_charging = true;
    }
    
    return is_charging;
}

/* Battery monitoring task */
static void battery_monitor_task(void *arg)
{
    ESP_LOGI(TAG, "Battery monitor task started");
    
    while (running) {
        // Read battery voltage
        int voltage_mv = read_battery_voltage_mv();
        
        if (voltage_mv > 0) {
            // Calculate battery percentage
            uint8_t battery_level = voltage_to_percentage(voltage_mv);
            
            // Detect charging status
            bool is_charging = detect_charging(voltage_mv);
            
            // Log battery status
            ESP_LOGI(TAG, "Battery: %d%% (%.2fV) %s", 
                     battery_level, 
                     voltage_mv / 1000.0f,
                     is_charging ? "[CHARGING]" : "[NOT CHARGING]");
            
            // Post battery level event if changed
            if (battery_level != last_battery_level) {
                power_battery_event_t event_data = {
                    .level = battery_level,
                    .is_charging = is_charging
                };
                
                system_event_post(power_service_id, power_events[4], 
                                &event_data, sizeof(event_data), 
                                SYSTEM_EVENT_PRIORITY_NORMAL);
                
                last_battery_level = battery_level;
            }
            
            // Post charging status event if changed
            if (is_charging != last_charging_state) {
                power_battery_event_t event_data = {
                    .level = battery_level,
                    .is_charging = is_charging
                };
                
                system_event_post(power_service_id, power_events[5], 
                                &event_data, sizeof(event_data), 
                                SYSTEM_EVENT_PRIORITY_NORMAL);
                
                last_charging_state = is_charging;
                ESP_LOGI(TAG, "Charging status changed: %s", 
                         is_charging ? "CHARGING" : "NOT CHARGING");
            }
            
            last_voltage_mv = voltage_mv;
            
            // Send heartbeat
            system_service_heartbeat(power_service_id);
        } else {
            ESP_LOGW(TAG, "Failed to read battery voltage");
        }
        
        // Wait for next check interval
        vTaskDelay(pdMS_TO_TICKS(BATTERY_CHECK_INTERVAL_MS));
    }
    
    ESP_LOGI(TAG, "Battery monitor task stopped");
    vTaskDelete(NULL);
}

esp_err_t power_service_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Power service already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing power service...");
    
    // Register with system service
    esp_err_t ret = system_service_register("power_service", NULL, &power_service_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register with system service");
        return ret;
    }
    ESP_LOGI(TAG, "✓ Registered with system service (ID: %d)", power_service_id);
    
    // Register event types
    const char *event_names[] = {
        "power.registered",
        "power.started",
        "power.stopped",
        "power.error",
        "power.battery_level",
        "power.battery_status"
    };
    
    for (int i = 0; i < 6; i++) {
        ret = system_event_register_type(event_names[i], &power_events[i]);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to register event type '%s' - continuing anyway", event_names[i]);
            // Continue anyway - events are nice to have but not critical for battery monitoring
        }
    }
    ESP_LOGI(TAG, "✓ Event registration complete");
    
    // Initialize battery ADC
    ret = init_battery_adc();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize battery ADC");
        system_service_unregister(power_service_id);
        return ret;
    }
    
    // Set service state
    system_service_set_state(power_service_id, SYSTEM_SERVICE_STATE_REGISTERED);
    
    // Post registered event
    system_event_post(power_service_id, power_events[0], NULL, 0, SYSTEM_EVENT_PRIORITY_NORMAL);
    ESP_LOGI(TAG, "  → Posted POWER_EVENT_REGISTERED");
    
    initialized = true;
    ESP_LOGI(TAG, "✓ Power service initialized successfully");
    
    return ESP_OK;
}

esp_err_t power_service_start(void)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Power service not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (running) {
        ESP_LOGW(TAG, "Power service already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting power service...");
    
    running = true;
    
    // Create battery monitoring task
    BaseType_t task_created = xTaskCreate(
        battery_monitor_task,
        "battery_monitor",
        4096,
        NULL,
        5,  // Priority
        &battery_monitor_task_handle
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create battery monitor task");
        running = false;
        return ESP_FAIL;
    }
    
    // Set service state
    system_service_set_state(power_service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Post started event
    system_event_post(power_service_id, power_events[1], NULL, 0, SYSTEM_EVENT_PRIORITY_NORMAL);
    ESP_LOGI(TAG, "  → Posted POWER_EVENT_STARTED");
    
    ESP_LOGI(TAG, "✓ Power service started");
    
    return ESP_OK;
}

esp_err_t power_service_stop(void)
{
    if (!running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping power service...");
    
    running = false;
    
    // Wait for task to finish
    if (battery_monitor_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));  // Give task time to exit
        battery_monitor_task_handle = NULL;
    }
    
    // Set service state
    system_service_set_state(power_service_id, SYSTEM_SERVICE_STATE_REGISTERED);
    
    // Post stopped event
    system_event_post(power_service_id, power_events[2], NULL, 0, SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Power service stopped");
    
    return ESP_OK;
}

esp_err_t power_service_deinit(void)
{
    if (running) {
        power_service_stop();
    }
    
    if (!initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing power service...");
    
    // Cleanup ADC
    if (adc_cali_handle) {
        adc_cali_delete_scheme_curve_fitting(adc_cali_handle);
        adc_cali_handle = NULL;
    }
    
    if (adc_handle) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }
    
    // Unregister from system service
    system_service_unregister(power_service_id);
    
    initialized = false;
    ESP_LOGI(TAG, "✓ Power service deinitialized");
    
    return ESP_OK;
}

system_service_id_t power_service_get_id(void)
{
    return power_service_id;
}