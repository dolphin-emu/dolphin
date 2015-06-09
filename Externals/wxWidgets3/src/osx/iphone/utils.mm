/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/iphone/utils.mm
// Purpose:     various cocoa utility functions
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/wxprec.h"

#include "wx/utils.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/app.h"
    #if wxUSE_GUI
        #include "wx/toplevel.h"
        #include "wx/font.h"
    #endif
#endif

#include "wx/apptrait.h"

#include "wx/osx/private.h"

#if wxUSE_GUI
    #include "wx/osx/private/timer.h"
    #include "wx/osx/dcclient.h"
#endif // wxUSE_GUI

#if wxOSX_USE_IPHONE

#include <AudioToolbox/AudioServices.h>

#if 1 // wxUSE_BASE

// ----------------------------------------------------------------------------
// Common Event Support
// ----------------------------------------------------------------------------

@interface wxAppDelegate : NSObject <UIApplicationDelegate> {
}

@end

@implementation wxAppDelegate

- (BOOL)application:(UIApplication *)application willFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    wxUnusedVar(application);
    wxUnusedVar(launchOptions);
    wxTheApp->OSXOnWillFinishLaunching();
    return YES;
}

- (void)applicationDidFinishLaunching:(UIApplication *)application {	
    wxTheApp->OSXOnDidFinishLaunching();
}

- (void)applicationWillTerminate:(UIApplication *)application { 
    wxUnusedVar(application);
    wxTheApp->OSXOnWillTerminate();
}

- (void)dealloc {
	[super dealloc];
}


@end

bool wxApp::CallOnInit()
{
    return true;
}

bool wxApp::DoInitGui()
{
    return true;
}

void wxApp::DoCleanUp()
{
}

#endif // wxUSE_BASE

#if wxUSE_GUI

// Emit a beeeeeep
void wxBell()
{
    // would be kSystemSoundID_UserPreferredAlert but since the headers aren't correct, add it manually
    AudioServicesPlayAlertSound(0x00001000 );
}

// ----------------------------------------------------------------------------
// Launch default browser
// ----------------------------------------------------------------------------

bool wxDoLaunchDefaultBrowser(const wxString& url, int flags)
{
    return [[UIApplication sharedApplication] openURL:[NSURL URLWithString:wxCFStringRef(url).AsNSString()]]
        == YES;
}

// TODO : reorganize

extern wxFont* CreateNormalFont()
{
    return new wxFont([UIFont systemFontSize] , wxSWISS, wxNORMAL, wxNORMAL, FALSE, "Helvetica" );
}

extern wxFont* CreateSmallFont()
{
    return new wxFont([UIFont smallSystemFontSize] , wxSWISS, wxNORMAL, wxNORMAL, FALSE, "Helvetica" );
}

extern UIFont* CreateUIFont( const wxFont& font )
{
    return [UIFont fontWithName:wxCFStringRef(font.GetFaceName() ).AsNSString() size:font.GetPointSize()];
}

CFArrayRef CopyAvailableFontFamilyNames()
{
    return (CFArrayRef) [[UIFont familyNames] retain];
}

extern void DrawTextInContext( CGContextRef context, CGPoint where, UIFont *font, NSString* text )
{
    bool contextChanged = ( UIGraphicsGetCurrentContext() != context );
    if ( contextChanged )
        UIGraphicsPushContext(context);

    [text drawAtPoint:where withFont:font];

    if ( contextChanged )
        UIGraphicsPopContext();
}

extern CGSize MeasureTextInContext( UIFont *font, NSString* text )
{
    return  [text sizeWithFont:font];
}

void wxClientDisplayRect(int *x, int *y, int *width, int *height)
{
#if 0
    CGRect r = [[UIScreen mainScreen] applicationFrame];
    CGRect bounds = [[UIScreen mainScreen] bounds];
    if ( bounds.size.height > r.size.height )
    {
        // portrait
        if ( x )
            *x = r.origin.x;
        if ( y )
            *y = r.origin.y;
        if ( width )
            *width = r.size.width;
        if ( height )
            *height = r.size.height;
    }
    else
    {
        // landscape
        if ( x )
            *x = r.origin.y;
        if ( y )
            *y = r.origin.x;
        if ( width )
            *width = r.size.height;
        if ( height )
            *height = r.size.width;
    }
#else
    // it's easier to treat the status bar as an element of the toplevel window 
    // instead of the desktop in order to support easy rotation
    if ( x )
        *x = 0;
    if ( y )
        *y = 0;
    wxDisplaySize(width, height);
#endif
}

