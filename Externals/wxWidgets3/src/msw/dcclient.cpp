/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/dcclient.cpp
// Purpose:     wxClientDC class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: dcclient.cpp 63229 2010-01-23 13:22:25Z VZ $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/dcclient.h"
#include "wx/msw/dcclient.h"

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/window.h"
#endif

#include "wx/msw/private.h"

// ----------------------------------------------------------------------------
// array/list types
// ----------------------------------------------------------------------------

struct WXDLLEXPORT wxPaintDCInfo
{
    wxPaintDCInfo(wxWindow *win, wxPaintDCImpl *dc)
    {
        hwnd = win->GetHWND();
        hdc = dc->GetHDC();
        count = 1;
    }

    WXHWND    hwnd;       // window for this DC
    WXHDC     hdc;        // the DC handle
    size_t    count;      // usage count
};

#include "wx/arrimpl.cpp"

WX_DEFINE_OBJARRAY(wxArrayDCInfo)

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// global variables
// ----------------------------------------------------------------------------

static PAINTSTRUCT g_paintStruct;

#ifdef wxHAS_PAINT_DEBUG
    // a global variable which we check to verify that wxPaintDC are only
    // created in response to WM_PAINT message - doing this from elsewhere is a
    // common programming error among wxWidgets programmers and might lead to
    // very subtle and difficult to debug refresh/repaint bugs.
    int g_isPainting = 0;
#endif // wxHAS_PAINT_DEBUG

// ===========================================================================
// implementation
// ===========================================================================

// ----------------------------------------------------------------------------
// wxMSWWindowDCImpl
// ----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxWindowDCImpl, wxMSWDCImpl)

wxWindowDCImpl::wxWindowDCImpl( wxDC *owner ) :
   wxMSWDCImpl( owner )
{
}

wxWindowDCImpl::wxWindowDCImpl( wxDC *owner, wxWindow *window ) :
   wxMSWDCImpl( owner )
{
    wxCHECK_RET( window, wxT("invalid window in wxWindowDCImpl") );

    m_window = window;
    m_hDC = (WXHDC) ::GetWindowDC(GetHwndOf(m_window));

    // m_bOwnsDC was already set to false in the base class ctor, so the DC
    // will be released (and not deleted) in ~wxDC
    InitDC();
}

void wxWindowDCImpl::InitDC()
{
    // the background mode is only used for text background and is set in
    // DrawText() to OPAQUE as required, otherwise always TRANSPARENT,
    ::SetBkMode(GetHdc(), TRANSPARENT);

    // since we are a window dc we need to grab the palette from the window
#if wxUSE_PALETTE
    InitializePalette();
#endif
}

void wxWindowDCImpl::DoGetSize(int *width, int *height) const
{
    wxCHECK_RET( m_window, wxT("wxWindowDCImpl without a window?") );

    m_window->GetSize(width, height);
}

// ----------------------------------------------------------------------------
// wxClientDCImpl
// ----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxClientDCImpl, wxWindowDCImpl)

wxClientDCImpl::wxClientDCImpl( wxDC *owner ) :
   wxWindowDCImpl( owner )
{
}

wxClientDCImpl::wxClientDCImpl( wxDC *owner, wxWindow *window ) :
   wxWindowDCImpl( owner )
{
    wxCHECK_RET( window, wxT("invalid window in wxClientDCImpl") );

    m_window = window;
    m_hDC = (WXHDC)::GetDC(GetHwndOf(window));

    // m_bOwnsDC was already set to false in the base class ctor, so the DC
    // will be released (and not deleted) in ~wxDC

    InitDC();
}

void wxClientDCImpl::InitDC()
{
    wxWindowDCImpl::InitDC();

    // in wxUniv build we must manually do some DC adjustments usually
    // performed by Windows for us
    //
    // we also need to take the menu/toolbar manually into account under
    // Windows CE because they're just another control there, not anything
    // special as usually under Windows
#if defined(__WXUNIVERSAL__) || defined(__WXWINCE__)
    wxPoint ptOrigin = m_window->GetClientAreaOrigin();
    if ( ptOrigin.x || ptOrigin.y )
    {
        // no need to shift DC origin if shift is null
        SetDeviceOrigin(ptOrigin.x, ptOrigin.y);
    }

    // clip the DC to avoid overwriting the non client area
    wxSize size = m_window->GetClientSize();
    DoSetClippingRegion(0, 0, size.x, size.y);
#endif // __WXUNIVERSAL__ || __WXWINCE__
}

wxClientDCImpl::~wxClientDCImpl()
{
}

void wxClientDCImpl::DoGetSize(int *width, int *height) const
{
    wxCHECK_RET( m_window, wxT("wxClientDCImpl without a window?") );

    m_window->GetClientSize(width, height);
}

// ----------------------------------------------------------------------------
// wxPaintDCImpl
// ----------------------------------------------------------------------------

// VZ: initial implementation (by JACS) only remembered the last wxPaintDC
//     created and tried to reuse it - this was supposed to take care of a
//     situation when a derived class OnPaint() calls base class OnPaint()
//     because in this case ::BeginPaint() shouldn't be called second time.
//
//     I'm not sure how useful this is, however we must remember the HWND
//     associated with the last HDC as well - otherwise we may (and will!) try
//     to reuse the HDC for another HWND which is a nice recipe for disaster.
//
//     So we store a list of windows for which we already have the DC and not
//     just one single hDC. This seems to work, but I'm really not sure about
//     the usefullness of the whole idea - IMHO it's much better to not call
//     base class OnPaint() at all, or, if we really want to allow it, add a
//     "wxPaintDC *" parameter to wxPaintEvent which should be used if it's
//     !NULL instead of creating a new DC.

