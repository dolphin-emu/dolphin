// Copyright (C) 2003-2008 Dolphin Project.

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

// TODO: create a OS-neutral version of this file and put it in Common.

#ifdef _WIN32

#include <windows.h>

#include <vector>

#include "Common.h"
#include "x64Analyzer.h"
#include "PowerPC/Jit64/Jit.h"

#include "MemTools.h"

//#ifdef ASDFKJLASJDF
namespace EMM
{
/* DESIGN

THIS IS NOT THE CURRENT STATE OF THIS FILE - IT'S UNFINISHED

We grab 4GB of virtual address space, and locate memories in there. The memories are either
VirtualAlloc or mapped swapfile.

I/O areas are mapped into the virtual memspace, and VirtualProtected where necessary.

Every chunk is mapped twice into memory, once into the virtual memspace, and once elsewhere.
This second mapping is used when a "read+writable" pointer is requested for a region. This 
would generally be for internal use by IO functions, and for actually performing the writes 
and reads after detecting them.

There is individual read and write protection for each chunk of memory.

Every region has a default read-write handler. If an exception is caught, this is executed.

The default read-write handlers use the "writable" pointers. 

There should be a method to mark a region for "write notification". Dynarecs can use this
to flush their code caches if a region is written to.

At this moment, there can only be one wrapped memspace at a time.
*/

DWORD_PTR memspaceBottom = 0;
DWORD_PTR memspaceTop = 0;

enum MSFlags
{
	MEMSPACE_MIRROR_FIRST_PART  = 1,
	MEMSPACE_MIRROR_OF_PREVIOUS = 2,
	MEMSPACE_MAPPED_HARDWARE = 4,
};

struct MemSpaceEntry
{
	u64 emulatedBase;
	u64 emulatedSize;
	u32 flags;
};

#define MEGABYTE 1024*1024

const MemSpaceEntry GCMemSpace[] =
{
	{0x80000000, 24*MEGABYTE, MEMSPACE_MIRROR_FIRST_PART},
	{0xC0000000, 24*MEGABYTE, MEMSPACE_MIRROR_OF_PREVIOUS},
	{0xCC000000,     0x10000, MEMSPACE_MAPPED_HARDWARE},
	{0xE0000000,      0x4000, 0}, //cache
};

struct Watch
{
	int ID;
	EAddr startAddr;
	EAddr endAddr;
	WR watchFor;
	WatchCallback callback;
	WatchType type;
	u64 userData;
};

std::vector<Watch> watches;



void UpdateProtection(EAddr startAddr, EAddr endAddr)
{
	
}


int AddWatchRegion(EAddr startAddr, EAddr endAddr, WR watchFor, WatchType type, WatchCallback callback, u64 userData)
{
	static int watchIDGen = 0;

	Watch watch;
	watch.ID = watchIDGen++;
	watch.startAddr = startAddr;
	watch.endAddr = endAddr;
	watch.watchFor = watchFor;
	watch.callback = callback;
	watch.userData = userData;
	watch.type = type;
	watches.push_back(watch);
	UpdateProtection(startAddr, endAddr);

	return watch.ID;
}



void Notify(EAddr address, WR action)
{
	for (std::vector<Watch>::iterator iter = watches.begin(); iter != watches.end(); ++iter)
	{
		if (action & iter->type)
		{
			if (address >= iter->startAddr && address < iter->endAddr)
			{
				//Alright!
				iter->callback(address, Access32 /*TODO*/, action, iter->ID);
			}
		}
	}
}


class MemSpace
{
	MemSpaceEntry *entries;

	u64 emulatedBottom;
	u64 emulatedTop;
	u64 emulatedSize;

	void *virtualBase;

public:

	void Init(const MemSpaceEntry *e, int count)
	{
		/*
		//first pass: figure out minimum address, and total amount of allocated memory
		emulatedBase = 0xFFFFFFFFFFFFFFFFL;
		emulatedTop = 0;

		u64 mappedTotal = 0;
		for (int i=0; i<count; i++)
		{
			if (e[i].emulatedBase < emulatedBase)
				emulatedBase = e[i].emulatedBase;
			if (e[i].emulatedBase+e[i].emulatedSize > emulatedTop)
				emulatedTop = e[i].emulatedBase+e[i].emulatedSize;
			if (e[i].flags & MEMSPACE_MIRROR_FIRST_PART)
			{
				mappedTotal += e[i].emulatedSize;
			}
		}
		emulatedSize = emulatedTop - emulatedBase;

		// The above stuff is not used atm - we just grab 4G
		
		//second pass: grab 4G of virtual address space
		virtualBase = VirtualAlloc(0, 0x100000000L, MEM_RESERVE, PAGE_READWRITE);

		//also grab a bunch of virtual memory while we're at it


		//Release the 4G space! 
		//Let's hope no weirdo thread klomps in here and grabs it
		VirtualFree(base, 0, MEM_RELEASE);

		for (int i=0; i<count; i++)
		{
			if (e[i].flags & MEMSPACE_MIRROR_FIRST_PART)
			{
				
			}
		}

		//TODO: fill all empty parts of the address space with no-access virtualalloc space
*/
	}

