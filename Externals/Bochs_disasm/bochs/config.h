#ifndef _BOCHS_CONFIG_H
#define _BOCHS_CONFIG_H

#ifdef _WIN32
typedef signed __int8 Bit8s;
typedef signed __int16 Bit16s;
typedef signed __int32 Bit32s;
typedef signed __int64 Bit64s;

typedef unsigned __int8 Bit8u;
typedef unsigned __int16 Bit16u;
typedef unsigned __int32 Bit32u;
typedef unsigned __int64 Bit64u;

typedef bool bx_bool;
typedef Bit64u bx_address;

#define BX_CPP_INLINE inline

#else

#include <stdint.h>

typedef int8_t Bit8s;
typedef int16_t Bit16s;
typedef int32_t Bit32s;
typedef int64_t Bit64s;

typedef uint8_t Bit8u;
typedef uint16_t Bit16u;
typedef uint32_t Bit32u;
typedef uint64_t Bit64u;

typedef bool bx_bool;
typedef Bit64u bx_address;

#define BX_CPP_INLINE inline

#endif

#define BX_CONST64(x)  (x##LL)
#define GET32L(val64) ((Bit32u)(((Bit64u)(val64)) & 0xFFFFFFFF))
#define GET32H(val64) ((Bit32u)(((Bit64u)(val64)) >> 32))

#endif
