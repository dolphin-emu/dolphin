/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/combobox.cpp
// Purpose:     wxComboBox class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: combobox.cpp 66555 2011-01-04 08:31:53Z SC $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_COMBOBOX

#include "wx/combobox.h"

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
    #include "wx/settings.h"
    #include "wx/log.h"
    // for wxEVT_COMMAND_TEXT_ENTER
    #include "wx/textctrl.h"
    #include "wx/app.h"
    #include "wx/brush.h"
#endif

#include "wx/clipbrd.h"
#include "wx/dynlib.h"
#include "wx/wupdlock.h"
#include "wx/msw/private.h"

#if wxUSE_UXTHEME
    #include "wx/msw/uxtheme.h"
#endif

#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h"
#endif // wxUSE_TOOLTIPS

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxComboBox, wxControl)
    EVT_MENU(wxID_CUT, wxComboBox::OnCut)
    EVT_MENU(wxID_COPY, wxComboBox::OnCopy)
    EVT_MENU(wxID_PASTE, wxComboBox::OnPaste)
    EVT_MENU(wxID_UNDO, wxComboBox::OnUndo)
    EVT_MENU(wxID_REDO, wxComboBox::OnRedo)
    EVT_MENU(wxID_CLEAR, wxComboBox::OnDelete)
    EVT_MENU(wxID_SELECTALL, wxComboBox::OnSelectAll)

    EVT_UPDATE_UI(wxID_CUT, wxComboBox::OnUpdateCut)
    EVT_UPDATE_UI(wxID_COPY, wxComboBox::OnUpdateCopy)
    EVT_UPDATE_UI(wxID_PASTE, wxComboBox::OnUpdatePaste)
    EVT_UPDATE_UI(wxID_UNDO, wxComboBox::OnUpdateUndo)
    EVT_UPDATE_UI(wxID_REDO, wxComboBox::OnUpdateRedo)
    EVT_UPDATE_UI(wxID_CLEAR, wxComboBox::OnUpdateDelete)
    EVT_UPDATE_UI(wxID_SELECTALL, wxComboBox::OnUpdateSelectAll)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// function prototypes
// ----------------------------------------------------------------------------

LRESULT APIENTRY _EXPORT wxComboEditWndProc(HWND hWnd,
                                            UINT message,
                                            WPARAM wParam,
                                            LPARAM lParam);

// ---------------------------------------------------------------------------
// global vars
// ---------------------------------------------------------------------------

// the pointer to standard radio button wnd proc
static WNDPROC gs_wndprocEdit = (WNDPROC)NULL;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wnd proc for subclassed edit control
// ----------------------------------------------------------------------------

LRESULT APIENTRY _EXPORT wxComboEditWndProc(HWND hWnd,
                                            UINT message,
                                            WPARAM wParam,
                                            LPARAM lParam)
{
    HWND hwndCombo = ::GetParent(hWnd);
    wxWindow *win = wxFindWinFromHandle((WXHWND)hwndCombo);

    switch ( message )
    {
        // forward some messages to the combobox to generate the appropriate
        // wxEvents from them
        case WM_KEYUP:
        case WM_KEYDOWN:
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            {
                wxComboBox *combo = wxDynamicCast(win, wxComboBox);
                if ( !combo )
                {
                    // we can get WM_KILLFOCUS while our parent is already half
                    // destroyed and hence doesn't look like a combobx any
                    // longer, check for it to avoid bogus assert failures
                    if ( !win->IsBeingDeleted() )
                    {
                        wxFAIL_MSG( wxT("should have combo as parent") );
                    }
                }
                else if ( combo->MSWProcessEditMsg(message, wParam, lParam) )
                {
                    // handled by parent
                    return 0;
                }
            }
            break;

        case WM_GETDLGCODE:
            {
                wxCHECK_MSG( win, 0, wxT("should have a parent") );

                if ( win->GetWindowStyle() & wxTE_PROCESS_ENTER )
                {
                    // need to return a custom dlg code or we'll never get it
                    return DLGC_WANTMESSAGE;
                }
            }
            break;
    }

    return ::CallWindowProc(CASTWNDPROC gs_wndprocEdit, hWnd, message, wParam, lParam);
}

