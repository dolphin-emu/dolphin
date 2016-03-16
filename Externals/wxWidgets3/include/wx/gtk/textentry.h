///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/textentry.h
// Purpose:     wxGTK-specific wxTextEntry implementation
// Author:      Vadim Zeitlin
// Created:     2007-09-24
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_TEXTENTRY_H_
#define _WX_GTK_TEXTENTRY_H_

typedef struct _GdkEventKey GdkEventKey;
typedef struct _GtkEditable GtkEditable;
typedef struct _GtkEntry GtkEntry;

// ----------------------------------------------------------------------------
// wxTextEntry: roughly corresponds to GtkEditable
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTextEntry : public wxTextEntryBase
{
public:
    wxTextEntry() { m_isUpperCase = false; }

    // implement wxTextEntryBase pure virtual methods
    virtual void WriteText(const wxString& text) wxOVERRIDE;
    virtual void Remove(long from, long to) wxOVERRIDE;

    virtual void Copy() wxOVERRIDE;
    virtual void Cut() wxOVERRIDE;
    virtual void Paste() wxOVERRIDE;

    virtual void Undo() wxOVERRIDE;
    virtual void Redo() wxOVERRIDE;
    virtual bool CanUndo() const wxOVERRIDE;
    virtual bool CanRedo() const wxOVERRIDE;

    virtual void SetInsertionPoint(long pos) wxOVERRIDE;
    virtual long GetInsertionPoint() const wxOVERRIDE;
    virtual long GetLastPosition() const wxOVERRIDE;

    virtual void SetSelection(long from, long to) wxOVERRIDE;
    virtual void GetSelection(long *from, long *to) const wxOVERRIDE;

    virtual bool IsEditable() const wxOVERRIDE;
    virtual void SetEditable(bool editable) wxOVERRIDE;

    virtual void SetMaxLength(unsigned long len) wxOVERRIDE;
    virtual void ForceUpper() wxOVERRIDE;

#ifdef __WXGTK3__
    virtual bool SetHint(const wxString& hint) wxOVERRIDE;
    virtual wxString GetHint() const wxOVERRIDE;
#endif

    // implementation only from now on
    void SendMaxLenEvent();
    bool GTKEntryOnInsertText(const char* text);
    bool GTKIsUpperCase() const { return m_isUpperCase; }

protected:
    // This method must be called from the derived class Create() to connect
    // the handlers for the clipboard (cut/copy/paste) events.
    void GTKConnectClipboardSignals(GtkWidget* entry);

    // And this one to connect "insert-text" signal.
    void GTKConnectInsertTextSignal(GtkEntry* entry);


    virtual void DoSetValue(const wxString& value, int flags) wxOVERRIDE;
    virtual wxString DoGetValue() const wxOVERRIDE;

    // margins functions
    virtual bool DoSetMargins(const wxPoint& pt) wxOVERRIDE;
    virtual wxPoint DoGetMargins() const wxOVERRIDE;

    virtual bool DoAutoCompleteStrings(const wxArrayString& choices) wxOVERRIDE;

    // Override the base class method to use GtkEntry IM context.
    virtual int GTKIMFilterKeypress(GdkEventKey* event) const;

private:
    // implement this to return the associated GtkEntry or another widget
    // implementing GtkEditable
    virtual GtkEditable *GetEditable() const = 0;

    // implement this to return the associated GtkEntry
    virtual GtkEntry *GetEntry() const = 0;

    bool m_isUpperCase;
};

// We don't need the generic version.
#define wxHAS_NATIVE_TEXT_FORCEUPPER

#endif // _WX_GTK_TEXTENTRY_H_

