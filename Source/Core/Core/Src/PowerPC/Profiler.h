// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/


#ifndef _PROFILER_H
#define _PROFILER_H

#ifdef _WIN32

#ifdef _M_IX86
#define PROFILER_QUERY_PERFORMACE_COUNTER(pt)		\
					LEA(32, EAX, M(pt)); PUSH(EAX);	\
					CALL(QueryPerformanceCounter)
// TODO: r64 way
// asm write : (u64) dt += t1-t0 
#define PROFILER_ADD_DIFF_LARGE_INTEGER(pdt, pt1, pt0)	\
					MOV(32, R(EAX), M(pt1));	\
					SUB(32, R(EAX), M(pt0));	\
					MOV(32, R(ECX), M(((u8*)pt1) + 4));	\
					SBB(32, R(ECX), M(((u8*)pt0) + 4));	\
					ADD(32, R(EAX), M(pdt));	\
					MOV(32, R(EDX), M(((u8*)pdt) + 4));	\
					ADC(32, R(EDX), R(ECX));			\
					MOV(32, M(pdt), R(EAX));	\
					MOV(32, M(((u8*)pdt) + 4), R(EDX))

#define PROFILER_VPUSH	PUSH(EAX);PUSH(ECX);PUSH(EDX)
#define PROFILER_VPOP	POP(EDX);POP(ECX);POP(EAX)

#else

#define PROFILER_QUERY_PERFORMACE_COUNTER(pt) 
#define PROFILER_ADD_DIFF_LARGE_INTEGER(pdt, pt1, pt0) 
#define PROFILER_VPUSH
#define PROFILER_VPOP
#endif

#else
// TODO
#define PROFILER_QUERY_PERFORMACE_COUNTER(pt) 
#define PROFILER_ADD_DIFF_LARGE_INTEGER(pdt, pt1, pt0) 
#define PROFILER_VPUSH
#define PROFILER_VPOP
#endif


namespace Profiler
{
extern bool g_ProfileBlocks;
extern bool g_ProfileInstructions;

void WriteProfileResults(const char *filename);
}

#endif  // _PROFILER_H