// ----------------------------------------------------------------------------
// wxComboBox callbacks
// ----------------------------------------------------------------------------

WXLRESULT wxComboBox::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    // TODO: handle WM_CTLCOLOR messages from our EDIT control to be able to
    //       set its colour correctly (to be the same as our own one)

    switch ( nMsg )
    {
        case WM_SIZE:
        // wxStaticBox can generate this message, when modifying the control's style.
        // This causes the content of the combobox to be selected, for some reason.
        case WM_STYLECHANGED:
            {
                // combobox selection sometimes spontaneously changes when its
                // size changes, restore it to the old value if necessary
                if ( !GetEditHWNDIfAvailable() )
                    break;

                long fromOld, toOld;
                GetSelection(&fromOld, &toOld);

                // if an editable combobox has a not empty text not from the
                // list, it tries to autocomplete it from the list when it is
                // resized, but we don't want this to happen as it doesn't seem
                // to make any sense, so we forcefully restore the old text
                wxString textOld;
                if ( !HasFlag(wxCB_READONLY) && GetCurrentSelection() == -1 )
                    textOld = GetValue();

                // eliminate flickering during following hacks
                wxWindowUpdateLocker lock(this);

                WXLRESULT result = wxChoice::MSWWindowProc(nMsg, wParam, lParam);

                if ( !textOld.empty() && GetValue() != textOld )
                    SetLabel(textOld);

                long fromNew, toNew;
                GetSelection(&fromNew, &toNew);

                if ( fromOld != fromNew || toOld != toNew )
                {
                    SetSelection(fromOld, toOld);
                }

                return result;
            }
    }

    return wxChoice::MSWWindowProc(nMsg, wParam, lParam);
}

bool wxComboBox::MSWProcessEditMsg(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam)
{
    switch ( msg )
    {
        case WM_CHAR:
            // for compatibility with wxTextCtrl, generate a special message
            // when Enter is pressed
            if ( wParam == VK_RETURN )
            {
                if (SendMessage(GetHwnd(), CB_GETDROPPEDSTATE, 0, 0))
                    return false;

                wxCommandEvent event(wxEVT_COMMAND_TEXT_ENTER, m_windowId);

                const int sel = GetSelection();
                event.SetInt(sel);
                event.SetString(GetValue());
                InitCommandEventWithItems(event, sel);

                if ( ProcessCommand(event) )
                {
                    // don't let the event through to the native control
                    // because it doesn't need it and may generate an annoying
                    // beep if it gets it
                    return true;
                }
            }
            // fall through

        case WM_SYSCHAR:
            return HandleChar(wParam, lParam);

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            return HandleKeyDown(wParam, lParam);

        case WM_SYSKEYUP:
        case WM_KEYUP:
            return HandleKeyUp(wParam, lParam);

        case WM_SETFOCUS:
            return HandleSetFocus((WXHWND)wParam);

        case WM_KILLFOCUS:
            return HandleKillFocus((WXHWND)wParam);

        case WM_CUT:
        case WM_COPY:
        case WM_PASTE:
            return HandleClipboardEvent(msg);
    }

    return false;
}

