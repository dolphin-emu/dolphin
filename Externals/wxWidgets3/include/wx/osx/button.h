/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/button.h
// Purpose:     wxButton class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: button.h 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_BUTTON_H_
#define _WX_OSX_BUTTON_H_

#include "wx/control.h"
#include "wx/gdicmn.h"

// Pushbutton
class WXDLLIMPEXP_CORE wxButton : public wxButtonBase
{
public:
    wxButton() {}
    wxButton(wxWindow *parent,
             wxWindowID id,
             const wxString& label = wxEmptyString,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = 0,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxButtonNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxButtonNameStr);

    static wxSize GetDefaultSize();

    virtual void SetLabel(const wxString& label);
    virtual wxWindow *SetDefault();
    virtual void Command(wxCommandEvent& event);

    // osx specific event handling common for all osx-ports

    virtual bool        OSXHandleClicked( double timestampsec );

protected:
    virtual wxSize DoGetBestSize() const ;

    void OnEnterWindow( wxMouseEvent& event);
    void OnLeaveWindow( wxMouseEvent& event);

    virtual wxBitmap DoGetBitmap(State which) const;
    virtual void DoSetBitmap(const wxBitmap& bitmap, State which);
    virtual void DoSetBitmapPosition(wxDirection dir);

    virtual void DoSetBitmapMargins(int x, int y)
    {
        m_marginX = x;
        m_marginY = y;
        InvalidateBestSize();
    }

#if wxUSE_MARKUP && wxOSX_USE_COCOA
    virtual bool DoSetLabelMarkup(const wxString& markup);
#endif // wxUSE_MARKUP && wxOSX_USE_COCOA


    // the margins around the bitmap
    int m_marginX;
    int m_marginY;

    // the bitmaps for the different state of the buttons, all of them may be
    // invalid and the button only shows a bitmap at all if State_Normal bitmap
    // is valid
    wxBitmap m_bitmaps[State_Max];

    DECLARE_DYNAMIC_CLASS(wxButton)
    DECLARE_EVENT_TABLE()
};

// OS X specific class, not part of public wx API
class WXDLLIMPEXP_CORE wxDisclosureTriangle : public wxControl
{
public:
    wxDisclosureTriangle(wxWindow *parent,
             wxWindowID id,
             const wxString& label = wxEmptyString,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = wxBORDER_NONE,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxButtonNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxBORDER_NONE,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxButtonNameStr);

    void SetOpen( bool open );
    bool IsOpen() const;

    // osx specific event handling common for all osx-ports

    virtual bool        OSXHandleClicked( double timestampsec );

protected:
    virtual wxSize DoGetBestSize() const ;
};

#endif // _WX_OSX_BUTTON_H_
