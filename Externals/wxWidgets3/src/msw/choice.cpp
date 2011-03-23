/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/choice.cpp
// Purpose:     wxChoice
// Author:      Julian Smart
// Modified by: Vadim Zeitlin to derive from wxChoiceBase
// Created:     04/01/98
// RCS-ID:      $Id: choice.cpp 67280 2011-03-22 14:17:38Z DS $
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

#if wxUSE_CHOICE && !(defined(__SMARTPHONE__) && defined(__WXWINCE__))

#include "wx/choice.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/app.h"
    #include "wx/log.h"
    #include "wx/brush.h"
    #include "wx/settings.h"
#endif

#include "wx/msw/private.h"

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// creation
// ----------------------------------------------------------------------------

bool wxChoice::Create(wxWindow *parent,
                      wxWindowID id,
                      const wxPoint& pos,
                      const wxSize& size,
                      int n, const wxString choices[],
                      long style,
                      const wxValidator& validator,
                      const wxString& name)
{
    // Experience shows that wxChoice vs. wxComboBox distinction confuses
    // quite a few people - try to help them
    wxASSERT_MSG( !(style & wxCB_DROPDOWN) &&
                  !(style & wxCB_READONLY) &&
                  !(style & wxCB_SIMPLE),
                  wxT("this style flag is ignored by wxChoice, you ")
                  wxT("probably want to use a wxComboBox") );

    return CreateAndInit(parent, id, pos, size, n, choices, style,
                         validator, name);
}

bool wxChoice::CreateAndInit(wxWindow *parent,
                             wxWindowID id,
                             const wxPoint& pos,
                             const wxSize& size,
                             int n, const wxString choices[],
                             long style,
                             const wxValidator& validator,
                             const wxString& name)
{
    // initialize wxControl
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    // now create the real HWND
    if ( !MSWCreateControl(wxT("COMBOBOX"), wxEmptyString, pos, size) )
        return false;


    // initialize the controls contents
    for ( int i = 0; i < n; i++ )
    {
        Append(choices[i]);
    }

    // and now we may finally size the control properly (if needed)
    SetInitialSize(size);

    return true;
}

void wxChoice::SetLabel(const wxString& label)
{
    if ( FindString(label) == wxNOT_FOUND )
    {
        // unless we explicitly do this here, CB_GETCURSEL will continue to
        // return the index of the previously selected item which will result
        // in wrongly replacing the value being set now with the previously
        // value if the user simply opens and closes (without selecting
        // anything) the combobox popup
        SetSelection(-1);
    }

    wxChoiceBase::SetLabel(label);
}

bool wxChoice::Create(wxWindow *parent,
                      wxWindowID id,
                      const wxPoint& pos,
                      const wxSize& size,
                      const wxArrayString& choices,
                      long style,
                      const wxValidator& validator,
                      const wxString& name)
{
    wxCArrayString chs(choices);
    return Create(parent, id, pos, size, chs.GetCount(), chs.GetStrings(),
                  style, validator, name);
}

bool wxChoice::MSWShouldPreProcessMessage(WXMSG *pMsg)
{
    MSG *msg = (MSG *) pMsg;

    // if the dropdown list is visible, don't preprocess certain keys
    if ( msg->message == WM_KEYDOWN
        && (msg->wParam == VK_ESCAPE || msg->wParam == VK_RETURN) )
    {
        if (::SendMessage(GetHwndOf(this), CB_GETDROPPEDSTATE, 0, 0))
        {
            return false;
        }
    }

    return wxControl::MSWShouldPreProcessMessage(pMsg);
}

WXDWORD wxChoice::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    // we never have an external border
    WXDWORD msStyle = wxControl::MSWGetStyle
                      (
                        (style & ~wxBORDER_MASK) | wxBORDER_NONE, exstyle
                      );

    // WS_CLIPSIBLINGS is useful with wxChoice and doesn't seem to result in
    // any problems
    msStyle |= WS_CLIPSIBLINGS;

    // wxChoice-specific styles
    msStyle |= CBS_DROPDOWNLIST | WS_HSCROLL | WS_VSCROLL;
    if ( style & wxCB_SORT )
        msStyle |= CBS_SORT;

    return msStyle;
}

#ifndef EP_EDITTEXT
    #define EP_EDITTEXT         1
    #define ETS_NORMAL          1
#endif

