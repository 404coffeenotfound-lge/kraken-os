/**
 * @file kraken.c
 * @brief Kraken main application entry point
 * 
 * This file initializes the system service and manages the application lifecycle.
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// System service headers
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"

// Application service headers
#include "audio_service.h"
#include "bluetooth_service.h"
#include "display_service.h"
#include "network_service.h"

// App manager and example apps
#include "system_service/app_manager.h"
#include "hello_app.h"
// goodbye_app will be loaded dynamically from partition
// #include "goodbye_app.h"

// Application tag for logging
static const char *TAG = "kraken";

// Store the system service secure key (keep private to main)
static system_secure_key_t g_system_secure_key = 0;

/**
 * @brief Print system information
 */
static void print_system_info(void)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    
    esp_chip_info(&chip_info);
    
    ESP_LOGI(TAG, "╔════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║              KRAKEN SYSTEM v1.0                    ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Hardware Information:");
    ESP_LOGI(TAG, "  Chip:         %s", CONFIG_IDF_TARGET);
    ESP_LOGI(TAG, "  Cores:        %d", chip_info.cores);
    ESP_LOGI(TAG, "  Silicon Rev:  %d", chip_info.revision);
    
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        ESP_LOGI(TAG, "  Flash:        %lu MB %s", 
                 flash_size / (1024 * 1024),
                 (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    }
    
    ESP_LOGI(TAG, "  Features:     WiFi%s%s%s",
             (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
             (chip_info.features & CHIP_FEATURE_IEEE802154) ? "/802.15.4" : "");
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Firmware Information:");
    ESP_LOGI(TAG, "  ESP-IDF:      %s", esp_get_idf_version());
    ESP_LOGI(TAG, "  Free heap:    %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "  Min free heap:%lu bytes", esp_get_minimum_free_heap_size());
    ESP_LOGI(TAG, "");
}

/**
 * @brief Initialize system service
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t init_system_service(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing system service...");
    
    // Initialize the system service and get secure key
    ret = system_service_init(&g_system_secure_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize system service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ System service initialized");
    ESP_LOGI(TAG, "  Secure key: 0x%08lX", g_system_secure_key);
    
    // Start the event processing task
    ret = system_service_start(g_system_secure_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start system service: %s", esp_err_to_name(ret));
        system_service_deinit(g_system_secure_key);
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ System service started");
    ESP_LOGI(TAG, "  Event processing task running");
    
    return ESP_OK;
}

/**
 * @brief Initialize application services
 * 
 * Add your service initialization calls here.
 * Each service should register itself with the system service.
 */
static void init_application_services(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing application services...");
    ESP_LOGI(TAG, "");
    
    // Initialize App Storage
    // DISABLED: We're loading apps directly from partition, not via FAT filesystem
    /*
    ret = app_storage_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize app storage");
    } else {
        ESP_LOGI(TAG, "✓ App storage initialized");
        
        // Mount storage partition
        ret = app_storage_mount();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount app storage (will format on first use)");
        } else {
            ESP_LOGI(TAG, "✓ App storage mounted");
        }
    }
    */
    ESP_LOGI(TAG, "✓ App storage disabled (using direct partition loading)");
    
    // Initialize App Manager
    ret = app_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize app manager");
    } else {
        ESP_LOGI(TAG, "✓ App manager initialized");
    }
    
    // Initialize Audio Service
    ret = audio_service_init();
    if (ret == ESP_OK) {
        audio_service_start();
    }
    
    // Initialize Bluetooth Service
    ret = bluetooth_service_init();
    if (ret == ESP_OK) {
        bluetooth_service_start();
    }
    
    // Initialize Display Service
    ret = display_service_init();
    if (ret == ESP_OK) {
        display_service_start();
    }
    
    // Initialize Network Service
    ret = network_service_init();
    if (ret == ESP_OK) {
        network_service_start();
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Registering built-in apps...");
    
    // Register Hello App (built-in)
    app_info_t *hello_info = NULL;
    ret = app_manager_register_app(&hello_app_manifest, &hello_info);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Registered 'hello' app (built-in)");
    }
    
    // NOTE: goodbye app will be loaded dynamically from partition (PSRAM)
    ESP_LOGI(TAG, "NOTE: goodbye app will be loaded dynamically at runtime");
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "✓ Application services initialized");
}

