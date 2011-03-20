/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/tglbtn.h
// Purpose:     Declaration of the wxToggleButton class, which implements a
//              toggle button under wxMSW.
// Author:      John Norris, minor changes by Axel Schlueter
// Modified by:
// Created:     08.02.01
// RCS-ID:      $Id: tglbtn.h 66992 2011-02-22 13:25:30Z VZ $
// Copyright:   (c) 2000 Johnny C. Norris II
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TOGGLEBUTTON_H_
#define _WX_TOGGLEBUTTON_H_

#include "wx/bitmap.h"

//-----------------------------------------------------------------------------
// wxBitmapToggleButton
//-----------------------------------------------------------------------------


class WXDLLIMPEXP_CORE wxBitmapToggleButton: public wxToggleButtonBase
{
public:
    // construction/destruction
    wxBitmapToggleButton() { Init(); }
    wxBitmapToggleButton(wxWindow *parent,
                   wxWindowID id,
                   const wxBitmap& label,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = 0,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxCheckBoxNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    // Create the control
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxBitmap& label,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize, long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxCheckBoxNameStr);

    // Get/set the value
    void SetValue(bool state);
    bool GetValue() const;

    // Set the label
    virtual void SetLabel(const wxString& label) { wxControl::SetLabel(label); }
    virtual void SetLabel(const wxBitmap& label);
    bool Enable(bool enable = true);

protected:
    void Init();

    wxBitmap  m_bitmap;
    wxBitmap  m_disabledBitmap;
    bool      m_capturing;
    bool      m_depressed,m_oldValue;

    void OnPaint(wxPaintEvent &event);
    void OnMouse(wxMouseEvent &event);
    void OnChar(wxKeyEvent &event);
    void OnSize(wxSizeEvent &event);

    virtual wxSize DoGetBestSize() const;

private:
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxBitmapToggleButton)
};

// Checkbox item (single checkbox)
class WXDLLIMPEXP_CORE wxToggleButton : public wxToggleButtonBase
{
public:
    wxToggleButton() {}
    wxToggleButton(wxWindow *parent,
                   wxWindowID id,
                   const wxString& label,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = 0,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxCheckBoxNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxCheckBoxNameStr);

    virtual void SetValue(bool value);
    virtual bool GetValue() const ;

    virtual void SetLabel(const wxString& label);

    virtual bool MSWCommand(WXUINT param, WXWORD id);
    virtual void Command(wxCommandEvent& event);

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const { return false; }

protected:
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }
    virtual wxSize DoGetBestSize() const;

    virtual WXDWORD MSWGetStyle(long flags, WXDWORD *exstyle = NULL) const;

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxToggleButton)
};

#endif // _WX_TOGGLEBUTTON_H_

