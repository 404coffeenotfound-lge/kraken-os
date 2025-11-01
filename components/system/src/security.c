#include "security.h"
#include "esp_random.h"
#include "esp_timer.h"

system_secure_key_t security_generate_key(void)
{
    uint32_t random_val = esp_random();
    uint32_t timestamp = (uint32_t)(esp_timer_get_time() & 0xFFFFFFFF);
    
    system_secure_key_t key = random_val ^ timestamp;
    
    if (key == 0) {
        key = 0xDEADBEEF;
    }
    
    return key;
}

bool security_validate_key(system_secure_key_t key, system_secure_key_t stored_key)
{
    return (key != 0 && key == stored_key);
}

void security_invalidate_key(system_secure_key_t *key)
{
    if (key != NULL) {
        *key = 0;
    }
}
