// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>






typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef unsigned int uint;

typedef signed char sint8;
typedef signed short sint16;
typedef signed int sint32;
typedef signed long long sint64;

extern uint16 FetchOpcode();
extern void ErrorLog(const char* _fmt, ...);

inline uint16 swap16(uint16 x)
{
	return((x >> 8) | (x << 8));
}

#include "OutBuffer.h"

// TODO: reference additional headers your program requires here
