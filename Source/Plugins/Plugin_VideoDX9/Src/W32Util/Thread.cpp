#include "Thread.h"	

namespace W32Util
{
	// __________________________________________________________________________________________________
	// Constructor
	//
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
	// __________________________________________________________________________________________________
	// Destructor
	//
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

	// __________________________________________________________________________________________________
	// Resume
	//
	void 
	Thread::Resume (void) 
	{ 
		if (_handle != NULL)
			ResumeThread (_handle); 
	}

	// __________________________________________________________________________________________________
	// WaitForDeath
	//
	void 
	Thread::WaitForDeath (void)
	{
		if (_handle != NULL)
			WaitForSingleObject (_handle, 100);
	}

	// __________________________________________________________________________________________________
	// Terminate
	//
	void 
	Thread::Terminate (void) 
	{ 
		if (_handle != NULL)
			TerminateThread (_handle, 0); 
		_handle = NULL;
	}

	// __________________________________________________________________________________________________
	// SetPriority
	//
	void 
	Thread::SetPriority (int _nPriority) 
	{ 
		if (_handle != NULL)
			SetThreadPriority(_handle, _nPriority); 
	}

	// __________________________________________________________________________________________________
	// Suspend
	//
	void 
	Thread::Suspend (void) 
	{ 
		if (_handle != NULL)
			SuspendThread(_handle); 
	}
}