void wxGetMousePosition( int* x, int* y )
{
    if ( x )
        *x = 0;
    if ( y )
        *y = 0;
};

wxMouseState wxGetMouseState()
{
    wxMouseState ms;
    return ms;
}    

// Returns depth of screen
int wxDisplayDepth()
{
    return 32; // TODO can we determine this ?
}

// Get size of display
void wxDisplaySize(int *width, int *height)
{
    CGRect r = [[UIScreen mainScreen] applicationFrame];
    CGRect bounds = [[UIScreen mainScreen] bounds];

    if ( UIInterfaceOrientationIsPortrait([[UIApplication sharedApplication] statusBarOrientation]) )
    {
        // portrait
        if ( width )
            *width = (int)bounds.size.width ;
        if ( height )
            *height = (int)bounds.size.height;
    }
    else
    {
        // landscape
        if ( width )
            *width = (int)bounds.size.height ;
        if ( height )
            *height = (int)bounds.size.width;
    }
}

wxTimerImpl* wxGUIAppTraits::CreateTimerImpl(wxTimer *timer)
{
    return new wxOSXTimerImpl(timer);
}

int gs_wxBusyCursorCount = 0;
extern wxCursor    gMacCurrentCursor;
wxCursor        gMacStoredActiveCursor;

// Set the cursor to the busy cursor for all windows
void wxBeginBusyCursor(const wxCursor *cursor)
{
    if (gs_wxBusyCursorCount++ == 0)
    {
        gMacStoredActiveCursor = gMacCurrentCursor;
        cursor->MacInstall();

        wxSetCursor(*cursor);
    }
    //else: nothing to do, already set
}

// Restore cursor to normal
void wxEndBusyCursor()
{
    wxCHECK_RET( gs_wxBusyCursorCount > 0,
        wxT("no matching wxBeginBusyCursor() for wxEndBusyCursor()") );

    if (--gs_wxBusyCursorCount == 0)
    {
        gMacStoredActiveCursor.MacInstall();
        gMacStoredActiveCursor = wxNullCursor;

        wxSetCursor(wxNullCursor);
    }
}

// true if we're between the above two calls
bool wxIsBusy()
{
    return (gs_wxBusyCursorCount > 0);
}

bool wxGetKeyState (wxKeyCode key)
{
    return false;
}

wxBitmap wxWindowDCImpl::DoGetAsBitmap(const wxRect *subrect) const
{
    // wxScreenDC is derived from wxWindowDC, so a screen dc will
    // call this method when a Blit is performed with it as a source.
    if (!m_window)
        return wxNullBitmap;

    wxSize sz = m_window->GetSize();

    int left = subrect != NULL ? subrect->x : 0 ;
    int top = subrect != NULL ? subrect->y : 0 ;
    int width = subrect != NULL ? subrect->width : sz.x;
    int height = subrect !=  NULL ? subrect->height : sz.y ;

    wxBitmap bmp = wxBitmap(width, height, 32);

    CGContextRef context = (CGContextRef)bmp.GetHBITMAP();

    CGContextSaveGState(context);


    CGContextTranslateCTM( context, 0,  height );
    CGContextScaleCTM( context, 1, -1 );

    if ( subrect )
        CGContextTranslateCTM( context, -subrect->x, -subrect->y ) ;

    UIGraphicsPushContext(context);
    [ (NSView*) m_window->GetHandle() drawRect:CGRectMake(left, top, width, height ) ];
    UIGraphicsPopContext();
    CGContextRestoreGState(context);

    return bmp;
}

#endif // wxUSE_GUI

wxOperatingSystemId wxGetOsVersion(int *verMaj, int *verMin)
{
    // get OS version
    int major, minor;

    wxString release = wxCFStringRef( wxCFRetain( [ [UIDevice currentDevice] systemVersion] ) ).AsString() ;

    if ( release.empty() ||
        // TODO use wx method
         scanf(release.c_str(), wxT("%d.%d"), &major, &minor) != 2 )
    {
        // failed to get version string or unrecognized format
        major =
        minor = -1;
    }

    if ( verMaj )
        *verMaj = major;
    if ( verMin )
        *verMin = minor;

    return wxOS_MAC_OSX_DARWIN;
}

wxString wxGetOsDescription()
{
    wxString release = wxCFStringRef( wxCFRetain([ [UIDevice currentDevice] systemName] )).AsString() ;

    return release;
}


#endif // wxOSX_USE_IPHONE