	u64   GetVirtualBaseAddr() {return (u64)virtualBase;}
	void *GetVirtualBase()     {return virtualBase;}

	void Shutdown()
	{

	}
};

LONG NTAPI Handler(PEXCEPTION_POINTERS pPtrs)
{
	switch (pPtrs->ExceptionRecord->ExceptionCode)
	{
	case EXCEPTION_ACCESS_VIOLATION:
		{
			int accessType = (int)pPtrs->ExceptionRecord->ExceptionInformation[0];
			if (accessType == 8) //Rule out DEP
			{
				MessageBox(0, _T("Tried to execute code that's not marked executable. This is likely a JIT bug.\n"), 0, 0);
				return EXCEPTION_CONTINUE_SEARCH;
			}
			
			//Where in the x86 code are we?
			PVOID codeAddr = pPtrs->ExceptionRecord->ExceptionAddress;
			unsigned char *codePtr = (unsigned char*)codeAddr;
			
			if (!Jit64::IsInJitCode(codePtr)) {
				// Let's not prevent debugging.
				return (DWORD)EXCEPTION_CONTINUE_SEARCH;
			}

			//Figure out what address was hit
			DWORD_PTR badAddress = (DWORD_PTR)pPtrs->ExceptionRecord->ExceptionInformation[1];
			//TODO: First examine the address, make sure it's within the emulated memory space
			if (badAddress < memspaceBottom) {
				PanicAlert("Exception handler - access below memory space. %08x%08x",
					badAddress >> 32, badAddress);
			}
			u32 emAddress = (u32)(badAddress - memspaceBottom);

			//Now we have the emulated address.
			//_assert_msg_(DYNA_REC,0,"MT : %08x",emAddress);

			//Let's notify everyone who wants to be notified
			//Notify(emAddress, accessType == 0 ? Read : Write);

			CONTEXT *ctx = pPtrs->ContextRecord;
			//opportunity to change the debug regs!

			//We could emulate the memory accesses here, but then they would still be around to take up
			//execution resources. Instead, we backpatch and retry.
			Jit64::BackPatch(codePtr, accessType);
			
			// We no longer touch Rip, since we return back to the instruction, after overwriting it with a
			// trampoline jump and some nops
			//ctx->Rip = (DWORD_PTR)codeAddr + info.instructionSize;
		}
		return (DWORD)EXCEPTION_CONTINUE_EXECUTION;

	case EXCEPTION_STACK_OVERFLOW:
		MessageBox(0, _T("Stack overflow!"), 0,0);
		return EXCEPTION_CONTINUE_SEARCH;

	case EXCEPTION_ILLEGAL_INSTRUCTION:
		//No SSE support? Or simply bad codegen?
		return EXCEPTION_CONTINUE_SEARCH;
		
	case EXCEPTION_PRIV_INSTRUCTION:
		//okay, dynarec codegen is obviously broken.
		return EXCEPTION_CONTINUE_SEARCH;

	case EXCEPTION_IN_PAGE_ERROR:
		//okay, something went seriously wrong, out of memory?
		return EXCEPTION_CONTINUE_SEARCH;

	case EXCEPTION_BREAKPOINT:
		//might want to do something fun with this one day?
		return EXCEPTION_CONTINUE_SEARCH;

	default:
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

void InstallExceptionHandler()
{
#ifdef _M_X64
	// Make sure this is only called once per process execution
	// Instead, could make a Uninstall function, but whatever..
	static bool handlerInstalled = false;
	if (handlerInstalled)
		return;

	AddVectoredExceptionHandler(TRUE, Handler);
	handlerInstalled = true;
#endif
}

}
#else

namespace EMM {

void InstallExceptionHandler()
{
/*
 * signal(xyz);
 */
}

}

#endif

