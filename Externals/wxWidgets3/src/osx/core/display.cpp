/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/display.cpp
// Purpose:     Mac implementation of wxDisplay class
// Author:      Ryan Norton & Brian Victor
// Modified by: Royce Mitchell III, Vadim Zeitlin
// Created:     06/21/02
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_DISPLAY

#include "wx/display.h"

#ifndef WX_PRECOMP
    #include "wx/dynarray.h"
    #include "wx/log.h"
    #include "wx/string.h"
    #include "wx/gdicmn.h"
#endif

#include "wx/display_impl.h"
#include "wx/scopedarray.h"
#include "wx/osx/private.h"

#if wxOSX_USE_COCOA_OR_CARBON

// ----------------------------------------------------------------------------
// display classes implementation
// ----------------------------------------------------------------------------

class wxDisplayImplMacOSX : public wxDisplayImpl
{
public:
    wxDisplayImplMacOSX(unsigned n, CGDirectDisplayID id)
        : wxDisplayImpl(n),
          m_id(id)
    {
    }

    virtual wxRect GetGeometry() const wxOVERRIDE;
    virtual wxRect GetClientArea() const wxOVERRIDE;
    virtual wxString GetName() const wxOVERRIDE { return wxString(); }

    virtual wxArrayVideoModes GetModes(const wxVideoMode& mode) const wxOVERRIDE;
    virtual wxVideoMode GetCurrentMode() const wxOVERRIDE;
    virtual bool ChangeMode(const wxVideoMode& mode) wxOVERRIDE;

    virtual bool IsPrimary() const wxOVERRIDE;

private:
    CGDirectDisplayID m_id;

    wxDECLARE_NO_COPY_CLASS(wxDisplayImplMacOSX);
};

class wxDisplayFactoryMacOSX : public wxDisplayFactory
{
public:
    wxDisplayFactoryMacOSX() {}

    virtual wxDisplayImpl *CreateDisplay(unsigned n) wxOVERRIDE;
    virtual unsigned GetCount() wxOVERRIDE;
    virtual int GetFromPoint(const wxPoint& pt) wxOVERRIDE;

protected:
    wxDECLARE_NO_COPY_CLASS(wxDisplayFactoryMacOSX);
};

// ============================================================================
// wxDisplayFactoryMacOSX implementation
// ============================================================================

// gets all displays that are not mirror displays

static CGDisplayErr wxOSXGetDisplayList(CGDisplayCount maxDisplays,
                                   CGDirectDisplayID *displays,
                                   CGDisplayCount *displayCount)
{
    CGDisplayErr error = kCGErrorSuccess;
    CGDisplayCount onlineCount;

    error = CGGetOnlineDisplayList(0,NULL,&onlineCount);
    if ( error == kCGErrorSuccess )
    {
        *displayCount = 0;
        if ( onlineCount > 0 )
        {
            CGDirectDisplayID *onlineDisplays = new CGDirectDisplayID[onlineCount];
            error = CGGetOnlineDisplayList(onlineCount,onlineDisplays,&onlineCount);
            if ( error == kCGErrorSuccess )
            {
                for ( CGDisplayCount i = 0; i < onlineCount; ++i )
                {
                    if ( CGDisplayMirrorsDisplay(onlineDisplays[i]) != kCGNullDirectDisplay )
                        continue;

                    if ( displays == NULL )
                        *displayCount += 1;
                    else
                    {
                        if ( *displayCount < maxDisplays )
                        {
                            displays[*displayCount] = onlineDisplays[i];
                            *displayCount += 1;
                        }
                    }
                }
            }
            delete[] onlineDisplays;
        }

    }
    return error;
}

unsigned wxDisplayFactoryMacOSX::GetCount()
{
    CGDisplayCount count;
    CGDisplayErr err = wxOSXGetDisplayList(0, NULL, &count);

    wxCHECK_MSG( err == CGDisplayNoErr, 0, "wxOSXGetDisplayList() failed" );

    return count;
}

int wxDisplayFactoryMacOSX::GetFromPoint(const wxPoint& p)
{
    CGPoint thePoint = {(float)p.x, (float)p.y};
    CGDirectDisplayID theID;
    CGDisplayCount theCount;
    CGDisplayErr err = CGGetDisplaysWithPoint(thePoint, 1, &theID, &theCount);
    wxASSERT(err == CGDisplayNoErr);

    int nWhich = wxNOT_FOUND;

    if (theCount)
    {
        theCount = GetCount();
        CGDirectDisplayID* theIDs = new CGDirectDisplayID[theCount];
        err = wxOSXGetDisplayList(theCount, theIDs, &theCount);
        wxASSERT(err == CGDisplayNoErr);

        for (nWhich = 0; nWhich < (int) theCount; ++nWhich)
        {
            if (theIDs[nWhich] == theID)
                break;
        }

        delete [] theIDs;

        if (nWhich == (int) theCount)
        {
            wxFAIL_MSG(wxT("Failed to find display in display list"));
            nWhich = wxNOT_FOUND;
        }
    }

    return nWhich;
}

