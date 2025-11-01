#ifndef SECURITY_H
#define SECURITY_H

#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

system_secure_key_t security_generate_key(void);

bool security_validate_key(system_secure_key_t key, system_secure_key_t stored_key);

void security_invalidate_key(system_secure_key_t *key);

#ifdef __cplusplus
}
#endif

#endif // SECURITY_H
