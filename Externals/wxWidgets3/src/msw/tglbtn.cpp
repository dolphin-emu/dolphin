/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/tglbtn.cpp
// Purpose:     Definition of the wxToggleButton class, which implements a
//              toggle button under wxMSW.
// Author:      John Norris, minor changes by Axel Schlueter
//              and William Gallafent.
// Modified by:
// Created:     08.02.01
// RCS-ID:      $Id: tglbtn.cpp 64940 2010-07-13 13:29:13Z VZ $
// Copyright:   (c) 2000 Johnny C. Norris II
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TOGGLEBTN

#include "wx/tglbtn.h"

#ifndef WX_PRECOMP
    #include "wx/button.h"
    #include "wx/brush.h"
    #include "wx/dcscreen.h"
    #include "wx/settings.h"

    #include "wx/log.h"
#endif // WX_PRECOMP

#include "wx/renderer.h"
#include "wx/dcclient.h"

#include "wx/msw/private.h"
#include "wx/msw/private/button.h"

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

wxDEFINE_EVENT( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEvent );

// ============================================================================
// implementation
// ============================================================================

//-----------------------------------------------------------------------------
// wxBitmapToggleButton
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxBitmapToggleButton, wxControl)

BEGIN_EVENT_TABLE(wxBitmapToggleButton,wxToggleButtonBase)
   EVT_PAINT(wxBitmapToggleButton::OnPaint)
   EVT_MOUSE_EVENTS(wxBitmapToggleButton::OnMouse)
   EVT_CHAR(wxBitmapToggleButton::OnChar)
   EVT_SIZE(wxBitmapToggleButton::OnSize)
END_EVENT_TABLE()

void wxBitmapToggleButton::Init()
{
    m_depressed = false;
    m_oldValue = false;
    m_capturing = false;
}

bool wxBitmapToggleButton::Create( wxWindow *parent, wxWindowID id,
                const wxBitmap& label,const wxPoint& pos, const wxSize& size, long style,
                const wxValidator& validator, const wxString& name )
{
    Init();

    if (!wxToggleButtonBase::Create( parent, id, pos, size, style, validator, name ))
        return false;

    m_bitmap = label;

    if (size.x == -1 || size.y == -1)
    {
        wxSize new_size = GetBestSize();
        if (size.x != -1)
            new_size.x = size.x;
        if (size.y != -1)
            new_size.y = size.y;
        SetSize( new_size );
    }

    return true;
}

void wxBitmapToggleButton::SetValue(bool state)
{
    if (m_capturing) return;

    if (state == m_depressed) return;

    m_depressed = state;
    Refresh();
}

bool wxBitmapToggleButton::GetValue() const
{
    return m_depressed;
}

void wxBitmapToggleButton::SetLabel(const wxBitmap& label)
{
    m_bitmap = label;
    m_disabledBitmap = wxBitmap();

    Refresh();
}

bool wxBitmapToggleButton::Enable(bool enable)
{
    if (m_capturing) return false;

    if (!wxToggleButtonBase::Enable( enable ))
      return false;

    Refresh();

    return true;
}

void wxBitmapToggleButton::OnPaint(wxPaintEvent &WXUNUSED(event))
{
    wxSize size = GetSize();

    wxBitmap bitmap = m_bitmap;

    wxPaintDC dc(this);
    wxRendererNative &renderer = wxRendererNative::Get();
    int flags = 0;
    if (m_depressed)
        flags |= wxCONTROL_PRESSED;
    wxRect rect(0,0,size.x,size.y);
    renderer.DrawPushButton( this, dc, rect, flags );

    if (bitmap.IsOk())
    {
        if (!IsEnabled())
        {
            if (!m_disabledBitmap.IsOk())
            {
                wxImage image = m_bitmap.ConvertToImage();
                m_disabledBitmap = wxBitmap( image.ConvertToGreyscale() );
            }

            bitmap = m_disabledBitmap;
        }

        wxSize bsize = bitmap.GetSize();
        int offset = 0;
        if (m_depressed) offset = 1;
        dc.DrawBitmap( bitmap, (size.x-bsize.x) / 2 + offset, (size.y-bsize.y) / 2 + offset, true );
    }

}