/**
 * @brief Print system statistics periodically
 */
static void print_system_stats(void)
{
    uint32_t total_services = 0;
    uint32_t total_events = 0;
    uint32_t total_subscriptions = 0;
    
    esp_err_t ret = system_service_get_stats(g_system_secure_key,
                                             &total_services,
                                             &total_events,
                                             &total_subscriptions);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
        ESP_LOGI(TAG, "System Statistics:");
        ESP_LOGI(TAG, "  Registered services: %lu", total_services);
        ESP_LOGI(TAG, "  Events processed:    %lu", total_events);
        ESP_LOGI(TAG, "  Active subscriptions:%lu", total_subscriptions);
        ESP_LOGI(TAG, "  Free heap:           %lu bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "  Uptime:              %llu seconds", esp_timer_get_time() / 1000000);
        
        // List all registered services
        if (total_services > 0) {
            // Allocate on heap to avoid stack overflow
            system_service_info_t *services = calloc(SYSTEM_SERVICE_MAX_SERVICES, sizeof(system_service_info_t));
            if (services == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for services list");
            } else {
                uint32_t count = 0;
                
                ret = system_service_list_all(services, SYSTEM_SERVICE_MAX_SERVICES, &count);
                ESP_LOGI(TAG, "system_service_list_all returned: %d, count: %lu", ret, count);
                if (ret == ESP_OK && count > 0) {
                    ESP_LOGI(TAG, "");
                    ESP_LOGI(TAG, "Registered Services (count=%lu):", count);
                    
                    // Sanity check: don't print more than we have
                    if (count > SYSTEM_SERVICE_MAX_SERVICES) {
                        ESP_LOGE(TAG, "ERROR: count (%lu) exceeds max (%d)!", count, SYSTEM_SERVICE_MAX_SERVICES);
                        count = SYSTEM_SERVICE_MAX_SERVICES;
                    }
                    
                    for (uint32_t i = 0; i < count; i++) {
                        ESP_LOGI(TAG, "Service %lu: name ptr=%p, first char=0x%02x",
                                 i, services[i].name, (unsigned char)services[i].name[0]);
                        
                        // Defensive: ensure name is valid before printing
                        if (services[i].name[0] == '\0') {
                            ESP_LOGI(TAG, "  [%d] <unnamed>           State: %d, Last HB: %lu ms",
                                     services[i].service_id,
                                     services[i].state,
                                     services[i].last_heartbeat);
                        } else {
                            ESP_LOGI(TAG, "  [%d] %-20s State: %d, Last HB: %lu ms",
                                     services[i].service_id,
                                     services[i].name,
                                     services[i].state,
                                     services[i].last_heartbeat);
                        }
                    }
                }
                free(services);
            }
        }
        ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
    }
}

/**
 * @brief Main application task
 * 
 * This task runs the main application loop.
 * Modify this to implement your application logic.
 */
