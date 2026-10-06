#ifndef AILO_ERROR_INTERNAL_H
#define AILO_ERROR_INTERNAL_H
#include "ailo/base.h"
#include <stddef.h>

/* Private error helper; intentionally absent from the dynamic export set. */
#ifdef __cplusplus
extern "C" {
#endif
ailo_status ailo_error_set(ailo_error *error, ailo_status status,
                           const char *message);
#ifdef __cplusplus
}
#endif
#endif
