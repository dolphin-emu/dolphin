/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/tooltip.cpp
// Purpose:     wxToolTip implementation
// Author:      Stefan Csomor
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_TOOLTIPS

#include "wx/tooltip.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/window.h"
    #include "wx/dc.h"
    #include "wx/timer.h"
    #include "wx/nonownedwnd.h"
#endif // WX_PRECOMP

#include "wx/geometry.h"
#include "wx/osx/uma.h"

//-----------------------------------------------------------------------------
// global data
//-----------------------------------------------------------------------------

#if wxUSE_TIMER
class wxMacToolTipTimer : public wxTimer
{
public:
    wxMacToolTipTimer(wxMacToolTip* tip, int iMilliseconds) ;
    wxMacToolTipTimer() {} ;
    virtual ~wxMacToolTipTimer() {} ;

    void Notify()
    {
        if ( m_mark == m_tip->GetMark() )
            m_tip->Draw() ;
    }

protected:
    wxMacToolTip*     m_tip;
    long        m_mark ;
};
#endif // wxUSE_TIMER

//-----------------------------------------------------------------------------
// wxToolTip
//-----------------------------------------------------------------------------
static long s_ToolTipDelay = 500 ;
static bool s_ShowToolTips = true ;
static wxMacToolTip s_ToolTip ;
static wxWindow* s_LastWindowEntered = NULL ;
static wxRect2DInt s_ToolTipArea ;
static WindowRef s_ToolTipWindowRef = NULL ;

IMPLEMENT_ABSTRACT_CLASS(wxToolTip, wxObject)


wxToolTip::wxToolTip( const wxString &tip )
{
    m_text = tip;
    m_window = NULL;
}

wxToolTip::~wxToolTip()
{
}

void wxToolTip::SetTip( const wxString &tip )
{
    m_text = tip;

    if ( m_window )
    {
#if 0
    // update it immediately
    wxToolInfo ti(GetHwndOf(m_window));
    ti.lpszText = (wxChar *)m_text.c_str();

    (void)SendTooltipMessage(GetToolTipCtrl(), TTM_UPDATETIPTEXT, 0, &ti);
#endif
    }
}

void wxToolTip::SetWindow( wxWindow *win )
{
    m_window = win ;
}

void wxToolTip::Enable( bool flag )
{
    if ( s_ShowToolTips != flag )
    {
        s_ShowToolTips = flag ;

        if ( s_ShowToolTips )
        {
        }
        else
        {
            s_ToolTip.Clear() ;
        }
    }
}

void wxToolTip::SetDelay( long msecs )
{
    s_ToolTipDelay = msecs ;
}

void wxToolTip::SetAutoPop( long WXUNUSED(msecs) )
{
}

void wxToolTip::SetReshow( long WXUNUSED(msecs) )
{
}

void wxToolTip::RelayEvent( wxWindow *win , wxMouseEvent &event )
{
    if ( s_ShowToolTips )
    {
        if ( event.GetEventType() == wxEVT_LEAVE_WINDOW )
        {
            s_ToolTip.Clear() ;
        }
        else if (event.GetEventType() == wxEVT_ENTER_WINDOW || event.GetEventType() == wxEVT_MOTION )
        {
            wxPoint2DInt where( event.m_x , event.m_y ) ;
            if ( s_LastWindowEntered == win && s_ToolTipArea.Contains( where ) )
            {
            }
            else
            {
                s_ToolTip.Clear() ;
                s_ToolTipArea = wxRect2DInt( event.m_x - 2 , event.m_y - 2 , 4 , 4 ) ;
                s_LastWindowEntered = win ;

                WindowRef window = MAC_WXHWND( win->MacGetTopLevelWindowRef() ) ;
                int x = event.m_x ;
                int y = event.m_y ;
                wxPoint local( x , y ) ;
                win->MacClientToRootWindow( &x, &y ) ;
                wxPoint windowlocal( x , y ) ;
                s_ToolTip.Setup( window , win->MacGetToolTipString( local ) , windowlocal ) ;
            }
        }
    }
}

void wxToolTip::RemoveToolTips()
{
    s_ToolTip.Clear() ;
}

// --- mac specific
#if wxUSE_TIMER
wxMacToolTipTimer::wxMacToolTipTimer( wxMacToolTip *tip , int msec )
{
    m_tip = tip;
    m_mark = tip->GetMark() ;
    Start(msec, true);
}
#endif // wxUSE_TIMER

wxMacToolTip::wxMacToolTip()
{
    m_window = NULL ;
    m_backpict = NULL ;
#if wxUSE_TIMER
    m_timer = NULL ;
#endif
    m_mark = 0 ;
    m_shown = false ;
}

void wxMacToolTip::Setup( WindowRef win  , const wxString& text , const wxPoint& localPosition )
{
    m_mark++ ;

    Clear() ;
    m_position = localPosition ;
    m_label = text ;
    m_window =win;
    s_ToolTipWindowRef = m_window ;
    m_backpict = NULL ;
#if wxUSE_TIMER
    delete m_timer ;

    m_timer = new wxMacToolTipTimer( this , s_ToolTipDelay ) ;
#endif // wxUSE_TIMER
}

wxMacToolTip::~wxMacToolTip()
{
#if wxUSE_TIMER
    wxDELETE(m_timer);
#endif // wxUSE_TIMER
    if ( m_backpict )
        Clear() ;
}

const short kTipBorder = 2 ;
const short kTipOffset = 5 ;

void wxMacToolTip::Draw()
{
    if ( m_label.empty() )
        return ;

    if ( m_window == s_ToolTipWindowRef )
    {
        m_shown = true ;

        HMHelpContentRec tag ;
        tag.version = kMacHelpVersion;

        int x = m_position.x;
        int y = m_position.y;
        wxNonOwnedWindow* tlw = wxNonOwnedWindow::GetFromWXWindow((WXWindow) m_window);
        if ( tlw )
            tlw->GetNonOwnedPeer()->WindowToScreen( &x, &y );
        SetRect( &tag.absHotRect , x - 2 , y - 2 , x + 2 , y + 2 );

        m_helpTextRef = wxCFStringRef( m_label , wxFONTENCODING_DEFAULT ) ;
        tag.content[kHMMinimumContentIndex].contentType = kHMCFStringContent ;
        tag.content[kHMMinimumContentIndex].u.tagCFString = m_helpTextRef ;
        tag.content[kHMMaximumContentIndex].contentType = kHMCFStringContent ;
        tag.content[kHMMaximumContentIndex].u.tagCFString = m_helpTextRef ;
        tag.tagSide = kHMDefaultSide;
        HMDisplayTag( &tag );
    }
}

void wxToolTip::NotifyWindowDelete( WXHWND win )
{
    if ( win == s_ToolTipWindowRef )
        s_ToolTipWindowRef = NULL ;
}

void wxMacToolTip::Clear()
{
    m_mark++ ;
#if wxUSE_TIMER
    wxDELETE(m_timer);
#endif // wxUSE_TIMER
    if ( !m_shown )
        return ;

    HMHideTag() ;
}

#endif // wxUSE_TOOLTIPS
