/////////////////////////////////////////////////////////////////////////////
// Name:        include/wx/thrimpl.cpp
// Purpose:     common part of wxThread Implementations
// Author:      Vadim Zeitlin
// Modified by:
// Created:     04.06.02 (extracted from src/*/thread.cpp files)
// RCS-ID:      $Id: thrimpl.cpp 42206 2006-10-21 16:06:11Z VZ $
// Copyright:   (c) Vadim Zeitlin (2002)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// this file is supposed to be included only by the various thread.cpp

// ----------------------------------------------------------------------------
// wxMutex
// ----------------------------------------------------------------------------

wxMutex::wxMutex(wxMutexType mutexType)
{
    m_internal = new wxMutexInternal(mutexType);

    if ( !m_internal->IsOk() )
    {
        delete m_internal;
        m_internal = NULL;
    }
}

wxMutex::~wxMutex()
{
    delete m_internal;
}

bool wxMutex::IsOk() const
{
    return m_internal != NULL;
}

wxMutexError wxMutex::Lock()
{
    wxCHECK_MSG( m_internal, wxMUTEX_INVALID,
                 _T("wxMutex::Lock(): not initialized") );

    return m_internal->Lock();
}

wxMutexError wxMutex::TryLock()
{
    wxCHECK_MSG( m_internal, wxMUTEX_INVALID,
                 _T("wxMutex::TryLock(): not initialized") );

    return m_internal->TryLock();
}

wxMutexError wxMutex::Unlock()
{
    wxCHECK_MSG( m_internal, wxMUTEX_INVALID,
                 _T("wxMutex::Unlock(): not initialized") );

    return m_internal->Unlock();
}

// --------------------------------------------------------------------------
// wxConditionInternal
// --------------------------------------------------------------------------

// Win32 and OS/2 don't have explicit support for the POSIX condition
// variables and their events/event semaphores have quite different semantics,
// so we reimplement the conditions from scratch using the mutexes and
// semaphores
#if defined(__WXMSW__) || defined(__OS2__) || defined(__EMX__)

class wxConditionInternal
{
public:
    wxConditionInternal(wxMutex& mutex);

    bool IsOk() const { return m_mutex.IsOk() && m_semaphore.IsOk(); }

    wxCondError Wait();
    wxCondError WaitTimeout(unsigned long milliseconds);

    wxCondError Signal();
    wxCondError Broadcast();

private:
    // the number of threads currently waiting for this condition
    LONG m_numWaiters;

    // the critical section protecting m_numWaiters
    wxCriticalSection m_csWaiters;

    wxMutex& m_mutex;
    wxSemaphore m_semaphore;

    DECLARE_NO_COPY_CLASS(wxConditionInternal)
};

wxConditionInternal::wxConditionInternal(wxMutex& mutex)
                   : m_mutex(mutex)
{
    // another thread can't access it until we return from ctor, so no need to
    // protect access to m_numWaiters here
    m_numWaiters = 0;
}

wxCondError wxConditionInternal::Wait()
{
    // increment the number of waiters
    {
        wxCriticalSectionLocker lock(m_csWaiters);
        m_numWaiters++;
    }

    m_mutex.Unlock();

    // a potential race condition can occur here
    //
    // after a thread increments m_numWaiters, and unlocks the mutex and before
    // the semaphore.Wait() is called, if another thread can cause a signal to
    // be generated
    //
    // this race condition is handled by using a semaphore and incrementing the
    // semaphore only if m_numWaiters is greater that zero since the semaphore,
    // can 'remember' signals the race condition will not occur

    // wait ( if necessary ) and decrement semaphore
    wxSemaError err = m_semaphore.Wait();
    m_mutex.Lock();

    if ( err == wxSEMA_NO_ERROR )
        return wxCOND_NO_ERROR;
    else if ( err == wxSEMA_TIMEOUT )
        return wxCOND_TIMEOUT;
    else
        return wxCOND_MISC_ERROR;
}

