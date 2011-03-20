/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/timer.cpp
// Purpose:     wxTimer implementation
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: timer.cpp 64943 2010-07-13 13:29:58Z VZ $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_TIMER

#ifndef WX_PRECOMP
    #include "wx/dynarray.h"
#endif

#include "wx/osx/private/timer.h"

#ifdef __WXMAC__
    #include "wx/osx/private.h"
#endif

struct MacTimerInfo
{
    wxCarbonTimerImpl* m_timer;
    EventLoopTimerUPP m_proc;
    EventLoopTimerRef   m_timerRef;
};

static pascal void wxProcessTimer( EventLoopTimerRef WXUNUSED(theTimer), void *data )
{
    if ( data == NULL )
        return;

    wxCarbonTimerImpl* timer = (wxCarbonTimerImpl*)data;

    if ( timer->IsOneShot() )
        timer->Stop();

    timer->Notify();
}

wxCarbonTimerImpl::wxCarbonTimerImpl(wxTimer *timer)
                 : wxTimerImpl(timer)
{
    m_info = new MacTimerInfo();
    m_info->m_timer = this;
    m_info->m_proc = NULL;
    m_info->m_timerRef = kInvalidID;
}

bool wxCarbonTimerImpl::IsRunning() const
{
    return ( m_info->m_timerRef != kInvalidID );
}

wxCarbonTimerImpl::~wxCarbonTimerImpl()
{
    delete m_info;
}

bool wxCarbonTimerImpl::Start( int milliseconds, bool mode )
{
    (void)wxTimerImpl::Start(milliseconds, mode);

    wxCHECK_MSG( m_milli > 0, false, wxT("invalid value for timer timeout") );
    wxCHECK_MSG( m_info->m_timerRef == NULL, false, wxT("attempting to restart a timer") );

    m_info->m_timer = this;
    m_info->m_proc = NewEventLoopTimerUPP( &wxProcessTimer );

    OSStatus err = InstallEventLoopTimer(
        GetMainEventLoop(),
        m_milli*kEventDurationMillisecond,
        IsOneShot() ? 0 : m_milli * kEventDurationMillisecond,
        m_info->m_proc,
        this,
        &m_info->m_timerRef );
    verify_noerr( err );

    return true;
}

void wxCarbonTimerImpl::Stop()
{
    if (m_info->m_timerRef)
        RemoveEventLoopTimer( m_info->m_timerRef );
    if (m_info->m_proc)
        DisposeEventLoopTimerUPP( m_info->m_proc );

    m_info->m_proc = NULL;
    m_info->m_timerRef = kInvalidID;
}

#endif // wxUSE_TIMER

