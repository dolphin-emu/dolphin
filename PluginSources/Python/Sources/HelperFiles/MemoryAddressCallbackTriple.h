#ifndef MEMORY_ADDRESS_TRIPLE
#define MEMORY_ADDRESS_TRIPLE

#include "IdentifierToCallback.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct MemoryAddressCallbackTriple
  {
    unsigned int memory_start_address;
    unsigned int memory_end_address;
    IdentifierToCallback identifier_to_callback;
  } MemoryAddressCallbackTriple;

#ifdef __cplusplus
}
#endif

#endif
