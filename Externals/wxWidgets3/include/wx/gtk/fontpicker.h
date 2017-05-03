/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/fontpicker.h
// Purpose:     wxFontButton header
// Author:      Francesco Montorsi
// Modified by:
// Created:     14/4/2006
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_FONTPICKER_H_
#define _WX_GTK_FONTPICKER_H_

#include "wx/button.h"

//-----------------------------------------------------------------------------
// wxFontButton
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFontButton : public wxButton,
                                      public wxFontPickerWidgetBase
{
public:
    wxFontButton() { Init(); }
    wxFontButton(wxWindow *parent,
                 wxWindowID id,
                 const wxFont& initial = wxNullFont,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxFONTBTN_DEFAULT_STYLE,
                 const wxValidator& validator = wxDefaultValidator,
                 const wxString& name = wxFontPickerWidgetNameStr)
    {
        Init();

        Create(parent, id, initial, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxFont& initial = wxNullFont,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxFONTBTN_DEFAULT_STYLE,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxFontPickerWidgetNameStr);

    virtual wxColour GetSelectedColour() const wxOVERRIDE
        { return m_selectedColour; }

    void SetSelectedColour(const wxColour &colour) wxOVERRIDE
        { m_selectedColour = colour; }

    virtual ~wxFontButton();

protected:
    void UpdateFont() wxOVERRIDE;


public:     // used by the GTK callback only
    void SetNativeFontInfo(const char* gtkdescription);

private:
    // Common part of both ctors.
    void Init()
    {
        m_selectedColour = *wxBLACK;
    }

    // This can't be changed by the user, but is provided to
    // satisfy the wxFontPickerWidgetBase interface.
    wxColour m_selectedColour;

    wxDECLARE_DYNAMIC_CLASS(wxFontButton);
};

#endif // _WX_GTK_FONTPICKER_H_