wxVisualAttributes
wxChoice::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    // it is important to return valid values for all attributes from here,
    // GetXXX() below rely on this
    wxVisualAttributes attrs;

    // FIXME: Use better dummy window?
    wxWindow* wnd = wxTheApp->GetTopWindow();
    if (!wnd)
        return attrs;

    attrs.font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);

    // there doesn't seem to be any way to get the text colour using themes
    // API: TMT_TEXTCOLOR doesn't work neither for EDIT nor COMBOBOX
    attrs.colFg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);

    // NB: use EDIT, not COMBOBOX (the latter works in XP but not Vista)
    attrs.colBg = wnd->MSWGetThemeColour(L"EDIT",
                                         EP_EDITTEXT,
                                         ETS_NORMAL,
                                         ThemeColourBackground,
                                         wxSYS_COLOUR_WINDOW);

    return attrs;
}

wxChoice::~wxChoice()
{
    Clear();
}

// ----------------------------------------------------------------------------
// adding/deleting items to/from the list
// ----------------------------------------------------------------------------

int wxChoice::DoInsertItems(const wxArrayStringsAdapter& items,
                            unsigned int pos,
                            void **clientData, wxClientDataType type)
{
    MSWAllocStorage(items, CB_INITSTORAGE);

    const bool append = pos == GetCount();

    // use CB_ADDSTRING when appending at the end to make sure the control is
    // resorted if it has wxCB_SORT style
    const unsigned msg = append ? CB_ADDSTRING : CB_INSERTSTRING;

    if ( append )
        pos = 0;

    int n = wxNOT_FOUND;
    const unsigned numItems = items.GetCount();
    for ( unsigned i = 0; i < numItems; ++i )
    {
        n = MSWInsertOrAppendItem(pos, items[i], msg);
        if ( n == wxNOT_FOUND )
            return n;

        if ( !append )
            pos++;

        AssignNewItemClientData(n, clientData, i, type);
    }

    // we need to refresh our size in order to have enough space for the
    // newly added items
    if ( !IsFrozen() )
        MSWUpdateDropDownHeight();

    InvalidateBestSize();

    return n;
}

void wxChoice::DoDeleteOneItem(unsigned int n)
{
    wxCHECK_RET( IsValid(n), wxT("invalid item index in wxChoice::Delete") );

    SendMessage(GetHwnd(), CB_DELETESTRING, n, 0);

    if ( !IsFrozen() )
        MSWUpdateDropDownHeight();

    InvalidateBestSize();
}

void wxChoice::DoClear()
{
    SendMessage(GetHwnd(), CB_RESETCONTENT, 0, 0);

    if ( !IsFrozen() )
        MSWUpdateDropDownHeight();

    InvalidateBestSize();
}

// ----------------------------------------------------------------------------
// selection
// ----------------------------------------------------------------------------

int wxChoice::GetSelection() const
{
    // if m_lastAcceptedSelection is set, it means that the dropdown is
    // currently shown and that we want to use the last "permanent" selection
    // instead of whatever is under the mouse pointer currently
    //
    // otherwise, get the selection from the control
    return m_lastAcceptedSelection == wxID_NONE ? GetCurrentSelection()
                                                : m_lastAcceptedSelection;
}

int wxChoice::GetCurrentSelection() const
{
    return (int)SendMessage(GetHwnd(), CB_GETCURSEL, 0, 0);
}

void wxChoice::SetSelection(int n)
{
    SendMessage(GetHwnd(), CB_SETCURSEL, n, 0);
}

// ----------------------------------------------------------------------------
// string list functions
// ----------------------------------------------------------------------------

unsigned int wxChoice::GetCount() const
{
    return (unsigned int)SendMessage(GetHwnd(), CB_GETCOUNT, 0, 0);
}

int wxChoice::FindString(const wxString& s, bool bCase) const
{
#if defined(__WATCOMC__) && defined(__WIN386__)
    // For some reason, Watcom in WIN386 mode crashes in the CB_FINDSTRINGEXACT message.
    // wxChoice::Do it the long way instead.
    unsigned int count = GetCount();
    for ( unsigned int i = 0; i < count; i++ )
    {
        // as CB_FINDSTRINGEXACT is case insensitive, be case insensitive too
        if (GetString(i).IsSameAs(s, bCase))
            return i;
    }

    return wxNOT_FOUND;
#else // !Watcom
   //TODO:  Evidently some MSW versions (all?) don't like empty strings
   //passed to SendMessage, so we have to do it ourselves in that case
   if ( s.empty() )
   {
       unsigned int count = GetCount();
       for ( unsigned int i = 0; i < count; i++ )
       {
         if (GetString(i).empty())
             return i;
       }

       return wxNOT_FOUND;
   }
   else if (bCase)
   {
       // back to base class search for not native search type
       return wxItemContainerImmutable::FindString( s, bCase );
   }
   else
   {
       int pos = (int)SendMessage(GetHwnd(), CB_FINDSTRINGEXACT,
                                  (WPARAM)-1, (LPARAM)s.wx_str());

       return pos == LB_ERR ? wxNOT_FOUND : pos;
   }
#endif // Watcom/!Watcom
}

