/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/timerunx.cpp
// Purpose:     wxTimer implementation for non-GUI applications under Unix
// Author:      Lukasz Michalski
// Created:     15/01/2005
// Copyright:   (c) 2007 Lukasz Michalski
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#if wxUSE_TIMER

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/module.h"
    #include "wx/app.h"
    #include "wx/list.h"
    #include "wx/hashmap.h"
    #include "wx/event.h"
#endif

#include "wx/apptrait.h"
#include "wx/longlong.h"
#include "wx/time.h"
#include "wx/vector.h"

#include <sys/time.h>
#include <signal.h>

#include "wx/unix/private/timer.h"

#include "wx/listimpl.cpp"
WX_DEFINE_LIST(wxTimerList)

// trace mask for the debugging messages used here
#define wxTrace_Timer wxT("timer")

// ============================================================================
// wxTimerScheduler implementation
// ============================================================================

wxTimerScheduler *wxTimerScheduler::ms_instance = NULL;

wxTimerScheduler::~wxTimerScheduler()
{
    for ( wxTimerList::iterator node = m_timers.begin();
          node != m_timers.end();
          ++node )
    {
        delete *node;
    }
}

void wxTimerScheduler::AddTimer(wxUnixTimerImpl *timer, wxUsecClock_t expiration)
{
    DoAddTimer(new wxTimerSchedule(timer, expiration));
}

void wxTimerScheduler::DoAddTimer(wxTimerSchedule *s)
{
    // do an insertion sort to keep the list sorted in expiration order
    wxTimerList::iterator node;
    for ( node = m_timers.begin(); node != m_timers.end(); ++node )
    {
        wxASSERT_MSG( (*node)->m_timer != s->m_timer,
                      wxT("adding the same timer twice?") );

        if ( (*node)->m_expiration > s->m_expiration )
            break;
    }

    m_timers.insert(node, s);

    wxLogTrace(wxTrace_Timer, wxT("Inserted timer %d expiring at %s"),
               s->m_timer->GetId(),
               s->m_expiration.ToString());
}

void wxTimerScheduler::RemoveTimer(wxUnixTimerImpl *timer)
{
    wxLogTrace(wxTrace_Timer, wxT("Removing timer %d"), timer->GetId());

    for ( wxTimerList::iterator node = m_timers.begin();
          node != m_timers.end();
          ++node )
    {
        if ( (*node)->m_timer == timer )
        {
            delete *node;
            m_timers.erase(node);
            return;
        }
    }

    wxFAIL_MSG( wxT("removing inexistent timer?") );
}

bool wxTimerScheduler::GetNext(wxUsecClock_t *remaining) const
{
    if ( m_timers.empty() )
      return false;

    wxCHECK_MSG( remaining, false, wxT("NULL pointer") );

    *remaining = (*m_timers.begin())->m_expiration - wxGetUTCTimeUSec();
    if ( *remaining < 0 )
    {
        // timer already expired, don't wait at all before notifying it
        *remaining = 0;
    }

    return true;
}

bool wxTimerScheduler::NotifyExpired()
{
    if ( m_timers.empty() )
      return false;

    const wxUsecClock_t now = wxGetUTCTimeUSec();

    typedef wxVector<wxUnixTimerImpl *> TimerImpls;
    TimerImpls toNotify;
    for ( wxTimerList::iterator next,
            cur = m_timers.begin(); cur != m_timers.end(); cur = next )
    {
        wxTimerSchedule * const s = *cur;
        if ( s->m_expiration > now )
        {
            // as the list is sorted by expiration time, we can skip the rest
            break;
        }

        // remember next as we will delete the node pointed to by cur
        next = cur;
        ++next;

        m_timers.erase(cur);

        // check whether we need to keep this timer
        wxUnixTimerImpl * const timer = s->m_timer;
        if ( timer->IsOneShot() )
        {
            // the timer needs to be stopped but don't call its Stop() from
            // here as it would attempt to remove the timer from our list and
            // we had already done it, so we just need to reset its state
            timer->MarkStopped();

            // don't need it any more
            delete s;
        }
        else // reschedule the next timer expiration
        {
            // always keep the expiration time in the future, i.e. base it on
            // the current time instead of just offsetting it from the current
            // expiration time because it could happen that we're late and the
            // current expiration time is (far) in the past
            s->m_expiration = now + timer->GetInterval()*1000;
            DoAddTimer(s);
        }

        // we can't notify the timer from this loop as the timer event handler
        // could modify m_timers (for example, but not only, by stopping this
        // timer) which would render our iterators invalid, so do it after the
        // loop end
        toNotify.push_back(timer);
    }

    if ( toNotify.empty() )
        return false;

    for ( TimerImpls::const_iterator i = toNotify.begin(),
                                     end = toNotify.end();
          i != end;
          ++i )
    {
        (*i)->Notify();
    }

    return true;
}

// ============================================================================
// wxUnixTimerImpl implementation
// ============================================================================

wxUnixTimerImpl::wxUnixTimerImpl(wxTimer* timer)
               : wxTimerImpl(timer)
{
    m_isRunning = false;
}

bool wxUnixTimerImpl::Start(int milliseconds, bool oneShot)
{
    // notice that this will stop an already running timer
    wxTimerImpl::Start(milliseconds, oneShot);

    wxTimerScheduler::Get().AddTimer(this, wxGetUTCTimeUSec() + m_milli*1000);
    m_isRunning = true;

    return true;
}

void wxUnixTimerImpl::Stop()
{
    if ( m_isRunning )
    {
        wxTimerScheduler::Get().RemoveTimer(this);

        m_isRunning = false;
    }
}

bool wxUnixTimerImpl::IsRunning() const
{
    return m_isRunning;
}

wxUnixTimerImpl::~wxUnixTimerImpl()
{
    wxASSERT_MSG( !m_isRunning, wxT("must have been stopped before") );
}

// ============================================================================
// wxTimerUnixModule: responsible for freeing the global timer scheduler
// ============================================================================

class wxTimerUnixModule : public wxModule
{
public:
    wxTimerUnixModule() {}
    virtual bool OnInit() wxOVERRIDE { return true; }
    virtual void OnExit() wxOVERRIDE { wxTimerScheduler::Shutdown(); }

    wxDECLARE_DYNAMIC_CLASS(wxTimerUnixModule);
};

wxIMPLEMENT_DYNAMIC_CLASS(wxTimerUnixModule, wxModule);

// ============================================================================
// global functions
// ============================================================================

wxTimerImpl *wxConsoleAppTraits::CreateTimerImpl(wxTimer *timer)
{
    return new wxUnixTimerImpl(timer);
}

#endif // wxUSE_TIMER