void wxBitmapToggleButton::OnMouse(wxMouseEvent &event)
{
    if (!IsEnabled())
        return;

    wxSize size = GetSize();
    bool mouse_in = ((event.GetX() > 0) && (event.GetX() < size.x) &&
                     (event.GetY() > 0) && (event.GetY() < size.y));

    if (m_capturing)
    {
        bool old_depressed = m_depressed;
        if (mouse_in)
            m_depressed = !m_oldValue;
        else
            m_depressed = m_oldValue;

        if (event.LeftUp())
        {
            ReleaseMouse();
            m_capturing = false;
            if (mouse_in)
            {
                wxCommandEvent event(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, m_windowId);
                event.SetInt(GetValue());
                event.SetEventObject(this);
                ProcessCommand(event);
            }
        }

        if (old_depressed != m_depressed)
           Refresh();
    }
    else
    {
        if (event.LeftDown())
        {
            m_capturing = true;
            m_oldValue = m_depressed;
            m_depressed = !m_oldValue;
            CaptureMouse();
            Refresh();
        }
    }
}

void wxBitmapToggleButton::OnChar(wxKeyEvent &event)
{
   if (event.GetKeyCode() == WXK_SPACE)
   {
       m_depressed = !m_depressed;
       Refresh();

       wxCommandEvent event(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, m_windowId);
       event.SetInt(GetValue());
       event.SetEventObject(this);
       ProcessCommand(event);
   }
}

void wxBitmapToggleButton::OnSize(wxSizeEvent &WXUNUSED(event))
{
    Refresh();
}

wxSize wxBitmapToggleButton::DoGetBestSize() const
{
    if (!m_bitmap.IsOk())
        return wxSize(16,16);

    wxSize ret = m_bitmap.GetSize();
    ret.x += 8;
    ret.y += 8;
    return ret;
}


// ----------------------------------------------------------------------------
// wxToggleButton
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxToggleButton, wxControl)

// Single check box item
bool wxToggleButton::Create(wxWindow *parent,
                            wxWindowID id,
                            const wxString& label,
                            const wxPoint& pos,
                            const wxSize& size, long style,
                            const wxValidator& validator,
                            const wxString& name)
{
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    // if the label contains several lines we must explicitly tell the button
    // about it or it wouldn't draw it correctly ("\n"s would just appear as
    // black boxes)
    //
    // NB: we do it here and not in MSWGetStyle() because we need the label
    //     value and the label is not set yet when MSWGetStyle() is called
    WXDWORD exstyle;
    WXDWORD msStyle = MSWGetStyle(style, &exstyle);
    msStyle |= wxMSWButton::GetMultilineStyle(label);

    return MSWCreateControl(wxT("BUTTON"), msStyle, pos, size, label, exstyle);
}

WXDWORD wxToggleButton::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    WXDWORD msStyle = wxControl::MSWGetStyle(style, exstyle);

    msStyle |= BS_AUTOCHECKBOX | BS_PUSHLIKE | WS_TABSTOP;

    if ( style & wxBU_LEFT )
      msStyle |= BS_LEFT;
    if ( style & wxBU_RIGHT )
      msStyle |= BS_RIGHT;
    if ( style & wxBU_TOP )
      msStyle |= BS_TOP;
    if ( style & wxBU_BOTTOM )
      msStyle |= BS_BOTTOM;

    return msStyle;
}

wxSize wxToggleButton::DoGetBestSize() const
{
    return wxMSWButton::ComputeBestSize(const_cast<wxToggleButton *>(this));
}

void wxToggleButton::SetLabel(const wxString& label)
{
    wxMSWButton::UpdateMultilineStyle(GetHwnd(), label);

    wxToggleButtonBase::SetLabel(label);
}

void wxToggleButton::SetValue(bool val)
{
   ::SendMessage(GetHwnd(), BM_SETCHECK, val, 0);
}

bool wxToggleButton::GetValue() const
{
    return ::SendMessage(GetHwnd(), BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void wxToggleButton::Command(wxCommandEvent& event)
{
    SetValue(event.GetInt() != 0);
    ProcessCommand(event);
}

bool wxToggleButton::MSWCommand(WXUINT WXUNUSED(param), WXWORD WXUNUSED(id))
{
    wxCommandEvent event(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, m_windowId);
    event.SetInt(GetValue());
    event.SetEventObject(this);
    ProcessCommand(event);
    return true;
}

#endif // wxUSE_TOGGLEBTN
