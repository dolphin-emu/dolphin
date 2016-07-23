/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/artmac.cpp
// Purpose:     wxArtProvider instance with native Mac stock icons
// Author:      Alan Shouls
// Created:     2006-10-30
// Copyright:   (c) wxWindows team
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

#ifndef WX_PRECOMP
    #include "wx/image.h"
#endif

#include "wx/osx/private.h"

// ----------------------------------------------------------------------------
// wxMacArtProvider
// ----------------------------------------------------------------------------

class wxMacArtProvider : public wxArtProvider
{
protected:
#if wxOSX_USE_COCOA_OR_CARBON
    virtual wxIconBundle CreateIconBundle(const wxArtID& id,
                                          const wxArtClient& client) wxOVERRIDE;
#endif
#if wxOSX_USE_COCOA_OR_IPHONE
    virtual wxBitmap CreateBitmap(const wxArtID& id,
                                  const wxArtClient& client,
                                  const wxSize& size) wxOVERRIDE
    {
        return wxOSXCreateSystemBitmap(id, client, size);
    }
#endif
};

/* static */ void wxArtProvider::InitNativeProvider()
{
    PushBack(new wxMacArtProvider);
}

#if wxOSX_USE_COCOA_OR_CARBON

// ----------------------------------------------------------------------------
// helper macros
// ----------------------------------------------------------------------------

#define CREATE_STD_ICON(iconId, xpmRc) \
    { \
        wxIconBundle icon(wxT(iconId), wxBITMAP_TYPE_ICON_RESOURCE); \
        return icon; \
    }

// Macro used in CreateBitmap to get wxICON_FOO icons:
#define ART_MSGBOX(artId, iconId, xpmRc) \
    if ( id == artId ) \
    { \
        CREATE_STD_ICON(#iconId, xpmRc) \
    }

static wxIconBundle wxMacArtProvider_CreateIconBundle(const wxArtID& id)
{
    ART_MSGBOX(wxART_ERROR,       wxICON_ERROR,       error)
    ART_MSGBOX(wxART_INFORMATION, wxICON_INFORMATION, info)
    ART_MSGBOX(wxART_WARNING,     wxICON_WARNING,     warning)
    ART_MSGBOX(wxART_QUESTION,    wxICON_QUESTION,    question)

    ART_MSGBOX(wxART_FOLDER,      wxICON_FOLDER,      folder)
    ART_MSGBOX(wxART_FOLDER_OPEN, wxICON_FOLDER_OPEN, folder_open)
    ART_MSGBOX(wxART_NORMAL_FILE, wxICON_NORMAL_FILE, deffile)
    ART_MSGBOX(wxART_EXECUTABLE_FILE, wxICON_EXECUTABLE_FILE, exefile)

    ART_MSGBOX(wxART_CDROM,       wxICON_CDROM,       cdrom)
    ART_MSGBOX(wxART_FLOPPY,      wxICON_FLOPPY,      floppy)
    ART_MSGBOX(wxART_HARDDISK,    wxICON_HARDDISK,    harddisk)
    ART_MSGBOX(wxART_REMOVABLE,   wxICON_REMOVABLE,   removable)
    ART_MSGBOX(wxART_PRINT,       wxICON_PRINT,       print)

    ART_MSGBOX(wxART_DELETE,      wxICON_DELETE,      delete)

    ART_MSGBOX(wxART_GO_BACK,     wxICON_GO_BACK,     back)
    ART_MSGBOX(wxART_GO_FORWARD,  wxICON_GO_FORWARD,  forward)
    ART_MSGBOX(wxART_GO_HOME,     wxICON_GO_HOME,     home)

    ART_MSGBOX(wxART_HELP_SETTINGS, wxICON_HELP_SETTINGS, htmoptns)
    ART_MSGBOX(wxART_HELP_PAGE,   wxICON_HELP_PAGE,   htmpage)
    ART_MSGBOX(wxART_HELP_FOLDER,   wxICON_HELP_FOLDER,   htmlfoldr)

    return wxNullIconBundle;
}

// ----------------------------------------------------------------------------
// CreateIconBundle
// ----------------------------------------------------------------------------

wxIconBundle wxMacArtProvider::CreateIconBundle(const wxArtID& id, const wxArtClient& client)
{
    // On the Mac folders in lists are always drawn closed, so if an open
    // folder icon is asked for we will ask for a closed one in its place
    if ( client == wxART_LIST && id == wxART_FOLDER_OPEN )
        return wxMacArtProvider_CreateIconBundle(wxART_FOLDER);

    return wxMacArtProvider_CreateIconBundle(id);
}

#endif

// ----------------------------------------------------------------------------
// wxArtProvider::GetNativeSizeHint()
// ----------------------------------------------------------------------------

/*static*/
wxSize wxArtProvider::GetNativeSizeHint(const wxArtClient& client)
{
    if ( client == wxART_TOOLBAR )
    {
        // See http://developer.apple.com/documentation/UserExperience/Conceptual/AppleHIGuidelines/XHIGIcons/chapter_15_section_9.html:
        // "32 x 32 pixels is the recommended size"
        return wxSize(32, 32);
    }
    else if ( client == wxART_BUTTON || client == wxART_MENU )
    {
        // Mac UI doesn't use any images in neither buttons nor menus in
        // general but the code using wxArtProvider can use wxART_BUTTON to
        // find the icons of a roughly appropriate size for the buttons and
        // 16x16 seems to be the best choice for this kind of use
        return wxSize(16, 16);
    }

    return wxDefaultSize;
}