static void main_task(void *pvParameters)
{
    uint32_t loop_count = 0;
    
    ESP_LOGI(TAG, "Main application task started");
    
    // Wait a bit then start the hello app
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "═══════════════════════════════════════");
    ESP_LOGI(TAG, "Starting hello app (built-in)...");
    ESP_LOGI(TAG, "═══════════════════════════════════════");
    app_manager_start_app("hello");
    
    // Wait for hello to finish
    vTaskDelay(pdMS_TO_TICKS(8000));
    
    // Load goodbye app dynamically from partition into PSRAM
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "═══════════════════════════════════════");
    ESP_LOGI(TAG, "Loading goodbye app dynamically (PSRAM)...");
    ESP_LOGI(TAG, "═══════════════════════════════════════");
    
    app_info_t *goodbye_info = NULL;
    // Load from partition at offset 0x0 (start of storage partition)
    // Storage partition starts at 0x410000
    // Code will be loaded into PSRAM but CANNOT be executed (hardware limitation)
    esp_err_t ret = app_manager_load_from_partition("storage", 0x0, &goodbye_info);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Successfully loaded goodbye app from partition");
        ESP_LOGI(TAG, "NOTE: App code is in PSRAM and CANNOT execute on ESP32-S3");
        ESP_LOGI(TAG, "      (PSRAM is not executable - hardware limitation)");
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "To make this work, you need to:");
        ESP_LOGI(TAG, "  1. Compile apps with -fPIC (position-independent code)");
        ESP_LOGI(TAG, "  2. Implement ELF loader with relocations");
        ESP_LOGI(TAG, "  3. Copy code to IRAM (limited ~200KB available)");
        ESP_LOGI(TAG, "  4. Or use interpreted approach (Lua/Python VM)");
        
        // Attempt to start will fail with InstructionFetchError
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "Attempting to start goodbye app (will fail)...");
        app_manager_start_app("goodbye");
    } else {
        ESP_LOGE(TAG, "✗ Failed to load goodbye app: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "Note: Build and upload goodbye.bin with:");
        ESP_LOGI(TAG, "  python3 build_dynamic_app.py goodbye");
        ESP_LOGI(TAG, "  esptool.py --port /dev/tty.usbmodem5AE70236261 write_flash 0x410000 build/apps/goodbye.bin");
    }
    
    vTaskDelay(pdMS_TO_TICKS(8000));
    
    // List all apps
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "═══════════════════════════════════════");
    ESP_LOGI(TAG, "Listing all registered apps:");
    ESP_LOGI(TAG, "═══════════════════════════════════════");
    
    app_info_t apps[APP_MAX_APPS];
    size_t count;
    if (app_manager_list_apps(apps, APP_MAX_APPS, &count) == ESP_OK) {
        for (size_t i = 0; i < count; i++) {
            ESP_LOGI(TAG, "[%d] %s v%s by %s - State: %d, Source: %d",
                     i,
                     apps[i].manifest.name,
                     apps[i].manifest.version,
                     apps[i].manifest.author,
                     apps[i].state,
                     apps[i].source);
        }
    }
    
    while (1) {
        loop_count++;
        
        // Print statistics every 30 seconds
        if (loop_count % 30 == 0) {
            print_system_stats();
        }
        
        // Sleep for 1 second
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    vTaskDelete(NULL);
}

/**
 * @brief Application main entry point
 * 
 * This is the entry point for the ESP-IDF application.
 * It initializes the system service and all application components.
 */
void app_main(void)
{
    esp_err_t ret;
    
    // Print system information
    print_system_info();
    
    // ========================================================================
    // STEP 1: Initialize System Service (REQUIRED FIRST)
    // ========================================================================
    ret = init_system_service();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CRITICAL: System service initialization failed!");
        ESP_LOGE(TAG, "System cannot continue. Rebooting in 5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
        return;
    }
    
    // Small delay to let system stabilize
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // ========================================================================
    // STEP 2: Initialize Application Services
    // ========================================================================
    // Now that system service is running, other components can:
    // - Register themselves as services
    // - Register event types
    // - Subscribe to events
    // - Post events
    
    init_application_services();
    
    // ========================================================================
    // STEP 3: Start Main Application Task
    // ========================================================================
    BaseType_t task_ret = xTaskCreate(
        main_task,              // Task function
        "main_task",            // Task name
        8192,                   // Stack size (bytes) - increased for stats printing
        NULL,                   // Parameters
        5,                      // Priority
        NULL                    // Task handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create main task!");
        return;
    }
    
    ESP_LOGI(TAG, "✓ Main task created");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║         KRAKEN SYSTEM READY                        ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    // Print initial statistics
    vTaskDelay(pdMS_TO_TICKS(1000));
    print_system_stats();
    
    // ========================================================================
    // Main thread can now be used for other purposes or just idle
    // ========================================================================
    // The application runs in main_task() and event processing runs in 
    // the system service task. This thread can be used for monitoring,
    // or you can just let it idle.
    
    // Optional: Monitor system health
    while (1) {
        // Sleep for a long time (app runs in main_task)
        vTaskDelay(pdMS_TO_TICKS(60000));  // 60 seconds
    }
    
    // ========================================================================
    // Cleanup (only reached if application exits - rare on embedded)
    // ========================================================================
    // ESP_LOGI(TAG, "Shutting down system service...");
    // system_service_stop(g_system_secure_key);
    // system_service_deinit(g_system_secure_key);
    // ESP_LOGI(TAG, "System service stopped");
}
