///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/textentry.cpp
// Purpose:     wxTextEntry implementation for wxMSW
// Author:      Vadim Zeitlin
// Created:     2007-09-26
// RCS-ID:      $Id: textentry.cpp 65057 2010-07-23 23:32:46Z VZ $
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/arrstr.h"
    #include "wx/string.h"
#endif // WX_PRECOMP

#if wxUSE_TEXTCTRL || wxUSE_COMBOBOX

#include "wx/textentry.h"
#include "wx/dynlib.h"

#include "wx/msw/private.h"

#if wxUSE_UXTHEME
    #include "wx/msw/uxtheme.h"
#endif

#define GetEditHwnd() ((HWND)(GetEditHWND()))

// ----------------------------------------------------------------------------
// wxIEnumString implements IEnumString interface
// ----------------------------------------------------------------------------

// standard VC6 SDK (WINVER == 0x0400) does not know about IAutoComplete
#if wxUSE_OLE && (WINVER >= 0x0500)
    #define HAS_AUTOCOMPLETE
#endif

#ifdef HAS_AUTOCOMPLETE

#include "wx/msw/ole/oleutils.h"
#include <shldisp.h>

#if defined(__MINGW32__) || defined (__WATCOMC__) || defined(__CYGWIN__)
    // needed for IID_IAutoComplete, IID_IAutoComplete2 and ACO_AUTOSUGGEST
    #include <shlguid.h>
#endif

#ifndef ACO_UPDOWNKEYDROPSLIST
    #define ACO_UPDOWNKEYDROPSLIST 0x20
#endif

#ifndef SHACF_FILESYS_ONLY
    #define SHACF_FILESYS_ONLY 0x00000010
#endif

DEFINE_GUID(CLSID_AutoComplete,
    0x00bb2763, 0x6a77, 0x11d0, 0xa5, 0x35, 0x00, 0xc0, 0x4f, 0xd7, 0xd0, 0x62);

class wxIEnumString : public IEnumString
{
public:
    wxIEnumString(const wxArrayString& strings) : m_strings(strings)
    {
        m_index = 0;
    }

    void ChangeStrings(const wxArrayString& strings)
    {
        m_strings = strings;
        Reset();
    }

    DECLARE_IUNKNOWN_METHODS;