void wxChoice::SetString(unsigned int n, const wxString& s)
{
    wxCHECK_RET( IsValid(n), wxT("invalid item index in wxChoice::SetString") );

    // we have to delete and add back the string as there is no way to change a
    // string in place

    // we need to preserve the client data manually
    void *oldData = NULL;
    wxClientData *oldObjData = NULL;
    if ( HasClientUntypedData() )
        oldData = GetClientData(n);
    else if ( HasClientObjectData() )
        oldObjData = GetClientObject(n);

    ::SendMessage(GetHwnd(), CB_DELETESTRING, n, 0);
    ::SendMessage(GetHwnd(), CB_INSERTSTRING, n, (LPARAM)s.wx_str() );

    // restore the client data
    if ( oldData )
        SetClientData(n, oldData);
    else if ( oldObjData )
        SetClientObject(n, oldObjData);

    InvalidateBestSize();
}

wxString wxChoice::GetString(unsigned int n) const
{
    int len = (int)::SendMessage(GetHwnd(), CB_GETLBTEXTLEN, n, 0);

    wxString str;
    if ( len != CB_ERR && len > 0 )
    {
        if ( ::SendMessage
               (
                GetHwnd(),
                CB_GETLBTEXT,
                n,
                (LPARAM)(wxChar *)wxStringBuffer(str, len)
               ) == CB_ERR )
        {
            wxLogLastError(wxT("SendMessage(CB_GETLBTEXT)"));
        }
    }

    return str;
}

// ----------------------------------------------------------------------------
// client data
// ----------------------------------------------------------------------------

void wxChoice::DoSetItemClientData(unsigned int n, void* clientData)
{
    if ( ::SendMessage(GetHwnd(), CB_SETITEMDATA,
                       n, (LPARAM)clientData) == CB_ERR )
    {
        wxLogLastError(wxT("CB_SETITEMDATA"));
    }
}

void* wxChoice::DoGetItemClientData(unsigned int n) const
{
    LPARAM rc = SendMessage(GetHwnd(), CB_GETITEMDATA, n, 0);
    if ( rc == CB_ERR )
    {
        wxLogLastError(wxT("CB_GETITEMDATA"));

        // unfortunately, there is no way to return an error code to the user
        rc = (LPARAM) NULL;
    }

    return (void *)rc;
}

// ----------------------------------------------------------------------------
// wxMSW-specific geometry management
// ----------------------------------------------------------------------------

namespace
{

// there is a difference between the height passed to CB_SETITEMHEIGHT and the
// real height of the combobox; it is probably not constant for all Windows
// versions/settings but right now I don't know how to find what it is so it is
// temporarily hardcoded to its value under XP systems with normal fonts sizes
const int COMBO_HEIGHT_ADJ = 6;

} // anonymous namespace

void wxChoice::MSWUpdateVisibleHeight()
{
    if ( m_heightOwn != wxDefaultCoord )
    {
        ::SendMessage(GetHwnd(), CB_SETITEMHEIGHT,
                      (WPARAM)-1, m_heightOwn - COMBO_HEIGHT_ADJ);
    }
}

#if wxUSE_DEFERRED_SIZING
void wxChoice::MSWEndDeferWindowPos()
{
    // we can only set the height of the choice itself now as it is reset to
    // default every time the control is resized
    MSWUpdateVisibleHeight();

    wxChoiceBase::MSWEndDeferWindowPos();
}
#endif // wxUSE_DEFERRED_SIZING

void wxChoice::MSWUpdateDropDownHeight()
{
    // be careful to not change the width here
    DoSetSize(wxDefaultCoord, wxDefaultCoord, wxDefaultCoord, GetSize().y,
              wxSIZE_USE_EXISTING);
}

void wxChoice::DoMoveWindow(int x, int y, int width, int height)
{
    // here is why this is necessary: if the width is negative, the combobox
    // window proc makes the window of the size width*height instead of
    // interpreting height in the usual manner (meaning the height of the drop
    // down list - usually the height specified in the call to MoveWindow()
    // will not change the height of combo box per se)
    //
    // this behaviour is not documented anywhere, but this is just how it is
    // here (NT 4.4) and, anyhow, the check shouldn't hurt - however without
    // the check, constraints/sizers using combos may break the height
    // constraint will have not at all the same value as expected
    if ( width < 0 )
        return;

    wxControl::DoMoveWindow(x, y, width, height);
}

