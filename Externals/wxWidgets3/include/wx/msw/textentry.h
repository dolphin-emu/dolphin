///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/textentry.h
// Purpose:     wxMSW-specific wxTextEntry implementation
// Author:      Vadim Zeitlin
// Created:     2007-09-26
// RCS-ID:      $Id: textentry.h 61834 2009-09-05 12:39:12Z JMS $
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_TEXTENTRY_H_
#define _WX_MSW_TEXTENTRY_H_

// ----------------------------------------------------------------------------
// wxTextEntry: common part of wxComboBox and (single line) wxTextCtrl
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTextEntry : public wxTextEntryBase
{
public:
    wxTextEntry()
    {
#if wxUSE_OLE
        m_enumStrings = NULL;
#endif // wxUSE_OLE
    }

    // implement wxTextEntryBase pure virtual methods
    virtual void WriteText(const wxString& text);
    virtual void Remove(long from, long to);

    virtual void Copy();
    virtual void Cut();
    virtual void Paste();

    virtual void Undo();
    virtual void Redo();
    virtual bool CanUndo() const;
    virtual bool CanRedo() const;

    virtual void SetInsertionPoint(long pos);
    virtual long GetInsertionPoint() const;
    virtual long GetLastPosition() const;

    virtual void SetSelection(long from, long to)
        { DoSetSelection(from, to); }
    virtual void GetSelection(long *from, long *to) const;

    // auto-completion uses COM under Windows so they won't work without
    // wxUSE_OLE as OleInitialize() is not called then
#if wxUSE_OLE
    virtual bool AutoComplete(const wxArrayString& choices);
    virtual bool AutoCompleteFileNames();
#endif // wxUSE_OLE

    virtual bool IsEditable() const;
    virtual void SetEditable(bool editable);

    virtual void SetMaxLength(unsigned long len);

#if wxUSE_UXTHEME
    virtual bool SetHint(const wxString& hint);
    virtual wxString GetHint() const;
#endif // wxUSE_UXTHEME

protected:
    virtual wxString DoGetValue() const;

    // this is really a hook for multiline text controls as the single line
    // ones don't need to ever scroll to show the selection but having it here
    // allows us to put Remove() in the base class
    enum
    {
        SetSel_NoScroll = 0,    // don't do anything special
        SetSel_Scroll = 1       // default: scroll to make the selection visible
    };
    virtual void DoSetSelection(long from, long to, int flags = SetSel_Scroll);

    // margins functions
    virtual bool DoSetMargins(const wxPoint& pt);
    virtual wxPoint DoGetMargins() const;

private:
    // implement this to return the HWND of the EDIT control
    virtual WXHWND GetEditHWND() const = 0;

#if wxUSE_OLE
    // enumerator for strings currently used for auto-completion or NULL
    class wxIEnumString *m_enumStrings;
#endif // wxUSE_OLE
};

#endif // _WX_MSW_TEXTENTRY_H_

