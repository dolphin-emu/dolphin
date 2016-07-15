/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/spinbutt.h
// Purpose:     wxSpinButton class
// Author:      Robert Roebling
// Modified by:
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_SPINBUTT_H_
#define _WX_GTK_SPINBUTT_H_

//-----------------------------------------------------------------------------
// wxSpinButton
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSpinButton : public wxSpinButtonBase
{
public:
    wxSpinButton();
    wxSpinButton(wxWindow *parent,
                 wxWindowID id = -1,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxSP_VERTICAL,
                 const wxString& name = wxSPIN_BUTTON_NAME)
    {
        Create(parent, id, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id = -1,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSP_VERTICAL,
                const wxString& name = wxSPIN_BUTTON_NAME);

    virtual int GetValue() const wxOVERRIDE;
    virtual void SetValue( int value ) wxOVERRIDE;
    virtual void SetRange( int minVal, int maxVal ) wxOVERRIDE;
    virtual int GetMin() const wxOVERRIDE;
    virtual int GetMax() const wxOVERRIDE;

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    virtual bool Enable( bool enable = true ) wxOVERRIDE;

    // implementation
    int m_pos;

protected:
    void GtkDisableEvents() const;
    void GtkEnableEvents() const;

    virtual wxSize DoGetBestSize() const wxOVERRIDE;
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const wxOVERRIDE;

private:
    typedef wxSpinButtonBase base_type;

    wxDECLARE_DYNAMIC_CLASS(wxSpinButton);
};

#endif // _WX_GTK_SPINBUTT_H_