void wxChoice::DoGetSize(int *w, int *h) const
{
    wxControl::DoGetSize(w, h);

    // this is weird: sometimes, the height returned by Windows is clearly the
    // total height of the control including the drop down list -- but only
    // sometimes, and sometimes it isn't so work around this here by using our
    // own stored value if we have it
    if ( h && m_heightOwn != wxDefaultCoord )
        *h = m_heightOwn;
}

void wxChoice::DoSetSize(int x, int y,
                         int width, int height,
                         int sizeFlags)
{
    const int heightBest = GetBestSize().y;

    // we need the real height below so get the current one if it's not given
    if ( height == wxDefaultCoord )
    {
        // height not specified, use the same as before
        DoGetSize(NULL, &height);
    }
    else if ( height == heightBest )
    {
        // we don't need to manually manage our height, let the system use the
        // default one
        m_heightOwn = wxDefaultCoord;
    }
    else // non-default height specified
    {
        // set our new own height but be careful not to make it too big: the
        // native control apparently stores it as a single byte and so setting
        // own height to 256 pixels results in default height being used (255
        // is still ok)
        m_heightOwn = height;

        if ( m_heightOwn > UCHAR_MAX )
            m_heightOwn = UCHAR_MAX;
        // nor too small: see MSWUpdateVisibleHeight()
        else if ( m_heightOwn < COMBO_HEIGHT_ADJ )
            m_heightOwn = COMBO_HEIGHT_ADJ;
    }


    // the height which we must pass to Windows should be the total height of
    // the control including the drop down list while the height given to us
    // is, of course, just the height of the permanently visible part of it so
    // add the drop down height to it

    // don't make the drop down list too tall, arbitrarily limit it to 30
    // items max and also don't make it too small if it's currently empty
    size_t nItems = GetCount();
    if (!HasFlag(wxCB_SIMPLE))
    {
        if ( !nItems )
            nItems = 9;
        else if ( nItems > 30 )
            nItems = 30;
    }

    const int hItem = SendMessage(GetHwnd(), CB_GETITEMHEIGHT, 0, 0);
    int heightWithItems = 0;
    if (!HasFlag(wxCB_SIMPLE))
        heightWithItems = height + hItem*nItems;
    else
        heightWithItems = SetHeightSimpleComboBox(nItems);


    // do resize the native control
    wxControl::DoSetSize(x, y, width, heightWithItems, sizeFlags);


    // make the control itself of the requested height: notice that this
    // must be done after changing its size or it has no effect (apparently
    // the height is reset to default during the control layout) and that it's
    // useless to do it when using the deferred sizing -- in this case it
    // will be done from MSWEndDeferWindowPos()
#if wxUSE_DEFERRED_SIZING
    if ( m_pendingSize == wxDefaultSize )
    {
        // not using deferred sizing, update it immediately
        MSWUpdateVisibleHeight();
    }
    else // in the middle of deferred sizing
    {
        // we need to report the size of the visible part of the control back
        // in GetSize() and not height stored by DoSetSize() in m_pendingSize
        m_pendingSize = wxSize(width, height);
    }
#else // !wxUSE_DEFERRED_SIZING
    // always update the visible height immediately
    MSWUpdateVisibleHeight();
#endif // wxUSE_DEFERRED_SIZING
}

wxSize wxChoice::DoGetBestSize() const
{
    // find the widest string
    int wChoice = 0;
    int hChoice;
    const unsigned int nItems = GetCount();
    for ( unsigned int i = 0; i < nItems; i++ )
    {
        int wLine;
        GetTextExtent(GetString(i), &wLine, NULL);
        if ( wLine > wChoice )
            wChoice = wLine;
    }

    // give it some reasonable default value if there are no strings in the
    // list
    if ( wChoice == 0 )
        wChoice = 100;

    // the combobox should be slightly larger than the widest string
    wChoice += 5*GetCharWidth();
    if( HasFlag( wxCB_SIMPLE ) )
    {
        hChoice = SetHeightSimpleComboBox( nItems );
    }
    else
        hChoice = EDIT_HEIGHT_FROM_CHAR_HEIGHT(GetCharHeight());

    wxSize best(wChoice, hChoice);
    CacheBestSize(best);
    return best;
}