bool wxComboBox::MSWCommand(WXUINT param, WXWORD id)
{
    int sel = -1;
    wxString value;

    switch ( param )
    {
        case CBN_DROPDOWN:
            // remember the last selection, just as wxChoice does
            m_lastAcceptedSelection = GetCurrentSelection();
            if ( m_lastAcceptedSelection == -1 )
            {
                // but unlike with wxChoice we may have no selection but still
                // have some text and we should avoid erasing it if the drop
                // down is cancelled (see #8474)
                m_lastAcceptedSelection = wxID_NONE;
            }
            {
                wxCommandEvent event(wxEVT_COMMAND_COMBOBOX_DROPDOWN, GetId());
                event.SetEventObject(this);
                ProcessCommand(event);
            }
            break;
        case CBN_CLOSEUP:
            {
                wxCommandEvent event(wxEVT_COMMAND_COMBOBOX_CLOSEUP, GetId());
                event.SetEventObject(this);
                ProcessCommand(event);
            }
            break;
        case CBN_SELENDOK:
#ifndef __SMARTPHONE__
            // we need to reset this to prevent the selection from being undone
            // by wxChoice, see wxChoice::MSWCommand() and comments there
            m_lastAcceptedSelection = wxID_NONE;
#endif

            // set these variables so that they could be also fixed in
            // CBN_EDITCHANGE below
            sel = GetSelection();
            value = GetStringSelection();

            // this string is going to become the new combobox value soon but
            // we need it to be done right now, otherwise the event handler
            // could get a wrong value when it calls our GetValue()
            ::SetWindowText(GetHwnd(), value.wx_str());

            {
                wxCommandEvent event(wxEVT_COMMAND_COMBOBOX_SELECTED, GetId());
                event.SetInt(sel);
                event.SetString(value);
                InitCommandEventWithItems(event, sel);

                ProcessCommand(event);
            }

            // fall through: for compability with wxGTK, also send the text
            // update event when the selection changes (this also seems more
            // logical as the text does change)

        case CBN_EDITCHANGE:
            if ( m_allowTextEvents )
            {
                wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, GetId());

                // if sel != -1, value was already initialized above
                if ( sel == -1 )
                {
                    value = wxGetWindowText(GetHwnd());
                }

                event.SetString(value);
                InitCommandEventWithItems(event, sel);

                ProcessCommand(event);
            }
            break;

        default:
            return wxChoice::MSWCommand(param, id);
    }

    // skip wxChoice version as it would generate its own events for
    // CBN_SELENDOK and also interfere with our handling of CBN_DROPDOWN
    return true;
}

bool wxComboBox::MSWShouldPreProcessMessage(WXMSG *pMsg)
{
    // prevent command accelerators from stealing editing
    // hotkeys when we have the focus
    if (wxIsCtrlDown())
    {
        WPARAM vkey = pMsg->wParam;

        switch (vkey)
        {
            case 'C':
            case 'V':
            case 'X':
            case VK_INSERT:
            case VK_DELETE:
            case VK_HOME:
            case VK_END:
                return false;
        }
    }

    return wxChoice::MSWShouldPreProcessMessage(pMsg);
}

WXHWND wxComboBox::GetEditHWNDIfAvailable() const
{
#if wxUSE_DYNLIB_CLASS
#if defined(WINVER) && WINVER >= 0x0500
    typedef BOOL (WINAPI *GetComboBoxInfo_t)(HWND, COMBOBOXINFO*);
    static GetComboBoxInfo_t s_pfnGetComboBoxInfo = NULL;
    static bool s_triedToLoad = false;
    if ( !s_triedToLoad )
    {
        s_triedToLoad = true;
        wxLoadedDLL dllUser32("user32.dll");
        wxDL_INIT_FUNC(s_pfn, GetComboBoxInfo, dllUser32);
    }

    if ( s_pfnGetComboBoxInfo )
    {
        WinStruct<COMBOBOXINFO> info;
        (*s_pfnGetComboBoxInfo)(GetHwnd(), &info);
        return info.hwndItem;
    }
#endif // WINVER >= 0x0500
#endif // wxUSE_DYNLIB_CLASS

    if (HasFlag(wxCB_SIMPLE))
    {
        POINT pt;
        pt.x = pt.y = 4;
        return (WXHWND) ::ChildWindowFromPoint(GetHwnd(), pt);
    }

    // notice that a slightly safer alternative could be to use FindWindowEx()
    // but it's not available under WinCE so just take the first child for now
    // to keep one version of the code for all platforms and fix it later if
    // problems are discovered

    // we assume that the only child of the combobox is the edit window
    return (WXHWND)::GetWindow(GetHwnd(), GW_CHILD);
}

