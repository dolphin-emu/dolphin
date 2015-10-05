/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/radiobox.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_RADIOBOX_H_
#define _WX_GTK_RADIOBOX_H_

#include "wx/bitmap.h"

class WXDLLIMPEXP_FWD_CORE wxGTKRadioButtonInfo;

#include "wx/list.h"

WX_DECLARE_EXPORTED_LIST(wxGTKRadioButtonInfo, wxRadioBoxButtonsInfoList);


//-----------------------------------------------------------------------------
// wxRadioBox
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxRadioBox : public wxControl,
                                    public wxRadioBoxBase
{
public:
    // ctors and dtor
    wxRadioBox() { }
    wxRadioBox(wxWindow *parent,
               wxWindowID id,
               const wxString& title,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               int n = 0,
               const wxString choices[] = (const wxString *) NULL,
               int majorDim = 0,
               long style = wxRA_SPECIFY_COLS,
               const wxValidator& val = wxDefaultValidator,
               const wxString& name = wxRadioBoxNameStr)
    {
        Create( parent, id, title, pos, size, n, choices, majorDim, style, val, name );
    }

    wxRadioBox(wxWindow *parent,
               wxWindowID id,
               const wxString& title,
               const wxPoint& pos,
               const wxSize& size,
               const wxArrayString& choices,
               int majorDim = 0,
               long style = wxRA_SPECIFY_COLS,
               const wxValidator& val = wxDefaultValidator,
               const wxString& name = wxRadioBoxNameStr)
    {
        Create( parent, id, title, pos, size, choices, majorDim, style, val, name );
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int n = 0,
                const wxString choices[] = (const wxString *) NULL,
                int majorDim = 0,
                long style = wxRA_SPECIFY_COLS,
                const wxValidator& val = wxDefaultValidator,
                const wxString& name = wxRadioBoxNameStr);
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                int majorDim = 0,
                long style = wxRA_SPECIFY_COLS,
                const wxValidator& val = wxDefaultValidator,
                const wxString& name = wxRadioBoxNameStr);

    virtual ~wxRadioBox();


    // implement wxItemContainerImmutable methods
    virtual unsigned int GetCount() const wxOVERRIDE;

    virtual wxString GetString(unsigned int n) const wxOVERRIDE;
    virtual void SetString(unsigned int n, const wxString& s) wxOVERRIDE;

    virtual void SetSelection(int n) wxOVERRIDE;
    virtual int GetSelection() const wxOVERRIDE;


    // implement wxRadioBoxBase methods
    virtual bool Show(unsigned int n, bool show = true) wxOVERRIDE;
    virtual bool Enable(unsigned int n, bool enable = true) wxOVERRIDE;

    virtual bool IsItemEnabled(unsigned int n) const wxOVERRIDE;
    virtual bool IsItemShown(unsigned int n) const wxOVERRIDE;


    // override some base class methods to operate on radiobox itself too
    virtual bool Show( bool show = true ) wxOVERRIDE;
    virtual bool Enable( bool enable = true ) wxOVERRIDE;

    virtual void SetLabel( const wxString& label ) wxOVERRIDE;

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    virtual int GetItemFromPoint( const wxPoint& pt ) const wxOVERRIDE;
#if wxUSE_HELP
    // override virtual wxWindow::GetHelpTextAtPoint to use common platform independent
    // wxRadioBoxBase::DoGetHelpTextAtPoint from the platform independent
    // base class-interface wxRadioBoxBase.
    virtual wxString GetHelpTextAtPoint(const wxPoint & pt, wxHelpEvent::Origin origin) const wxOVERRIDE
    {
        return wxRadioBoxBase::DoGetHelpTextAtPoint( this, pt, origin );
    }
#endif // wxUSE_HELP

    // implementation
    // --------------

    void GtkDisableEvents();
    void GtkEnableEvents();
#if wxUSE_TOOLTIPS
    virtual void GTKApplyToolTip(const char* tip) wxOVERRIDE;
#endif // wxUSE_TOOLTIPS

    wxRadioBoxButtonsInfoList   m_buttonsInfo;

protected:
    virtual wxBorder GetDefaultBorder() const wxOVERRIDE { return wxBORDER_NONE; }

#if wxUSE_TOOLTIPS
    virtual void DoSetItemToolTip(unsigned int n, wxToolTip *tooltip) wxOVERRIDE;
#endif

    virtual void DoApplyWidgetStyle(GtkRcStyle *style) wxOVERRIDE;
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const wxOVERRIDE;

    virtual bool GTKNeedsToFilterSameWindowFocus() const wxOVERRIDE { return true; }

    virtual bool GTKWidgetNeedsMnemonic() const wxOVERRIDE;
    virtual void GTKWidgetDoSetMnemonic(GtkWidget* w) wxOVERRIDE;

private:
    wxDECLARE_DYNAMIC_CLASS(wxRadioBox);
};

#endif // _WX_GTK_RADIOBOX_H_