wxCondError wxConditionInternal::WaitTimeout(unsigned long milliseconds)
{
    {
        wxCriticalSectionLocker lock(m_csWaiters);
        m_numWaiters++;
    }

    m_mutex.Unlock();

    // a race condition can occur at this point in the code
    //
    // please see the comments in Wait(), for details

    wxSemaError err = m_semaphore.WaitTimeout(milliseconds);

    if ( err == wxSEMA_TIMEOUT )
    {
        // another potential race condition exists here it is caused when a
        // 'waiting' thread times out, and returns from WaitForSingleObject,
        // but has not yet decremented m_numWaiters
        //
        // at this point if another thread calls signal() then the semaphore
        // will be incremented, but the waiting thread will miss it.
        //
        // to handle this particular case, the waiting thread calls
        // WaitForSingleObject again with a timeout of 0, after locking
        // m_csWaiters. This call does not block because of the zero
        // timeout, but will allow the waiting thread to catch the missed
        // signals.
        wxCriticalSectionLocker lock(m_csWaiters);

        wxSemaError err2 = m_semaphore.WaitTimeout(0);

        if ( err2 != wxSEMA_NO_ERROR )
        {
            m_numWaiters--;
        }
    }

    m_mutex.Lock();

    return err == wxSEMA_NO_ERROR ? wxCOND_NO_ERROR
                                  : err == wxSEMA_TIMEOUT ? wxCOND_TIMEOUT
                                                          : wxCOND_MISC_ERROR;
}

wxCondError wxConditionInternal::Signal()
{
    wxCriticalSectionLocker lock(m_csWaiters);

    if ( m_numWaiters > 0 )
    {
        // increment the semaphore by 1
        if ( m_semaphore.Post() != wxSEMA_NO_ERROR )
            return wxCOND_MISC_ERROR;

        m_numWaiters--;
    }

    return wxCOND_NO_ERROR;
}

wxCondError wxConditionInternal::Broadcast()
{
    wxCriticalSectionLocker lock(m_csWaiters);

    while ( m_numWaiters > 0 )
    {
        if ( m_semaphore.Post() != wxSEMA_NO_ERROR )
            return wxCOND_MISC_ERROR;

        m_numWaiters--;
    }

    return wxCOND_NO_ERROR;
}

#endif // MSW or OS2

// ----------------------------------------------------------------------------
// wxCondition
// ----------------------------------------------------------------------------

wxCondition::wxCondition(wxMutex& mutex)
{
    m_internal = new wxConditionInternal(mutex);

    if ( !m_internal->IsOk() )
    {
        delete m_internal;
        m_internal = NULL;
    }
}

wxCondition::~wxCondition()
{
    delete m_internal;
}

bool wxCondition::IsOk() const
{
    return m_internal != NULL;
}

wxCondError wxCondition::Wait()
{
    wxCHECK_MSG( m_internal, wxCOND_INVALID,
                 _T("wxCondition::Wait(): not initialized") );

    return m_internal->Wait();
}

wxCondError wxCondition::WaitTimeout(unsigned long milliseconds)
{
    wxCHECK_MSG( m_internal, wxCOND_INVALID,
                 _T("wxCondition::Wait(): not initialized") );

    return m_internal->WaitTimeout(milliseconds);
}

wxCondError wxCondition::Signal()
{
    wxCHECK_MSG( m_internal, wxCOND_INVALID,
                 _T("wxCondition::Signal(): not initialized") );

    return m_internal->Signal();
}

wxCondError wxCondition::Broadcast()
{
    wxCHECK_MSG( m_internal, wxCOND_INVALID,
                 _T("wxCondition::Broadcast(): not initialized") );

    return m_internal->Broadcast();
}

// --------------------------------------------------------------------------
// wxSemaphore
// --------------------------------------------------------------------------

wxSemaphore::wxSemaphore(int initialcount, int maxcount)
{
    m_internal = new wxSemaphoreInternal( initialcount, maxcount );
    if ( !m_internal->IsOk() )
    {
        delete m_internal;
        m_internal = NULL;
    }
}

wxSemaphore::~wxSemaphore()
{
    delete m_internal;
}

bool wxSemaphore::IsOk() const
{
    return m_internal != NULL;
}

wxSemaError wxSemaphore::Wait()
{
    wxCHECK_MSG( m_internal, wxSEMA_INVALID,
                 _T("wxSemaphore::Wait(): not initialized") );

    return m_internal->Wait();
}

wxSemaError wxSemaphore::TryWait()
{
    wxCHECK_MSG( m_internal, wxSEMA_INVALID,
                 _T("wxSemaphore::TryWait(): not initialized") );

    return m_internal->TryWait();
}

wxSemaError wxSemaphore::WaitTimeout(unsigned long milliseconds)
{
    wxCHECK_MSG( m_internal, wxSEMA_INVALID,
                 _T("wxSemaphore::WaitTimeout(): not initialized") );

    return m_internal->WaitTimeout(milliseconds);
}

wxSemaError wxSemaphore::Post()
{
    wxCHECK_MSG( m_internal, wxSEMA_INVALID,
                 _T("wxSemaphore::Post(): not initialized") );

    return m_internal->Post();
}

