#include <stddef.h>
#include "../../../components/system/include/system_service/app_manager.h"

const app_manifest_t minimal_app_manifest __attribute__((section(".data.rel.ro"))) = {
    .name = "minimal",
    .version = "1.0.0",
    .author = "Test",
    .entry = NULL,
    .exit = NULL,
    .user_data = NULL
};

esp_err_t minimal_app_entry(app_context_t *ctx) {
    return 0;
}
