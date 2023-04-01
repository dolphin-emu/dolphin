///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/aboutdlg.mm
// Purpose:     native wxAboutBox() implementation for wxMac
// Author:      Vadim Zeitlin
// Created:     2006-10-08
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
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

#if wxUSE_ABOUTDLG

#ifndef WX_PRECOMP
#endif //WX_PRECOMP

#include "wx/aboutdlg.h"
#include "wx/generic/aboutdlgg.h"

#include "wx/osx/private.h"

// see http://developer.apple.com/mac/library/technotes/tn2006/tn2179.html for
// information about the various keys used here

// helper class for HIAboutBox options
class AboutBoxOptions : public wxCFRef<CFMutableDictionaryRef>
{
public:
    AboutBoxOptions() : wxCFRef<CFMutableDictionaryRef>
                        (
                          CFDictionaryCreateMutable
                          (
                           kCFAllocatorDefault,
                           4, // there are at most 4 values
                           &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks
                          )
                        )
    {
    }

    void Set(CFStringRef key, const wxString& value)
    {
        CFDictionarySetValue(*this, key, wxCFStringRef(value));
    }

    void SetAttributedString( CFStringRef key, const wxString& value )
    {
          wxCFRef<CFAttributedStringRef> attrString(
            CFAttributedStringCreate(kCFAllocatorDefault, wxCFStringRef(value), NULL) );
        CFDictionarySetValue(*this, key, attrString);
    }
};

// ============================================================================
// implementation
// ============================================================================

void wxAboutBox(const wxAboutDialogInfo& info, wxWindow *parent)
{
    // Mac native about box currently can show only name, version, copyright
    // and description fields and we also shoehorn the credits text into the
    // description but if we have anything else we must use the generic version

    if ( info.IsSimple() )
    {
        AboutBoxOptions opts;

        opts.Set(CFSTR("ApplicationName"), info.GetName());

        if ( info.HasVersion() )
        {
            opts.Set(CFSTR("Version"),info.GetVersion());
            if ( info.GetLongVersion() != (_("Version ")+info.GetVersion()))
                opts.Set(CFSTR("ApplicationVersion"),info.GetLongVersion());
        }

        if ( info.HasCopyright() )
            opts.Set(CFSTR("Copyright"), info.GetCopyrightToDisplay());

        opts.SetAttributedString(CFSTR("Credits"), info.GetDescriptionAndCredits());

        [[NSApplication sharedApplication] orderFrontStandardAboutPanelWithOptions:((NSDictionary*)(CFDictionaryRef) opts)];
    }
    else // simple "native" version is not enough
    {
        // we need to use the full-blown generic version
        wxGenericAboutBox(info, parent);
    }
}

#endif // wxUSE_ABOUTDLG
