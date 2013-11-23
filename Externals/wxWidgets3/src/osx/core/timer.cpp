/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/timer.cpp
// Purpose:     wxTimer implementation using CoreFoundation
// Author:      Stefan Csomor
// Modified by:
// Created:     2008-07-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_TIMER

#ifndef WX_PRECOMP
    #include "wx/dynarray.h"
#endif

#include "wx/osx/private.h"
#include "wx/osx/private/timer.h"

struct wxOSXTimerInfo
{
    wxOSXTimerImpl*     m_timer;
    CFRunLoopTimerRef   m_timerRef;
};

void wxProcessTimer(CFRunLoopTimerRef WXUNUSED(theTimer), void *data)
{
    if ( data == NULL )
        return;

    wxOSXTimerImpl* timer = (wxOSXTimerImpl*)data;

    if ( timer->IsOneShot() )
        timer->Stop();

    timer->Notify();
}

wxOSXTimerImpl::wxOSXTimerImpl(wxTimer *timer)
                 : wxTimerImpl(timer)
{
    m_info = new wxOSXTimerInfo();
    m_info->m_timer = this;
    m_info->m_timerRef = kInvalidID;
}

bool wxOSXTimerImpl::IsRunning() const
{
    return ( m_info->m_timerRef != kInvalidID && CFRunLoopTimerIsValid(m_info->m_timerRef));
}

wxOSXTimerImpl::~wxOSXTimerImpl()
{
    if (m_info->m_timerRef)
    {
        if ( CFRunLoopTimerIsValid(m_info->m_timerRef) )
            CFRunLoopTimerInvalidate( m_info->m_timerRef );
        CFRelease( m_info->m_timerRef );
    }
    delete m_info;
}

bool wxOSXTimerImpl::Start( int milliseconds, bool mode )
{
    (void)wxTimerImpl::Start(milliseconds, mode);

    wxCHECK_MSG( m_milli > 0, false, wxT("invalid value for timer timeout") );
    wxCHECK_MSG( m_info->m_timerRef == NULL, false, wxT("attempting to restart a timer") );

    CFGregorianUnits gumilli ;
    memset(&gumilli,0,sizeof(gumilli) );
    gumilli.seconds = m_milli / 1000.0;

    CFRunLoopTimerContext ctx ;
    memset( &ctx, 0 , sizeof(ctx) );
    ctx.version = 0;
    ctx.info = this;

    m_info->m_timer = this;
    m_info->m_timerRef = CFRunLoopTimerCreate(
        kCFAllocatorDefault,
        CFAbsoluteTimeAddGregorianUnits( CFAbsoluteTimeGetCurrent() , NULL, gumilli ),
        IsOneShot() ? 0 : CFTimeInterval( m_milli / 1000.0 ) ,
        0, 0, wxProcessTimer, &ctx);

    wxASSERT_MSG( m_info->m_timerRef != NULL, wxT("unable to create timer"));

    CFRunLoopRef runLoop = 0;
#if wxOSX_USE_IPHONE
    runLoop = CFRunLoopGetMain();
#else
    runLoop = CFRunLoopGetCurrent();
#endif
    CFRunLoopAddTimer( runLoop, m_info->m_timerRef, kCFRunLoopCommonModes) ;


    return true;
}

void wxOSXTimerImpl::Stop()
{
    if (m_info->m_timerRef)
    {
        CFRunLoopTimerInvalidate( m_info->m_timerRef );
        CFRelease( m_info->m_timerRef );
    }
    m_info->m_timerRef = kInvalidID;
}

#endif // wxUSE_TIMER

