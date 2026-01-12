/**
 * @file request_response.h
 * @brief Request/response event pattern
 * 
 * Provides synchronous request/response communication over async event bus.
 */

#ifndef REQUEST_RESPONSE_H
#define REQUEST_RESPONSE_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Request/Response Types
 * ============================================================================ */

/** Request ID type */
typedef uint32_t request_id_t;

/** Invalid request ID */
#define REQUEST_ID_INVALID 0

/** Request/response callback */
typedef void (*response_callback_t)(request_id_t request_id, 
                                     const void *response_data,
                                     size_t response_size,
                                     void *user_data);

/* ============================================================================
 * Request/Response API
 * ============================================================================ */

/**
 * @brief Send request and wait for response
 * 
 * Sends request event and blocks until response received or timeout.
 * 
 * @param target_service Service to send request to
 * @param request_type Request event type
 * @param request_data Request data
 * @param request_size Request data size
 * @param response_data Output response data buffer
 * @param response_size Input: buffer size, Output: actual response size
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if no response, error code otherwise
 */
esp_err_t request_send_sync(system_service_id_t target_service,
                             system_event_type_t request_type,
                             const void *request_data,
                             size_t request_size,
                             void *response_data,
                             size_t *response_size,
                             uint32_t timeout_ms);

/**
 * @brief Send request with async callback
 * 
 * @param target_service Service to send request to
 * @param request_type Request event type
 * @param request_data Request data
 * @param request_size Request data size
 * @param callback Response callback
 * @param user_data User data for callback
 * @param out_request_id Output request ID
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t request_send_async(system_service_id_t target_service,
                              system_event_type_t request_type,
                              const void *request_data,
                              size_t request_size,
                              response_callback_t callback,
                              void *user_data,
                              request_id_t *out_request_id);

/**
 * @brief Send response to request
 * 
 * Called by service handling request to send response.
 * 
 * @param request_id Request ID from request event
 * @param response_data Response data
 * @param response_size Response data size
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t request_send_response(request_id_t request_id,
                                 const void *response_data,
                                 size_t response_size);

/**
 * @brief Cancel pending request
 * 
 * @param request_id Request ID to cancel
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t request_cancel(request_id_t request_id);

#ifdef __cplusplus
}
#endif

#endif // REQUEST_RESPONSE_H