    virtual HRESULT STDMETHODCALLTYPE Next(ULONG celt,
                                           LPOLESTR *rgelt,
                                           ULONG *pceltFetched)
    {
        if ( !rgelt || (!pceltFetched && celt > 1) )
            return E_POINTER;

        ULONG pceltFetchedDummy;
        if ( !pceltFetched )
            pceltFetched = &pceltFetchedDummy;

        *pceltFetched = 0;

        for ( const unsigned count = m_strings.size(); celt--; ++m_index )
        {
            if ( m_index == count )
                return S_FALSE;

            const wxWX2WCbuf wcbuf = m_strings[m_index].wc_str();
            const size_t size = (wcslen(wcbuf) + 1)*sizeof(wchar_t);
            void *olestr = CoTaskMemAlloc(size);
            if ( !olestr )
                return E_OUTOFMEMORY;

            memcpy(olestr, wcbuf, size);

            *rgelt++ = static_cast<LPOLESTR>(olestr);

            ++(*pceltFetched);
        }

        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt)
    {
        m_index += celt;
        if ( m_index > m_strings.size() )
        {
            m_index = m_strings.size();
            return S_FALSE;
        }

        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Reset()
    {
        m_index = 0;

        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Clone(IEnumString **ppEnum)
    {
        if ( !ppEnum )
            return E_POINTER;

        wxIEnumString *e = new wxIEnumString(m_strings);
        e->m_index = m_index;

        e->AddRef();
        *ppEnum = e;

        return S_OK;
    }

private:
    // dtor doesn't have to be virtual as we're only ever deleted from our own
    // Release() and are not meant to be derived form anyhow, but making it
    // virtual silences gcc warnings; making it private makes it impossible to
    // (mistakenly) delete us directly instead of calling Release()
    virtual ~wxIEnumString() { }


    wxArrayString m_strings;
    unsigned m_index;

    wxDECLARE_NO_COPY_CLASS(wxIEnumString);
};

BEGIN_IID_TABLE(wxIEnumString)
    ADD_IID(Unknown)
    ADD_IID(EnumString)
END_IID_TABLE;

IMPLEMENT_IUNKNOWN_METHODS(wxIEnumString)

#endif // HAS_AUTOCOMPLETE

// ============================================================================
// wxTextEntry implementation
// ============================================================================

// ----------------------------------------------------------------------------
// operations on text
// ----------------------------------------------------------------------------

void wxTextEntry::WriteText(const wxString& text)
{
    ::SendMessage(GetEditHwnd(), EM_REPLACESEL, 0, (LPARAM)text.wx_str());
}

wxString wxTextEntry::DoGetValue() const
{
    return wxGetWindowText(GetEditHWND());
}

void wxTextEntry::Remove(long from, long to)
{
    DoSetSelection(from, to, SetSel_NoScroll);
    WriteText(wxString());
}

// ----------------------------------------------------------------------------
// clipboard operations
// ----------------------------------------------------------------------------

void wxTextEntry::Copy()
{
    ::SendMessage(GetEditHwnd(), WM_COPY, 0, 0);
}

void wxTextEntry::Cut()
{
    ::SendMessage(GetEditHwnd(), WM_CUT, 0, 0);
}

void wxTextEntry::Paste()
{
    ::SendMessage(GetEditHwnd(), WM_PASTE, 0, 0);
}

// ----------------------------------------------------------------------------
// undo/redo
// ----------------------------------------------------------------------------

void wxTextEntry::Undo()
{
    ::SendMessage(GetEditHwnd(), EM_UNDO, 0, 0);
}

void wxTextEntry::Redo()
{
    // same as Undo, since Undo undoes the undo
    Undo();
    return;
}

bool wxTextEntry::CanUndo() const
{
    return ::SendMessage(GetEditHwnd(), EM_CANUNDO, 0, 0) != 0;
}

bool wxTextEntry::CanRedo() const
{
    // see comment in Redo()
    return CanUndo();
}

// ----------------------------------------------------------------------------
// insertion point and selection
// ----------------------------------------------------------------------------

void wxTextEntry::SetInsertionPoint(long pos)
{
    // calling DoSetSelection(-1, -1) would select everything which is not what
    // we want here
    if ( pos == -1 )
        pos = GetLastPosition();

    // be careful to call DoSetSelection() which is overridden in wxTextCtrl
    // and not just SetSelection() here
    DoSetSelection(pos, pos);
}

long wxTextEntry::GetInsertionPoint() const
{
    long from;
    GetSelection(&from, NULL);
    return from;
}

long wxTextEntry::GetLastPosition() const
{
    return ::SendMessage(GetEditHwnd(), EM_LINELENGTH, 0, 0);
}

void wxTextEntry::DoSetSelection(long from, long to, int WXUNUSED(flags))
{
    // if from and to are both -1, it means (in wxWidgets) that all text should
    // be selected, translate this into Windows convention
    if ( (from == -1) && (to == -1) )
    {
        from = 0;
    }

    ::SendMessage(GetEditHwnd(), EM_SETSEL, from, to);
}

void wxTextEntry::GetSelection(long *from, long *to) const
{
    DWORD dwStart, dwEnd;
    ::SendMessage(GetEditHwnd(), EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);

    if ( from )
        *from = dwStart;
    if ( to )
        *to = dwEnd;
}

// ----------------------------------------------------------------------------
// auto-completion
// ----------------------------------------------------------------------------

#if wxUSE_OLE
bool wxTextEntry::AutoCompleteFileNames()
{
#ifdef HAS_AUTOCOMPLETE
    typedef HRESULT (WINAPI *SHAutoComplete_t)(HWND, DWORD);
    static SHAutoComplete_t s_pfnSHAutoComplete = (SHAutoComplete_t)-1;
    static wxDynamicLibrary s_dllShlwapi;
    if ( s_pfnSHAutoComplete == (SHAutoComplete_t)-1 )
    {
        if ( !s_dllShlwapi.Load(wxT("shlwapi.dll"), wxDL_VERBATIM | wxDL_QUIET) )
        {
            s_pfnSHAutoComplete = NULL;
        }
        else
        {
            wxDL_INIT_FUNC(s_pfn, SHAutoComplete, s_dllShlwapi);
        }
    }

    if ( !s_pfnSHAutoComplete )
        return false;

    HRESULT hr = (*s_pfnSHAutoComplete)(GetEditHwnd(), SHACF_FILESYS_ONLY);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxT("SHAutoComplete()"), hr);

        return false;
    }
    return true;
#else // !HAS_AUTOCOMPLETE
    return false;
#endif // HAS_AUTOCOMPLETE/!HAS_AUTOCOMPLETE
}

