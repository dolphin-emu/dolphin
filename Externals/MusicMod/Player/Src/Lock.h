////////////////////////////////////////////////////////////////////////////////
// Plainamp, Open source Winamp core
// 
// Copyright © 2005  Sebastian Pipping <webmaster@hartwork.org>
// 
// -->  http://www.hartwork.org
// 
// This source code is released under the GNU General Public License (GPL).
// See GPL.txt for details. Any non-GPL usage is strictly forbidden.
////////////////////////////////////////////////////////////////////////////////


#ifndef PA_LOCK_H
#define PA_LOCK_H



#include "Global.h"



// INFO: http://www.devarticles.com/c/a/Cplusplus/Multithreading-in-C/3/


// #define LOCK_USES_MUTEX



////////////////////////////////////////////////////////////////////////////////
/// Lock for thread synchronization
////////////////////////////////////////////////////////////////////////////////
class Lock
{
public:
	Lock( TCHAR * szName );
	~Lock();
	
	void Enter();
	void Leave();
	
private:
#ifndef LOCK_USES_MUTEX
	CRITICAL_SECTION * m_pCrit;
#else
	HANDLE hLock;
#endif
};



////////////////////////////////////////////////////////////////////////////////
/// Creates a new named lock.
/// Note: Don't use the same name for several locks
////////////////////////////////////////////////////////////////////////////////
inline Lock::Lock( TCHAR * szName )
{
#ifndef LOCK_USES_MUTEX
	m_pCrit = new CRITICAL_SECTION;
	InitializeCriticalSection( m_pCrit );
#else
	hLock = CreateMutex( NULL, TRUE, szName );
	ReleaseMutex( hLock );
#endif
}



////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
inline Lock::~Lock()
{
#ifndef LOCK_USES_MUTEX
	DeleteCriticalSection( m_pCrit );
	delete [] m_pCrit;
#else
	CloseHandle( hLock );
#endif
}



////////////////////////////////////////////////////////////////////////////////
/// Closes lock.
////////////////////////////////////////////////////////////////////////////////
inline void Lock::Enter()
{
#ifndef LOCK_USES_MUTEX
	EnterCriticalSection( m_pCrit );
#else
	WaitForSingleObject( hLock, INFINITE );
#endif
}



////////////////////////////////////////////////////////////////////////////////
/// Opens lock.
////////////////////////////////////////////////////////////////////////////////
inline void Lock::Leave()
{
#ifndef LOCK_USES_MUTEX
	LeaveCriticalSection( m_pCrit );
#else
	ReleaseMutex( hLock );
#endif
}



#endif // PA_LOCK_H
