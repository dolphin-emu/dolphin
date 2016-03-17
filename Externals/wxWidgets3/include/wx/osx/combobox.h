/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/combobox.h
// Purpose:     wxComboBox class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COMBOBOX_H_
#define _WX_COMBOBOX_H_

#include "wx/containr.h"
#include "wx/choice.h"
#include "wx/textctrl.h"

WX_DEFINE_ARRAY( char * , wxComboBoxDataArray ) ;

// forward declaration of private implementation classes

class wxComboBoxText;
class wxComboBoxChoice;
class wxComboWidgetImpl;

// Combobox item
class WXDLLIMPEXP_CORE wxComboBox :
    public wxWindowWithItems<
                wxControl,
                wxComboBoxBase>
{
    wxDECLARE_DYNAMIC_CLASS(wxComboBox);

 public:
    virtual ~wxComboBox();

    // callback functions
    virtual void DelegateTextChanged( const wxString& value );
    virtual void DelegateChoice( const wxString& value );

    wxComboBox() { }

    wxComboBox(wxWindow *parent, wxWindowID id,
           const wxString& value = wxEmptyString,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           int n = 0, const wxString choices[] = NULL,
           long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxComboBoxNameStr)
    {
        Create(parent, id, value, pos, size, n, choices, style, validator, name);
    }

    wxComboBox(wxWindow *parent, wxWindowID id,
           const wxString& value,
           const wxPoint& pos,
           const wxSize& size,
           const wxArrayString& choices,
           long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxComboBoxNameStr)
    {
        Create(parent, id, value, pos, size, choices, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
           const wxString& value = wxEmptyString,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           int n = 0, const wxString choices[] = NULL,
           long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxComboBoxNameStr);

    bool Create(wxWindow *parent, wxWindowID id,
           const wxString& value,
           const wxPoint& pos,
           const wxSize& size,
           const wxArrayString& choices,
           long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxComboBoxNameStr);

    virtual int GetSelection() const;
    virtual void GetSelection(long *from, long *to) const;
    virtual void SetSelection(int n);
    virtual void SetSelection(long from, long to);
    virtual int FindString(const wxString& s, bool bCase = false) const;
    virtual wxString GetString(unsigned int n) const;
    virtual wxString GetStringSelection() const;
    virtual void SetString(unsigned int n, const wxString& s);

    virtual unsigned int GetCount() const;

    virtual void SetValue(const wxString& value);
// these methods are provided by wxTextEntry for the native impl.

#if wxOSX_USE_COCOA
    virtual void Popup();
    virtual void Dismiss();
#endif // wxOSX_USE_COCOA


    // osx specific event handling common for all osx-ports

    virtual bool        OSXHandleClicked( double timestampsec );

#if wxOSX_USE_COCOA
    wxComboWidgetImpl* GetComboPeer() const;
#endif
protected:
    // List functions
    virtual void DoDeleteOneItem(unsigned int n);
    virtual void DoClear();

    // wxTextEntry functions
    virtual wxWindow *GetEditableWindow() { return this; }

    // override the base class virtuals involved in geometry calculations
    virtual wxSize DoGetBestSize() const;

    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);

    virtual void DoSetItemClientData(unsigned int n, void* clientData);
    virtual void * DoGetItemClientData(unsigned int n) const;


    virtual void EnableTextChangedEvents(bool enable);

    // the subcontrols
    wxComboBoxText*     m_text;
    wxComboBoxChoice*   m_choice;

    wxComboBoxDataArray m_datas;
};

#endif // _WX_COMBOBOX_H_