WXHWND wxComboBox::GetEditHWND() const
{
    // this function should not be called for wxCB_READONLY controls, it is
    // the callers responsibility to check this
    wxASSERT_MSG( !HasFlag(wxCB_READONLY),
                  wxT("read-only combobox doesn't have any edit control") );

    WXHWND hWndEdit = GetEditHWNDIfAvailable();
    wxASSERT_MSG( hWndEdit, wxT("combobox without edit control?") );

    return hWndEdit;
}

wxWindow *wxComboBox::GetEditableWindow()
{
    wxASSERT_MSG( !HasFlag(wxCB_READONLY),
                  wxT("read-only combobox doesn't have any edit control") );

    return this;
}

// ----------------------------------------------------------------------------
// wxComboBox creation
// ----------------------------------------------------------------------------

bool wxComboBox::Create(wxWindow *parent, wxWindowID id,
                        const wxString& value,
                        const wxPoint& pos,
                        const wxSize& size,
                        int n, const wxString choices[],
                        long style,
                        const wxValidator& validator,
                        const wxString& name)
{
    // pretend that wxComboBox is hidden while it is positioned and resized and
    // show it only right before leaving this method because otherwise there is
    // some noticeable flicker while the control rearranges itself
    m_isShown = false;

    if ( !CreateAndInit(parent, id, pos, size, n, choices, style,
                        validator, name) )
        return false;

    // we shouldn't call SetValue() for an empty string because this would
    // (correctly) result in an assert with a read only combobox and is useless
    // for the other ones anyhow
    if ( !value.empty() )
        SetValue(value);

    // a (not read only) combobox is, in fact, 2 controls: the combobox itself
    // and an edit control inside it and if we want to catch events from this
    // edit control, we must subclass it as well
    if ( !(style & wxCB_READONLY) )
    {
        gs_wndprocEdit = wxSetWindowProc((HWND)GetEditHWND(), wxComboEditWndProc);
    }

    // and finally, show the control
    Show(true);

    return true;
}

bool wxComboBox::Create(wxWindow *parent, wxWindowID id,
                        const wxString& value,
                        const wxPoint& pos,
                        const wxSize& size,
                        const wxArrayString& choices,
                        long style,
                        const wxValidator& validator,
                        const wxString& name)
{
    wxCArrayString chs(choices);
    return Create(parent, id, value, pos, size, chs.GetCount(),
                  chs.GetStrings(), style, validator, name);
}

WXDWORD wxComboBox::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    // we never have an external border
    WXDWORD msStyle = wxChoice::MSWGetStyle
                      (
                        (style & ~wxBORDER_MASK) | wxBORDER_NONE, exstyle
                      );

    // usually WS_TABSTOP is added by wxControl::MSWGetStyle() but as we're
    // created hidden (see Create() above), it is not done for us but we still
    // want to have this style
    msStyle |= WS_TABSTOP;

    // remove the style always added by wxChoice
    msStyle &= ~CBS_DROPDOWNLIST;

    if ( style & wxCB_READONLY )
        msStyle |= CBS_DROPDOWNLIST;
#ifndef __WXWINCE__
    else if ( style & wxCB_SIMPLE )
        msStyle |= CBS_SIMPLE; // A list (shown always) and edit control
#endif
    else
        msStyle |= CBS_DROPDOWN;

    // there is no reason to not always use CBS_AUTOHSCROLL, so do use it
    msStyle |= CBS_AUTOHSCROLL;

    // NB: we used to also add CBS_NOINTEGRALHEIGHT here but why?

    return msStyle;
}

// ----------------------------------------------------------------------------
// wxComboBox text control-like methods
// ----------------------------------------------------------------------------

wxString wxComboBox::GetValue() const
{
    return HasFlag(wxCB_READONLY) ? GetStringSelection()
                                  : wxTextEntry::GetValue();
}

void wxComboBox::SetValue(const wxString& value)
{
    if ( HasFlag(wxCB_READONLY) )
        SetStringSelection(value);
    else
        wxTextEntry::SetValue(value);
}