int wxChoice::SetHeightSimpleComboBox(int nItems) const
{
    int cx, cy;
    wxGetCharSize( GetHWND(), &cx, &cy, GetFont() );
    int hItem = SendMessage(GetHwnd(), CB_GETITEMHEIGHT, (WPARAM)-1, 0);
    return EDIT_HEIGHT_FROM_CHAR_HEIGHT( cy ) * wxMin( wxMax( nItems, 3 ), 6 ) + hItem - 1;
}

// ----------------------------------------------------------------------------
// MSW message handlers
// ----------------------------------------------------------------------------

WXLRESULT wxChoice::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    switch ( nMsg )
    {
        case WM_LBUTTONUP:
            {
                int x = (int)LOWORD(lParam);
                int y = (int)HIWORD(lParam);

                // Ok, this is truly weird, but if a panel with a wxChoice
                // loses the focus, then you get a *fake* WM_LBUTTONUP message
                // with x = 65535 and y = 65535. Filter out this nonsense.
                //
                // VZ: I'd like to know how to reproduce this please...
                if ( x == 65535 && y == 65535 )
                    return 0;
            }
            break;

            // we have to handle both: one for the normal case and the other
            // for readonly
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSTATIC:
            {
                WXHDC hdc;
                WXHWND hwnd;
                UnpackCtlColor(wParam, lParam, &hdc, &hwnd);

                WXHBRUSH hbr = MSWControlColor((WXHDC)hdc, hwnd);
                if ( hbr )
                    return (WXLRESULT)hbr;
                //else: fall through to default window proc
            }
    }

    return wxWindow::MSWWindowProc(nMsg, wParam, lParam);
}

bool wxChoice::MSWCommand(WXUINT param, WXWORD WXUNUSED(id))
{
    /*
        The native control provides a great variety in the events it sends in
        the different selection scenarios (undoubtedly for greater amusement of
        the programmers using it). For the reference, here are the cases when
        the final selection is accepted (things are quite interesting when it
        is cancelled too):

        A. Selecting with just the arrows without opening the dropdown:
            1. CBN_SELENDOK
            2. CBN_SELCHANGE

        B. Opening dropdown with F4 and selecting with arrows:
            1. CBN_DROPDOWN
            2. many CBN_SELCHANGE while changing selection in the list
            3. CBN_SELENDOK
            4. CBN_CLOSEUP

        C. Selecting with the mouse:
            1. CBN_DROPDOWN
            -- no intermediate CBN_SELCHANGEs --
            2. CBN_SELENDOK
            3. CBN_CLOSEUP
            4. CBN_SELCHANGE

        Admire the different order of messages in all of those cases, it must
        surely have taken a lot of effort to Microsoft developers to achieve
        such originality.
     */
    switch ( param )
    {
        case CBN_DROPDOWN:
            // we use this value both because we don't want to track selection
            // using CB_GETCURSEL while the dropdown is opened and because we
            // need to reset the selection back to it if it's eventually
            // cancelled by user
            m_lastAcceptedSelection = GetCurrentSelection();
            break;

        case CBN_CLOSEUP:
            // if the selection was accepted by the user, it should have been
            // reset to wxID_NONE by CBN_SELENDOK, otherwise the selection was
            // cancelled and we must restore the old one
            if ( m_lastAcceptedSelection != wxID_NONE )
            {
                SetSelection(m_lastAcceptedSelection);
                m_lastAcceptedSelection = wxID_NONE;
            }
            break;

        case CBN_SELENDOK:
            // reset it to prevent CBN_CLOSEUP from undoing the selection (it's
            // ok to reset it now as GetCurrentSelection() will now return the
            // same thing anyhow)
            m_lastAcceptedSelection = wxID_NONE;

            {
                const int n = GetSelection();

                wxCommandEvent event(wxEVT_COMMAND_CHOICE_SELECTED, m_windowId);
                event.SetInt(n);
                event.SetEventObject(this);

                if ( n > -1 )
                {
                    event.SetString(GetStringSelection());
                    InitCommandEventWithItems(event, n);
                }

                ProcessCommand(event);
            }
            break;

        // don't handle CBN_SELENDCANCEL: just leave m_lastAcceptedSelection
        // valid and the selection will be undone in CBN_CLOSEUP above

        // don't handle CBN_SELCHANGE neither, we don't want to generate events
        // while the dropdown is opened -- but do add it if we ever need this

        default:
            return false;
    }

    return true;
}

WXHBRUSH wxChoice::MSWControlColor(WXHDC hDC, WXHWND hWnd)
{
    if ( !IsEnabled() )
        return MSWControlColorDisabled(hDC);

    return wxChoiceBase::MSWControlColor(hDC, hWnd);
}

#endif // wxUSE_CHOICE && !(__SMARTPHONE__ && __WXWINCE__)
