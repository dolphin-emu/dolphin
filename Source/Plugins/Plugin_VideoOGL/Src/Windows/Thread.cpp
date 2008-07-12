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

#include "Thread.h"	

namespace W32Util
{
	Thread::Thread ( DWORD (WINAPI * pFun) (void* arg), void* pArg)
	{
		_handle = CreateThread (
			0, // Security attributes
			0, // Stack size
			pFun,
			pArg,
			CREATE_SUSPENDED,
			&_tid);
	}

	Thread::~Thread (void) 
	{ 
		if (_handle != NULL)
		{
			if (CloseHandle (_handle) == FALSE)
			{
				Terminate();
			}		
		}
	}

	void Thread::Resume (void) 
	{ 
		if (_handle != NULL)
			ResumeThread (_handle); 
	}

	void Thread::WaitForDeath (void)
	{
		if (_handle != NULL)
			WaitForSingleObject (_handle, 100);
	}

	void Thread::Terminate (void) 
	{ 
		if (_handle != NULL)
			TerminateThread (_handle, 0); 
		_handle = NULL;
	}

	void Thread::SetPriority (int _nPriority) 
	{ 
		if (_handle != NULL)
			SetThreadPriority(_handle, _nPriority); 
	}

	void Thread::Suspend (void) 
	{ 
		if (_handle != NULL)
			SuspendThread(_handle); 
	}
}