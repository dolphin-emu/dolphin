#ifndef RC_VALIDATE_H
#define RC_VALIDATE_H

#include "rc_runtime_types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int rc_validate_memrefs(const rc_memref_t* memref, char result[], const size_t result_size, unsigned max_address);

int rc_validate_condset(const rc_condset_t* condset, char result[], const size_t result_size, unsigned max_address);
int rc_validate_trigger(const rc_trigger_t* trigger, char result[], const size_t result_size, unsigned max_address);

#ifdef __cplusplus
}
#endif

#endif /* RC_VALIDATE_H */