void wxComboBox::Clear()
{
    wxChoice::Clear();
    if ( !HasFlag(wxCB_READONLY) )
        wxTextEntry::Clear();
}

void wxComboBox::GetSelection(long *from, long *to) const
{
    if ( !HasFlag(wxCB_READONLY) )
    {
        wxTextEntry::GetSelection(from, to);
    }
    else // text selection doesn't make sense for read only comboboxes
    {
        if ( from )
            *from = -1;
        if ( to )
            *to = -1;
    }
}

bool wxComboBox::IsEditable() const
{
    return !HasFlag(wxCB_READONLY) && wxTextEntry::IsEditable();
}

// ----------------------------------------------------------------------------
// standard event handling
// ----------------------------------------------------------------------------

void wxComboBox::OnCut(wxCommandEvent& WXUNUSED(event))
{
    Cut();
}

void wxComboBox::OnCopy(wxCommandEvent& WXUNUSED(event))
{
    Copy();
}

void wxComboBox::OnPaste(wxCommandEvent& WXUNUSED(event))
{
    Paste();
}

void wxComboBox::OnUndo(wxCommandEvent& WXUNUSED(event))
{
    Undo();
}

void wxComboBox::OnRedo(wxCommandEvent& WXUNUSED(event))
{
    Redo();
}

void wxComboBox::OnDelete(wxCommandEvent& WXUNUSED(event))
{
    RemoveSelection();
}

void wxComboBox::OnSelectAll(wxCommandEvent& WXUNUSED(event))
{
    SelectAll();
}

void wxComboBox::OnUpdateCut(wxUpdateUIEvent& event)
{
    event.Enable( CanCut() );
}

void wxComboBox::OnUpdateCopy(wxUpdateUIEvent& event)
{
    event.Enable( CanCopy() );
}

void wxComboBox::OnUpdatePaste(wxUpdateUIEvent& event)
{
    event.Enable( CanPaste() );
}

void wxComboBox::OnUpdateUndo(wxUpdateUIEvent& event)
{
    event.Enable( IsEditable() && CanUndo() );
}

void wxComboBox::OnUpdateRedo(wxUpdateUIEvent& event)
{
    event.Enable( IsEditable() && CanRedo() );
}

void wxComboBox::OnUpdateDelete(wxUpdateUIEvent& event)
{
    event.Enable(IsEditable() && HasSelection());
}

void wxComboBox::OnUpdateSelectAll(wxUpdateUIEvent& event)
{
    event.Enable(IsEditable() && !wxTextEntry::IsEmpty());
}

void wxComboBox::MSWDoPopupOrDismiss(bool show)
{
    wxASSERT_MSG( !HasFlag(wxCB_SIMPLE),
                  wxT("can't popup/dismiss the list for simple combo box") );

    // we *must* set focus to the combobox before showing or hiding the drop
    // down as without this we get WM_LBUTTONDOWN messages with invalid HWND
    // when hiding it (whether programmatically or manually) resulting in a
    // crash when we pass them to IsDialogMessage()
    //
    // this can be seen in the combo page of the widgets sample under Windows 7
    SetFocus();

    ::SendMessage(GetHwnd(), CB_SHOWDROPDOWN, show, 0);
}

#if wxUSE_TOOLTIPS

void wxComboBox::DoSetToolTip(wxToolTip *tip)
{
    wxChoice::DoSetToolTip(tip);

    if ( tip && !HasFlag(wxCB_READONLY) )
        tip->Add(GetEditHWND());
}

#endif // wxUSE_TOOLTIPS

#if wxUSE_UXTHEME

bool wxComboBox::SetHint(const wxString& hintOrig)
{
    wxString hint(hintOrig);
    if ( wxUxThemeEngine::GetIfActive() )
    {
        // under XP (but not Vista) there is a bug in cue banners
        // implementation for combobox edit control: the first character is
        // partially chopped off, so prepend a space to make it fully visible
        hint.insert(0, " ");
    }

    return wxTextEntry::SetHint(hint);
}

#endif // wxUSE_UXTHEME

#endif // wxUSE_COMBOBOX