wxDisplayImpl *wxDisplayFactoryMacOSX::CreateDisplay(unsigned n)
{
    CGDisplayCount theCount = GetCount();
    wxScopedArray<CGDirectDisplayID> theIDs(theCount);

    CGDisplayErr err = wxOSXGetDisplayList(theCount, theIDs.get(), &theCount);
    wxCHECK_MSG( err == CGDisplayNoErr, NULL, "wxOSXGetDisplayList() failed" );

    wxCHECK_MSG( n < theCount, NULL, wxS("Invalid display index") );

    return new wxDisplayImplMacOSX(n, theIDs[n]);
}

// ============================================================================
// wxDisplayImplMacOSX implementation
// ============================================================================

bool wxDisplayImplMacOSX::IsPrimary() const
{
    return CGDisplayIsMain(m_id);
}

wxRect wxDisplayImplMacOSX::GetGeometry() const
{
    CGRect theRect = CGDisplayBounds(m_id);
    return wxRect( (int)theRect.origin.x,
                   (int)theRect.origin.y,
                   (int)theRect.size.width,
                   (int)theRect.size.height ); //floats
}

wxRect wxDisplayImplMacOSX::GetClientArea() const
{
    // VZ: I don't know how to get client area for arbitrary display but
    //     wxGetClientDisplayRect() does work correctly for at least the main
    //     one (TODO: do it correctly for the other displays too)
    if ( IsPrimary() )
        return wxGetClientDisplayRect();

    return wxDisplayImpl::GetClientArea();
}

static int wxCFDictKeyToInt( CFDictionaryRef desc, CFStringRef key )
{
    CFNumberRef value = (CFNumberRef) CFDictionaryGetValue( desc, key );
    if (value == NULL)
        return 0;

    int num = 0;
    CFNumberGetValue( value, kCFNumberIntType, &num );

    return num;
}

wxArrayVideoModes wxDisplayImplMacOSX::GetModes(const wxVideoMode& mode) const
{
    wxArrayVideoModes resultModes;

    CFArrayRef theArray = CGDisplayAvailableModes( m_id );

    for (CFIndex i = 0; i < CFArrayGetCount(theArray); ++i)
    {
        CFDictionaryRef theValue = (CFDictionaryRef) CFArrayGetValueAtIndex( theArray, i );

        wxVideoMode theMode(
            wxCFDictKeyToInt( theValue, kCGDisplayWidth ),
            wxCFDictKeyToInt( theValue, kCGDisplayHeight ),
            wxCFDictKeyToInt( theValue, kCGDisplayBitsPerPixel ),
            wxCFDictKeyToInt( theValue, kCGDisplayRefreshRate ));

        if (theMode.Matches( mode ))
            resultModes.Add( theMode );
    }

    return resultModes;
}

wxVideoMode wxDisplayImplMacOSX::GetCurrentMode() const
{
    CFDictionaryRef theValue = CGDisplayCurrentMode( m_id );

    return wxVideoMode(
        wxCFDictKeyToInt( theValue, kCGDisplayWidth ),
        wxCFDictKeyToInt( theValue, kCGDisplayHeight ),
        wxCFDictKeyToInt( theValue, kCGDisplayBitsPerPixel ),
        wxCFDictKeyToInt( theValue, kCGDisplayRefreshRate ));
}

bool wxDisplayImplMacOSX::ChangeMode( const wxVideoMode& mode )
{
#ifndef __WXOSX_IPHONE__
    if (mode == wxDefaultVideoMode)
    {
        CGRestorePermanentDisplayConfiguration();
        return true;
    }
#endif

    boolean_t bExactMatch;
    CFDictionaryRef theCGMode = CGDisplayBestModeForParametersAndRefreshRate(
        m_id,
        (size_t)mode.GetDepth(),
        (size_t)mode.GetWidth(),
        (size_t)mode.GetHeight(),
        (double)mode.GetRefresh(),
        &bExactMatch );

    bool bOK = bExactMatch;

    if (bOK)
        bOK = CGDisplaySwitchToMode( m_id, theCGMode ) == CGDisplayNoErr;

    return bOK;
}

// ============================================================================
// wxDisplay::CreateFactory()
// ============================================================================

/* static */ wxDisplayFactory *wxDisplay::CreateFactory()
{
    return new wxDisplayFactoryMacOSX;
}

#else

/* static */ wxDisplayFactory *wxDisplay::CreateFactory()
{
    return new wxDisplayFactorySingle;
}

#endif

#endif // wxUSE_DISPLAY
