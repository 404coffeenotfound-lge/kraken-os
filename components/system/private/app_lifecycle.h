/**
 * @file app_lifecycle.h
 * @brief Application lifecycle management utilities
 * 
 * Provides utilities for safe app lifecycle management including
 * automatic event unsubscription.
 */

#ifndef APP_LIFECYCLE_H
#define APP_LIFECYCLE_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Subscription Tracking
 * ============================================================================ */

/**
 * @brief Track app subscription
 * 
 * Records that an app has subscribed to an event type.
 * 
 * @param service_id App's service ID
 * @param event_type Event type subscribed to
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_lifecycle_track_subscription(system_service_id_t service_id,
                                            system_event_type_t event_type);

/**
 * @brief Untrack app subscription
 * 
 * Removes subscription tracking.
 * 
 * @param service_id App's service ID
 * @param event_type Event type unsubscribed from
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_lifecycle_untrack_subscription(system_service_id_t service_id,
                                              system_event_type_t event_type);

/**
 * @brief Unsubscribe all app events
 * 
 * Automatically unsubscribes all events when app stops.
 * 
 * @param service_id App's service ID
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_lifecycle_unsubscribe_all(system_service_id_t service_id);

/**
 * @brief Get app subscription count
 * 
 * @param service_id App's service ID
 * @param count Output subscription count
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_lifecycle_get_subscription_count(system_service_id_t service_id,
                                                uint32_t *count);

#ifdef __cplusplus
}
#endif

#endif // APP_LIFECYCLE_H
