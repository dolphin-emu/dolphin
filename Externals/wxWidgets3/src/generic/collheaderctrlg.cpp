/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/collheaderctrlg.cpp
// Purpose:     Generic wxCollapsibleHeaderCtrl implementation
// Author:      Tobias Taschner
// Created:     2015-09-19
// Copyright:   (c) 2015 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include "wx/defs.h"

#if wxUSE_COLLPANE

#include "wx/collheaderctrl.h"

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
    #include "wx/sizer.h"
#endif // !WX_PRECOMP

#include "wx/renderer.h"

// if we have another implementation of this class we should extract
// the lines below to a common file

const char wxCollapsibleHeaderCtrlNameStr[] = "collapsibleHeader";

wxDEFINE_EVENT(wxEVT_COLLAPSIBLEHEADER_CHANGED, wxCommandEvent);

// ============================================================================
// implementation
// ============================================================================

void wxGenericCollapsibleHeaderCtrl::Init()
{
    m_collapsed = true;
    m_inWindow = false;
    m_mouseDown = false;
}

bool wxGenericCollapsibleHeaderCtrl::Create(wxWindow *parent,
    wxWindowID id,
    const wxString& label,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxValidator& validator,
    const wxString& name)
{
    if ( !wxCollapsibleHeaderCtrlBase::Create(parent, id, label, pos, size,
                                              style, validator, name) )
    {
        return false;
    }

    Bind(wxEVT_PAINT, &wxGenericCollapsibleHeaderCtrl::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &wxGenericCollapsibleHeaderCtrl::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP, &wxGenericCollapsibleHeaderCtrl::OnLeftUp, this);
    Bind(wxEVT_ENTER_WINDOW, &wxGenericCollapsibleHeaderCtrl::OnEnterWindow, this);
    Bind(wxEVT_LEAVE_WINDOW, &wxGenericCollapsibleHeaderCtrl::OnLeaveWindow, this);
    Bind(wxEVT_CHAR, &wxGenericCollapsibleHeaderCtrl::OnChar, this);
    Bind(wxEVT_SET_FOCUS, &wxGenericCollapsibleHeaderCtrl::OnFocus, this);
    Bind(wxEVT_KILL_FOCUS, &wxGenericCollapsibleHeaderCtrl::OnFocus, this);

    return true;
}

wxSize wxGenericCollapsibleHeaderCtrl::DoGetBestClientSize() const
{
    wxClientDC dc(const_cast<wxGenericCollapsibleHeaderCtrl*>(this));
    wxSize btnSize = wxRendererNative::Get().GetCollapseButtonSize(const_cast<wxGenericCollapsibleHeaderCtrl*>(this), dc);
    wxString text;
    wxControl::FindAccelIndex(GetLabel(), &text);
    wxSize textSize = dc.GetTextExtent(text);
    // Add some padding if the label is not empty
    if ( textSize.x > 0 )
        textSize.x += FromDIP(4);

    return wxSize(btnSize.x + textSize.x,
        wxMax(textSize.y, btnSize.y));
}

void wxGenericCollapsibleHeaderCtrl::SetCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    Refresh();
}

void wxGenericCollapsibleHeaderCtrl::DoSetCollapsed(bool collapsed)
{
    SetCollapsed(collapsed);

    wxCommandEvent evt(wxEVT_COLLAPSIBLEHEADER_CHANGED, GetId());
    evt.SetEventObject(this);
    ProcessEvent(evt);
}

void wxGenericCollapsibleHeaderCtrl::OnFocus(wxFocusEvent& event)
{
    Refresh();
    event.Skip();
}

void wxGenericCollapsibleHeaderCtrl::OnChar(wxKeyEvent& event)
{
    switch (event.GetKeyCode())
    {
    case WXK_SPACE:
    case WXK_RETURN:
        DoSetCollapsed(!m_collapsed);
        break;
    default:
        event.Skip();
        break;
    }
}

void wxGenericCollapsibleHeaderCtrl::OnEnterWindow(wxMouseEvent& event)
{
    m_inWindow = true;
    Refresh();
    event.Skip();
}

void wxGenericCollapsibleHeaderCtrl::OnLeaveWindow(wxMouseEvent& event)
{
    m_inWindow = false;
    Refresh();
    event.Skip();
}

void wxGenericCollapsibleHeaderCtrl::OnLeftUp(wxMouseEvent& event)
{
    m_mouseDown = false;
    DoSetCollapsed(!m_collapsed);
    event.Skip();
}

void wxGenericCollapsibleHeaderCtrl::OnLeftDown(wxMouseEvent& event)
{
    m_mouseDown = true;
    Refresh();
    event.Skip();
}

void wxGenericCollapsibleHeaderCtrl::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);

    wxRect rect(wxPoint(0, 0), GetClientSize());

    wxSize btnSize = wxRendererNative::Get().GetCollapseButtonSize(this, dc);

    wxRect btnRect(wxPoint(0, 0), btnSize);
    btnRect = btnRect.CenterIn(rect, wxVERTICAL);

    int flags = 0;

    if ( m_inWindow )
        flags |= wxCONTROL_CURRENT;

    if ( m_mouseDown )
        flags |= wxCONTROL_PRESSED;

    if ( !m_collapsed )
        flags |= wxCONTROL_EXPANDED;

    wxRendererNative::Get().DrawCollapseButton(this, dc, btnRect, flags);

    wxString text;
    int indexAccel = wxControl::FindAccelIndex(GetLabel(), &text);

    wxSize textSize = dc.GetTextExtent(text);

    wxRect textRect(wxPoint(btnSize.x + FromDIP(2), 0), textSize);
    textRect = textRect.CenterIn(rect, wxVERTICAL);

    dc.DrawLabel(text, textRect, wxALIGN_CENTRE_VERTICAL, indexAccel);

#ifdef __WXMSW__
    if ( HasFocus() )
        wxRendererNative::Get().DrawFocusRect(this, dc, textRect.Inflate(1), flags);
#endif
}


#endif // wxUSE_COLLPANE
