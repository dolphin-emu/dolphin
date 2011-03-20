/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/artmsw.cpp
// Purpose:     stock wxArtProvider instance with native MSW stock icons
// Author:      Vaclav Slavik
// Modified by:
// Created:     2008-10-15
// RCS-ID:      $Id: artmsw.cpp 62199 2009-09-29 17:04:08Z VS $
// Copyright:   (c) Vaclav Slavik, 2008
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#include "wx/artprov.h"
#include "wx/image.h"
#include "wx/msw/wrapwin.h"


// ----------------------------------------------------------------------------
// wxWindowsArtProvider
// ----------------------------------------------------------------------------

class wxWindowsArtProvider : public wxArtProvider
{
protected:
    virtual wxBitmap CreateBitmap(const wxArtID& id, const wxArtClient& client,
                                  const wxSize& size);
};

static wxBitmap CreateFromStdIcon(const char *iconName,
                                  const wxArtClient& client)
{
    wxIcon icon(iconName);
    wxBitmap bmp;
    bmp.CopyFromIcon(icon);

#if wxUSE_IMAGE
    // The standard native message box icons are in message box size (32x32).
    // If they are requested in any size other than the default or message
    // box size, they must be rescaled first.
    if ( client != wxART_MESSAGE_BOX && client != wxART_OTHER )
    {
        const wxSize size = wxArtProvider::GetNativeSizeHint(client);
        if ( size != wxDefaultSize )
        {
            wxImage img = bmp.ConvertToImage();
            img.Rescale(size.x, size.y);
            bmp = wxBitmap(img);
        }
    }
#endif // wxUSE_IMAGE

    return bmp;
}

wxBitmap wxWindowsArtProvider::CreateBitmap(const wxArtID& id,
                                            const wxArtClient& client,
                                            const wxSize& WXUNUSED(size))
{
    // handle message box icons specially (wxIcon ctor treat these names
    // as special cases via wxICOResourceHandler::LoadIcon):
    const char *name = NULL;
    if ( id == wxART_ERROR )
        name = "wxICON_ERROR";
    else if ( id == wxART_INFORMATION )
        name = "wxICON_INFORMATION";
    else if ( id == wxART_WARNING )
        name = "wxICON_WARNING";
    else if ( id == wxART_QUESTION )
        name = "wxICON_QUESTION";

    if ( name )
        return CreateFromStdIcon(name, client);

    // for anything else, fall back to generic provider:
    return wxNullBitmap;
}

// ----------------------------------------------------------------------------
// wxArtProvider::InitNativeProvider()
// ----------------------------------------------------------------------------

/*static*/ void wxArtProvider::InitNativeProvider()
{
    Push(new wxWindowsArtProvider);
}

// ----------------------------------------------------------------------------
// wxArtProvider::GetNativeSizeHint()
// ----------------------------------------------------------------------------

/*static*/
wxSize wxArtProvider::GetNativeSizeHint(const wxArtClient& client)
{
    if ( client == wxART_TOOLBAR )
    {
        return wxSize(24, 24);
    }
    else if ( client == wxART_MENU )
    {
        return wxSize(16, 16);
    }
    else if ( client == wxART_FRAME_ICON )
    {
        return wxSize(::GetSystemMetrics(SM_CXSMICON),
                      ::GetSystemMetrics(SM_CYSMICON));
    }
    else if ( client == wxART_CMN_DIALOG ||
              client == wxART_MESSAGE_BOX )
    {
        return wxSize(::GetSystemMetrics(SM_CXICON),
                      ::GetSystemMetrics(SM_CYICON));
    }
    else if (client == wxART_BUTTON)
    {
        return wxSize(16, 16);
    }

    return wxDefaultSize;
}
