/**
 * @file example_service.c
 * @brief Complete example of a production-ready Kraken-OS service
 * 
 * This example demonstrates all best practices for service development:
 * - Watchdog registration
 * - Resource quotas
 * - Event handling
 * - Heartbeat management
 * - Error handling
 * - Thread safety
 */

#include "example_service.h"
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "service_watchdog.h"
#include "resource_quota.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "example_service";

/* ============================================================================
 * Service State
 * ============================================================================ */

static system_service_id_t example_service_id = 0;
static system_event_type_t example_events[4];
static bool initialized = false;
static bool running = false;

/* Task handle */
static TaskHandle_t worker_task_handle = NULL;

/* Thread safety */
static SemaphoreHandle_t state_mutex = NULL;

/* Service data */
static uint32_t data_counter = 0;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void worker_task(void *arg);
static void example_event_handler(const system_event_t *event, void *user_data);

/* ============================================================================
 * Service Initialization
 * ============================================================================ */

esp_err_t example_service_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Service already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing example service...");
    
    /* Create mutex for thread safety */
    state_mutex = xSemaphoreCreateMutex();
    if (state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    /* Register with system service */
    ret = system_service_register("example_service", NULL, &example_service_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register with system: %s", esp_err_to_name(ret));
        vSemaphoreDelete(state_mutex);
        return ret;
    }
    ESP_LOGI(TAG, "✓ Registered with system (ID: %d)", example_service_id);
    
    /* Register event types */
    const char *event_names[] = {
        "example.registered",
        "example.started",
        "example.data_ready",
        "example.error"
    };
    
    for (int i = 0; i < 4; i++) {
        ret = system_event_register_type(event_names[i], &example_events[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register event '%s'", event_names[i]);
            system_service_unregister(example_service_id);
            vSemaphoreDelete(state_mutex);
            return ret;
        }
    }
    ESP_LOGI(TAG, "✓ Registered %d event types", 4);
    
    /* Register with watchdog */
    service_watchdog_config_t watchdog_config = {
        .timeout_ms = 30000,              // 30 second timeout
        .auto_restart = true,             // Auto-restart on failure
        .max_restart_attempts = 3,        // Max 3 restart attempts
        .is_critical = false              // Not a critical service
    };
    watchdog_register_service(example_service_id, &watchdog_config);
    ESP_LOGI(TAG, "✓ Registered with watchdog (30s timeout)");
    
    /* Set resource quotas */
    service_quota_t quota = {
        .max_events_per_sec = 50,         // Max 50 events/sec
        .max_subscriptions = 8,           // Max 8 subscriptions
        .max_event_data_size = 256,       // Max 256 bytes per event
        .max_memory_bytes = 32 * 1024     // Max 32KB memory
    };
    quota_set(example_service_id, &quota);
    ESP_LOGI(TAG, "✓ Resource quotas set (50 events/s, 32KB memory)");
    
    /* Subscribe to system events (example) */
    ret = system_event_subscribe(example_service_id, 
                                  example_events[EXAMPLE_EVENT_DATA_READY],
                                  example_event_handler,
                                  NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to subscribe to events: %s", esp_err_to_name(ret));
    }
    
    /* Set service state */
    system_service_set_state(example_service_id, SYSTEM_SERVICE_STATE_REGISTERED);
    
    initialized = true;
    
    /* Post registration event */
    system_event_post(example_service_id, 
                     example_events[EXAMPLE_EVENT_REGISTERED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Example service initialized successfully");
    
    return ESP_OK;
}

/* ============================================================================
 * Service Start/Stop
 * ============================================================================ */

esp_err_t example_service_start(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (running) {
        ESP_LOGW(TAG, "Service already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting example service...");
    
    /* Update state */
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    running = true;
    xSemaphoreGive(state_mutex);
    
    /* Create worker task */
    BaseType_t task_ret = xTaskCreate(
        worker_task,
        "example_worker",
        4096,
        NULL,
        5,
        &worker_task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create worker task");
        running = false;
        return ESP_FAIL;
    }
    
    /* Update service state */
    system_service_set_state(example_service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    /* Send heartbeat */
    system_service_heartbeat(example_service_id);
    
    /* Post started event */
    system_event_post(example_service_id,
                     example_events[EXAMPLE_EVENT_STARTED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Example service started");
    
    return ESP_OK;
}

esp_err_t example_service_stop(void)
{
    if (!initialized || !running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping example service...");
    
    /* Update state */
    system_service_set_state(example_service_id, SYSTEM_SERVICE_STATE_STOPPING);
    
    /* Stop worker task */
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    running = false;
    xSemaphoreGive(state_mutex);
    
    /* Wait for task to finish */
    if (worker_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));  // Give task time to exit
        worker_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "✓ Example service stopped");
    
    return ESP_OK;
}

esp_err_t example_service_deinit(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing example service...");
    
    /* Stop if running */
    if (running) {
        example_service_stop();
    }
    
    /* Unsubscribe from events */
    system_event_unsubscribe(example_service_id, 
                             example_events[EXAMPLE_EVENT_DATA_READY]);
    
    /* Unregister from system */
    system_service_unregister(example_service_id);
    
    /* Cleanup mutex */
    if (state_mutex != NULL) {
        vSemaphoreDelete(state_mutex);
        state_mutex = NULL;
    }
    
    initialized = false;
    
    ESP_LOGI(TAG, "✓ Example service deinitialized");
    
    return ESP_OK;
}

/* ============================================================================
 * Service Operations
 * ============================================================================ */

esp_err_t example_service_process_data(uint32_t value)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Send heartbeat (keeps watchdog happy) */
    system_service_heartbeat(example_service_id);
    
    /* Thread-safe update */
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    data_counter++;
    xSemaphoreGive(state_mutex);
    
    /* Prepare event data */
    example_data_event_t event_data = {
        .value = value,
        .timestamp = esp_timer_get_time() / 1000,
        .counter = data_counter
    };
    
    /* Post event with appropriate priority */
    esp_err_t ret = system_event_post(example_service_id,
                                      example_events[EXAMPLE_EVENT_DATA_READY],
                                      &event_data,
                                      sizeof(event_data),
                                      SYSTEM_EVENT_PRIORITY_HIGH);
    
    if (ret == ESP_ERR_QUOTA_EVENTS_EXCEEDED) {
        ESP_LOGW(TAG, "Event rate limit exceeded, throttling");
        vTaskDelay(pdMS_TO_TICKS(100));
        return ret;
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post event: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Data processed: value=%lu, counter=%lu", value, data_counter);
    
    return ESP_OK;
}

uint32_t example_service_get_counter(void)
{
    uint32_t counter;
    
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    counter = data_counter;
    xSemaphoreGive(state_mutex);
    
    return counter;
}

system_service_id_t example_service_get_id(void)
{
    return example_service_id;
}

/* ============================================================================
 * Worker Task
 * ============================================================================ */

static void worker_task(void *arg)
{
    ESP_LOGI(TAG, "Worker task started");
    
    uint32_t iteration = 0;
    
    while (true) {
        /* Check if we should stop */
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        bool should_run = running;
        xSemaphoreGive(state_mutex);
        
        if (!should_run) {
            break;
        }
        
        /* Do work */
        iteration++;
        
        /* Process some data every 5 seconds */
        if (iteration % 5 == 0) {
            example_service_process_data(iteration);
        }
        
        /* Send heartbeat every iteration */
        system_service_heartbeat(example_service_id);
        
        /* Sleep for 1 second */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "Worker task exiting");
    vTaskDelete(NULL);
}

/* ============================================================================
 * Event Handler
 * ============================================================================ */

static void example_event_handler(const system_event_t *event, void *user_data)
{
    ESP_LOGI(TAG, "Event received: %s", event->name);
    
    if (event->data_size == sizeof(example_data_event_t)) {
        example_data_event_t *data = (example_data_event_t *)event->data;
        ESP_LOGI(TAG, "  Value: %lu, Timestamp: %lu, Counter: %lu",
                 data->value, data->timestamp, data->counter);
    }
    
    /* Send heartbeat after processing event */
    system_service_heartbeat(example_service_id);
}
