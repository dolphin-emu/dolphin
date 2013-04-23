// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// This header contains type definitions that are shared between the Dolphin core and
// other parts of the code. Any definitions that are only used by the core should be
// placed in "Common.h" instead.

#ifndef _COMMONTYPES_H_
#define _COMMONTYPES_H_

#ifdef _WIN32

#include <tchar.h>

typedef unsigned __int8 u8;
typedef unsigned __int16 u16;
typedef unsigned __int32 u32;
typedef unsigned __int64 u64;

typedef signed __int8 s8;
typedef signed __int16 s16;
typedef signed __int32 s32;
typedef signed __int64 s64;

#else

#ifndef GEKKO

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

#endif
// For using windows lock code
#define TCHAR char
#define LONG int

#endif // _WIN32

#endif // _COMMONTYPES_H_
