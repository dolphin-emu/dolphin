/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/textctrl.h
// Purpose:     wxTextCtrl class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TEXTCTRL_H_
#define _WX_TEXTCTRL_H_

#if wxUSE_SYSTEM_OPTIONS
    // set this to 'true' if you want to use the 'classic' MLTE-based implementation
    // instead of the HIView-based implementation in 10.3 and upwards, the former
    // has more features (backgrounds etc.), but may show redraw artefacts and other
    // problems depending on your usage; hence, the default is 'false'.
    #define wxMAC_TEXTCONTROL_USE_MLTE wxT("mac.textcontrol-use-mlte")
    // set this to 'true' if you want editable text controls to have spell checking turned
    // on by default, you can change this setting individually on a control using MacCheckSpelling
    #define wxMAC_TEXTCONTROL_USE_SPELL_CHECKER wxT("mac.textcontrol-use-spell-checker")
#endif

#include "wx/control.h"
#include "wx/textctrl.h"

class WXDLLIMPEXP_CORE wxTextCtrl: public wxTextCtrlBase
{
    wxDECLARE_DYNAMIC_CLASS(wxTextCtrl);

public:
    wxTextCtrl()
    { Init(); }

    wxTextCtrl(wxWindow *parent,
        wxWindowID id,
        const wxString& value = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxTextCtrlNameStr)
    {
        Init();
        Create(parent, id, value, pos, size, style, validator, name);
    }

    virtual ~wxTextCtrl();

    bool Create(wxWindow *parent,
        wxWindowID id,
        const wxString& value = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxTextCtrlNameStr);

    // accessors
    // ---------

    virtual int GetLineLength(long lineNo) const wxOVERRIDE;
    virtual wxString GetLineText(long lineNo) const wxOVERRIDE;
    virtual int GetNumberOfLines() const wxOVERRIDE;

    virtual bool IsModified() const wxOVERRIDE;

    // operations
    // ----------


    // sets/clears the dirty flag
    virtual void MarkDirty() wxOVERRIDE;
    virtual void DiscardEdits() wxOVERRIDE;

    // text control under some platforms supports the text styles: these
    // methods apply the given text style to the given selection or to
    // set/get the style which will be used for all appended text
    virtual bool SetFont( const wxFont &font ) wxOVERRIDE;
    virtual bool GetStyle(long position, wxTextAttr& style) wxOVERRIDE;
    virtual bool SetStyle(long start, long end, const wxTextAttr& style) wxOVERRIDE;
    virtual bool SetDefaultStyle(const wxTextAttr& style) wxOVERRIDE;

    // translate between the position (which is just an index into the textctrl
    // considering all its contents as a single strings) and (x, y) coordinates
    // which represent column and line.
    virtual long XYToPosition(long x, long y) const wxOVERRIDE;
    virtual bool PositionToXY(long pos, long *x, long *y) const wxOVERRIDE;

    virtual void ShowPosition(long pos) wxOVERRIDE;

    // overrides so that we can send text updated events
    virtual void Copy() wxOVERRIDE;
    virtual void Cut() wxOVERRIDE;
    virtual void Paste() wxOVERRIDE;

    // Implementation
    // --------------
    virtual void Command(wxCommandEvent& event) wxOVERRIDE;

    virtual bool AcceptsFocus() const wxOVERRIDE;

    // callbacks
    void OnDropFiles(wxDropFilesEvent& event);
    void OnChar(wxKeyEvent& event); // Process 'enter' if required
    void OnKeyDown(wxKeyEvent& event); // Process clipboard shortcuts

    void OnCut(wxCommandEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnPaste(wxCommandEvent& event);
    void OnUndo(wxCommandEvent& event);
    void OnRedo(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnSelectAll(wxCommandEvent& event);

    void OnUpdateCut(wxUpdateUIEvent& event);
    void OnUpdateCopy(wxUpdateUIEvent& event);
    void OnUpdatePaste(wxUpdateUIEvent& event);
    void OnUpdateUndo(wxUpdateUIEvent& event);
    void OnUpdateRedo(wxUpdateUIEvent& event);
    void OnUpdateDelete(wxUpdateUIEvent& event);
    void OnUpdateSelectAll(wxUpdateUIEvent& event);

    void OnContextMenu(wxContextMenuEvent& event);

    virtual bool MacSetupCursor( const wxPoint& pt ) wxOVERRIDE;

    virtual void MacVisibilityChanged() wxOVERRIDE;
    virtual void MacSuperChangedPosition() wxOVERRIDE;
    virtual void MacCheckSpelling(bool check);

protected:
    // common part of all ctors
    void Init();

    virtual wxSize DoGetBestSize() const wxOVERRIDE;

    // flag is set to true when the user edits the controls contents
    bool m_dirty;

    virtual void EnableTextChangedEvents(bool WXUNUSED(enable)) wxOVERRIDE
    {
        // nothing to do here as the events are never generated when we change
        // the controls value programmatically anyhow
    }

private :
    wxMenu  *m_privateContextMenu;

    wxDECLARE_EVENT_TABLE();
};

#endif // _WX_TEXTCTRL_H_
