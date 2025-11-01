#include "system_service/service_manager.h"
#include "system_internal.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "service_manager";

esp_err_t system_service_register(const char *service_name,
                                   void *service_context,
                                   system_service_id_t *out_service_id)
{
    if (service_name == NULL || out_service_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_context_t *ctx = system_get_context();
    if (!ctx->initialized) {
        ESP_LOGE(TAG, "System service not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (ctx->service_count >= SYSTEM_SERVICE_MAX_SERVICES) {
        ESP_LOGE(TAG, "Maximum services reached");
        system_unlock();
        return ESP_ERR_NO_MEM;
    }
    
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (ctx->services[i].registered &&
            strcmp(ctx->services[i].name, service_name) == 0) {
            ESP_LOGE(TAG, "Service '%s' already registered", service_name);
            system_unlock();
            return ESP_ERR_INVALID_STATE;
        }
    }
    
    int slot = -1;
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (!ctx->services[i].registered) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        system_unlock();
        return ESP_ERR_NO_MEM;
    }
    
    strncpy(ctx->services[slot].name, service_name, SYSTEM_SERVICE_MAX_NAME_LEN - 1);
    ctx->services[slot].name[SYSTEM_SERVICE_MAX_NAME_LEN - 1] = '\0';
    ctx->services[slot].service_id = (system_service_id_t)slot;
    ctx->services[slot].state = SYSTEM_SERVICE_STATE_REGISTERED;
    ctx->services[slot].service_context = service_context;
    ctx->services[slot].last_heartbeat = (uint32_t)(esp_timer_get_time() / 1000);
    ctx->services[slot].registered = true;
    ctx->services[slot].event_count = 0;
    
    *out_service_id = ctx->services[slot].service_id;
    ctx->service_count++;
    
    system_unlock();
    
    ESP_LOGI(TAG, "Service '%s' registered with ID %d", service_name, *out_service_id);
    
    return ESP_OK;
}

esp_err_t system_service_unregister(system_service_id_t service_id)
{
    system_context_t *ctx = system_get_context();
    if (!ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (service_id >= SYSTEM_SERVICE_MAX_SERVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (!ctx->services[service_id].registered) {
        system_unlock();
        return ESP_ERR_NOT_FOUND;
    }
    
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SUBSCRIBERS; i++) {
        if (ctx->subscriptions[i].active &&
            ctx->subscriptions[i].service_id == service_id) {
            ctx->subscriptions[i].active = false;
            ctx->subscription_count--;
        }
    }
    
    memset(&ctx->services[service_id], 0, sizeof(service_entry_t));
    ctx->service_count--;
    
    system_unlock();
    
    ESP_LOGI(TAG, "Service ID %d unregistered", service_id);
    
    return ESP_OK;
}

esp_err_t system_service_set_state(system_service_id_t service_id,
                                    system_service_state_t state)
{
    system_context_t *ctx = system_get_context();
    if (!ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (service_id >= SYSTEM_SERVICE_MAX_SERVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (!ctx->services[service_id].registered) {
        system_unlock();
        return ESP_ERR_NOT_FOUND;
    }
    
    ctx->services[service_id].state = state;
    
    system_unlock();
    
    return ESP_OK;
}

esp_err_t system_service_get_state(system_service_id_t service_id,
                                    system_service_state_t *out_state)
{
    if (out_state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_context_t *ctx = system_get_context();
    if (!ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (service_id >= SYSTEM_SERVICE_MAX_SERVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (!ctx->services[service_id].registered) {
        system_unlock();
        return ESP_ERR_NOT_FOUND;
    }
    
    *out_state = ctx->services[service_id].state;
    
    system_unlock();
    
    return ESP_OK;
}

esp_err_t system_service_heartbeat(system_service_id_t service_id)
{
    system_context_t *ctx = system_get_context();
    if (!ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (service_id >= SYSTEM_SERVICE_MAX_SERVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (!ctx->services[service_id].registered) {
        system_unlock();
        return ESP_ERR_NOT_FOUND;
    }
    
    ctx->services[service_id].last_heartbeat = (uint32_t)(esp_timer_get_time() / 1000);
    
    system_unlock();
    
    return ESP_OK;
}

esp_err_t system_service_get_info(system_service_id_t service_id,
                                   system_service_info_t *out_info)
{
    if (out_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_context_t *ctx = system_get_context();
    if (!ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (service_id >= SYSTEM_SERVICE_MAX_SERVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (!ctx->services[service_id].registered) {
        system_unlock();
        return ESP_ERR_NOT_FOUND;
    }
    
    strncpy(out_info->name, ctx->services[service_id].name, SYSTEM_SERVICE_MAX_NAME_LEN);
    out_info->service_id = ctx->services[service_id].service_id;
    out_info->state = ctx->services[service_id].state;
    out_info->last_heartbeat = ctx->services[service_id].last_heartbeat;
    out_info->service_context = ctx->services[service_id].service_context;
    
    system_unlock();
    
    return ESP_OK;
}

esp_err_t system_service_list_all(system_service_info_t *out_services,
                                   uint32_t max_count,
                                   uint32_t *out_count)
{
    if (out_services == NULL || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_context_t *ctx = system_get_context();
    if (!ctx->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint32_t count = 0;
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES && count < max_count; i++) {
        if (ctx->services[i].registered) {
            strncpy(out_services[count].name, ctx->services[i].name, SYSTEM_SERVICE_MAX_NAME_LEN);
            out_services[count].service_id = ctx->services[i].service_id;
            out_services[count].state = ctx->services[i].state;
            out_services[count].last_heartbeat = ctx->services[i].last_heartbeat;
            out_services[count].service_context = ctx->services[i].service_context;
            count++;
        }
    }
    
    *out_count = count;
    
    system_unlock();
    
    return ESP_OK;
}