bool wxTextEntry::AutoComplete(const wxArrayString& choices)
{
#ifdef HAS_AUTOCOMPLETE
    // if we had an old enumerator we must reuse it as IAutoComplete doesn't
    // free it if we call Init() again (see #10968) -- and it's also simpler
    if ( m_enumStrings )
    {
        m_enumStrings->ChangeStrings(choices);
        return true;
    }

    // create an object exposing IAutoComplete interface (don't go for
    // IAutoComplete2 immediately as, presumably, it might be not available on
    // older systems as otherwise why do we have both -- although in practice I
    // don't know when can this happen)
    IAutoComplete *pAutoComplete = NULL;
    HRESULT hr = CoCreateInstance
                 (
                    CLSID_AutoComplete,
                    NULL,
                    CLSCTX_INPROC_SERVER,
                    IID_IAutoComplete,
                    reinterpret_cast<void **>(&pAutoComplete)
                 );
    if ( FAILED(hr) )
    {
        wxLogApiError(wxT("CoCreateInstance(CLSID_AutoComplete)"), hr);
        return false;
    }

    // associate it with our strings
    m_enumStrings = new wxIEnumString(choices);
    m_enumStrings->AddRef();
    hr = pAutoComplete->Init(GetEditHwnd(), m_enumStrings, NULL, NULL);
    m_enumStrings->Release();
    if ( FAILED(hr) )
    {
        wxLogApiError(wxT("IAutoComplete::Init"), hr);
        return false;
    }

    // if IAutoComplete2 is available, set more user-friendly options
    IAutoComplete2 *pAutoComplete2 = NULL;
    hr = pAutoComplete->QueryInterface
                        (
                           IID_IAutoComplete2,
                           reinterpret_cast<void **>(&pAutoComplete2)
                        );
    if ( SUCCEEDED(hr) )
    {
        pAutoComplete2->SetOptions(ACO_AUTOSUGGEST | ACO_UPDOWNKEYDROPSLIST);
        pAutoComplete2->Release();
    }

    // the docs are unclear about when can we release it but it seems safe to
    // do it immediately, presumably the edit control itself keeps a reference
    // to the auto completer object
    pAutoComplete->Release();
    return true;
#else // !HAS_AUTOCOMPLETE
    wxUnusedVar(choices);

    return false;
#endif // HAS_AUTOCOMPLETE/!HAS_AUTOCOMPLETE
}
#endif // wxUSE_OLE

// ----------------------------------------------------------------------------
// editable state
// ----------------------------------------------------------------------------

bool wxTextEntry::IsEditable() const
{
    return !(::GetWindowLong(GetEditHwnd(), GWL_STYLE) & ES_READONLY);
}

void wxTextEntry::SetEditable(bool editable)
{
    ::SendMessage(GetEditHwnd(), EM_SETREADONLY, !editable, 0);
}

// ----------------------------------------------------------------------------
// max length
// ----------------------------------------------------------------------------

void wxTextEntry::SetMaxLength(unsigned long len)
{
    if ( len >= 0xffff )
    {
        // this will set it to a platform-dependent maximum (much more
        // than 64Kb under NT)
        len = 0;
    }

    ::SendMessage(GetEditHwnd(), EM_LIMITTEXT, len, 0);
}

// ----------------------------------------------------------------------------
// hints
// ----------------------------------------------------------------------------

#if wxUSE_UXTHEME

#ifndef EM_SETCUEBANNER
    #define EM_SETCUEBANNER 0x1501
    #define EM_GETCUEBANNER 0x1502
#endif

bool wxTextEntry::SetHint(const wxString& hint)
{
    if ( wxUxThemeEngine::GetIfActive() )
    {
        // notice that this message always works with Unicode strings
        //
        // we always use TRUE for wParam to show the hint even when the window
        // has focus, otherwise there would be no way to show the hint for the
        // initially focused window
        if ( ::SendMessage(GetEditHwnd(), EM_SETCUEBANNER,
                             TRUE, (LPARAM)(const wchar_t *)hint.wc_str()) )
            return true;
    }

    return wxTextEntryBase::SetHint(hint);
}

wxString wxTextEntry::GetHint() const
{
    if ( wxUxThemeEngine::GetIfActive() )
    {
        wchar_t buf[256];
        if ( ::SendMessage(GetEditHwnd(), EM_GETCUEBANNER,
                            (WPARAM)buf, WXSIZEOF(buf)) )
            return wxString(buf);
    }

    return wxTextEntryBase::GetHint();
}


#endif // wxUSE_UXTHEME

// ----------------------------------------------------------------------------
// margins support
// ----------------------------------------------------------------------------

bool wxTextEntry::DoSetMargins(const wxPoint& margins)
{
#if !defined(__WXWINCE__)
    bool res = true;

    if ( margins.x != -1 )
    {
        // left margin
        ::SendMessage(GetEditHwnd(), EM_SETMARGINS,
                      EC_LEFTMARGIN, MAKELONG(margins.x, 0));
    }

    if ( margins.y != -1 )
    {
        res = false;
    }

    return res;
#else
    return false;
#endif
}

wxPoint wxTextEntry::DoGetMargins() const
{
#if !defined(__WXWINCE__)
    LRESULT lResult = ::SendMessage(GetEditHwnd(), EM_GETMARGINS,
                                    0, 0);
    int left = LOWORD(lResult);
    int top = -1;
    return wxPoint(left, top);
#else
    return wxPoint(-1, -1);
#endif
}

#endif // wxUSE_TEXTCTRL || wxUSE_COMBOBOX
