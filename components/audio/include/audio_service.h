#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AUDIO_EVENT_REGISTERED = 0,
    AUDIO_EVENT_STARTED,
    AUDIO_EVENT_STOPPED,
    AUDIO_EVENT_VOLUME_CHANGED,
    AUDIO_EVENT_PLAYBACK_STATE,
    AUDIO_EVENT_ERROR,
} audio_event_id_t;

typedef struct {
    uint8_t volume;
    bool muted;
} audio_volume_event_t;

typedef struct {
    bool playing;
    uint32_t position_ms;
    uint32_t duration_ms;
} audio_playback_event_t;

esp_err_t audio_service_init(void);

esp_err_t audio_service_deinit(void);

esp_err_t audio_service_start(void);

esp_err_t audio_service_stop(void);

esp_err_t audio_set_volume(uint8_t volume);

esp_err_t audio_get_volume(uint8_t *volume);

system_service_id_t audio_service_get_id(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_SERVICE_H