IMPLEMENT_ABSTRACT_CLASS(wxPaintDCImpl, wxClientDCImpl)

wxArrayDCInfo wxPaintDCImpl::ms_cache;

wxPaintDCImpl::wxPaintDCImpl( wxDC *owner ) :
   wxClientDCImpl( owner )
{
}

wxPaintDCImpl::wxPaintDCImpl( wxDC *owner, wxWindow *window ) :
   wxClientDCImpl( owner )
{
    wxCHECK_RET( window, wxT("NULL canvas in wxPaintDCImpl ctor") );

#ifdef wxHAS_PAINT_DEBUG
    if ( g_isPainting <= 0 )
    {
        wxFAIL_MSG( wxT("wxPaintDCImpl may be created only in EVT_PAINT handler!") );

        return;
    }
#endif // wxHAS_PAINT_DEBUG

    m_window = window;

    // do we have a DC for this window in the cache?
    wxPaintDCInfo *info = FindInCache();
    if ( info )
    {
        m_hDC = info->hdc;
        info->count++;
    }
    else // not in cache, create a new one
    {
        // see comments in src/msw/window.cpp where this is defined
        extern bool wxDidCreatePaintDC;

        wxDidCreatePaintDC = true;

        m_hDC = (WXHDC)::BeginPaint(GetHwndOf(m_window), &g_paintStruct);
        if (m_hDC)
            ms_cache.Add(new wxPaintDCInfo(m_window, this));
    }

    // Note: at this point m_hDC can be NULL under MicroWindows, when dragging.
    if (!GetHDC())
        return;

    // (re)set the DC parameters.
    InitDC();

    // the HDC can have a clipping box (which we didn't set), make sure our
    // DoGetClippingBox() checks for it
    m_clipping = true;
}

wxPaintDCImpl::~wxPaintDCImpl()
{
    if ( m_hDC )
    {
        SelectOldObjects(m_hDC);

        size_t index;
        wxPaintDCInfo *info = FindInCache(&index);

        wxCHECK_RET( info, wxT("existing DC should have a cache entry") );

        if ( --info->count == 0 )
        {
            ::EndPaint(GetHwndOf(m_window), &g_paintStruct);

            ms_cache.RemoveAt(index);

            // Reduce the number of bogus reports of non-freed memory
            // at app exit
            if (ms_cache.IsEmpty())
                ms_cache.Clear();
        }
        //else: cached DC entry is still in use

        // prevent the base class dtor from ReleaseDC()ing it again
        m_hDC = 0;
    }
}

wxPaintDCInfo *wxPaintDCImpl::FindInCache(size_t *index) const
{
    wxPaintDCInfo *info = NULL;
    size_t nCache = ms_cache.GetCount();
    for ( size_t n = 0; n < nCache; n++ )
    {
        wxPaintDCInfo *info1 = &ms_cache[n];
        if ( info1->hwnd == m_window->GetHWND() )
        {
            info = info1;
            if ( index )
                *index = n;
            break;
        }
    }

    return info;
}

// find the entry for this DC in the cache (keyed by the window)
WXHDC wxPaintDCImpl::FindDCInCache(wxWindow* win)
{
    size_t nCache = ms_cache.GetCount();
    for ( size_t n = 0; n < nCache; n++ )
    {
        wxPaintDCInfo *info = &ms_cache[n];
        if ( info->hwnd == win->GetHWND() )
        {
            return info->hdc;
        }
    }
    return 0;
}

/*
 * wxPaintDCEx
 */

// TODO: don't duplicate wxPaintDCImpl code here!!

class wxPaintDCExImpl: public wxPaintDCImpl
{
public:
    wxPaintDCExImpl( wxDC *owner, wxWindow *window, WXHDC dc );
    ~wxPaintDCExImpl();

    int m_saveState;
};


IMPLEMENT_ABSTRACT_CLASS(wxPaintDCEx,wxPaintDC)

wxPaintDCEx::wxPaintDCEx(wxWindow *window, WXHDC dc)
           : wxPaintDC(new wxPaintDCExImpl(this, window, dc))
{
}

wxPaintDCExImpl::wxPaintDCExImpl(wxDC *owner, wxWindow *window, WXHDC dc)
               : wxPaintDCImpl( owner )
{
        wxCHECK_RET( dc, wxT("wxPaintDCEx requires an existing device context") );

        m_saveState = 0;

        m_window = window;

        wxPaintDCInfo *info = FindInCache();
        if ( info )
        {
           m_hDC = info->hdc;
           info->count++;
        }
        else // not in cache, create a new one
        {
                m_hDC = dc;
                ms_cache.Add(new wxPaintDCInfo(m_window, this));
                m_saveState = SaveDC((HDC) dc);
        }
}

wxPaintDCExImpl::~wxPaintDCExImpl()
{
        size_t index;
        wxPaintDCInfo *info = FindInCache(&index);

        wxCHECK_RET( info, wxT("existing DC should have a cache entry") );

        if ( --info->count == 0 )
        {
                RestoreDC((HDC) m_hDC, m_saveState);
                ms_cache.RemoveAt(index);

                // Reduce the number of bogus reports of non-freed memory
                // at app exit
                if (ms_cache.IsEmpty())
                        ms_cache.Clear();
        }
        //else: cached DC entry is still in use

        // prevent the base class dtor from ReleaseDC()ing it again
        m_hDC = 0;
}


