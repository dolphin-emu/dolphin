/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/utils.mm
// Purpose:     various cocoa utility functions
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/utils.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/app.h"
    #if wxUSE_GUI
        #include "wx/dialog.h"
        #include "wx/toplevel.h"
        #include "wx/font.h"
    #endif
#endif

#include "wx/apptrait.h"

#include "wx/osx/private.h"

#if wxUSE_GUI
#if wxOSX_USE_COCOA_OR_CARBON
    #include <CoreServices/CoreServices.h>
    #include "wx/osx/dcclient.h"
    #include "wx/osx/private/timer.h"
#endif
#endif // wxUSE_GUI

#if wxOSX_USE_COCOA

#if wxUSE_GUI

// Emit a beeeeeep
void wxBell()
{
    NSBeep();
}

@implementation wxNSAppController

- (void)applicationWillFinishLaunching:(NSNotification *)application
{
    wxUnusedVar(application);
    
    // we must install our handlers later than setting the app delegate, because otherwise our handlers
    // get overwritten in the meantime

    NSAppleEventManager *appleEventManager = [NSAppleEventManager sharedAppleEventManager];
    
    [appleEventManager setEventHandler:self andSelector:@selector(handleGetURLEvent:withReplyEvent:)
                         forEventClass:kInternetEventClass andEventID:kAEGetURL];
    
    [appleEventManager setEventHandler:self andSelector:@selector(handleOpenAppEvent:withReplyEvent:)
                         forEventClass:kCoreEventClass andEventID:kAEOpenApplication];
    
    wxTheApp->OSXOnWillFinishLaunching();
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    wxTheApp->OSXOnDidFinishLaunching();
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)fileNames
{
    wxUnusedVar(sender);
    wxArrayString fileList;
    size_t i;
    const size_t count = [fileNames count];
    for (i = 0; i < count; i++)
    {
        fileList.Add( wxCFStringRef::AsStringWithNormalizationFormC([fileNames objectAtIndex:i]) );
    }

    wxTheApp->MacOpenFiles(fileList);
}

- (BOOL)application:(NSApplication *)sender printFile:(NSString *)filename
{
    wxUnusedVar(sender);
    wxCFStringRef cf(wxCFRetain(filename));
    wxTheApp->MacPrintFile(cf.AsString()) ;
    return YES;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
{
    wxUnusedVar(flag);
    wxUnusedVar(sender);
    wxTheApp->MacReopenApp() ;
    return NO;
}

- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event
           withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    wxUnusedVar(replyEvent);
    NSString* url = [[event descriptorAtIndex:1] stringValue];
    wxCFStringRef cf(wxCFRetain(url));
    wxTheApp->MacOpenURL(cf.AsString()) ;
}

- (void)handleOpenAppEvent:(NSAppleEventDescriptor *)event
           withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    wxUnusedVar(replyEvent);
    wxTheApp->MacNewFile() ;
}

/*
    Allowable return values are:
        NSTerminateNow - it is ok to proceed with termination
        NSTerminateCancel - the application should not be terminated
        NSTerminateLater - it may be ok to proceed with termination later.  The application must call -replyToApplicationShouldTerminate: with YES or NO once the answer is known
            this return value is for delegates who need to provide document modal alerts (sheets) in order to decide whether to quit.
*/
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    wxUnusedVar(sender);
    if ( !wxTheApp->OSXOnShouldTerminate() )
        return NSTerminateCancel;
    
    return NSTerminateNow;
}

- (void)applicationWillTerminate:(NSNotification *)application {
    wxUnusedVar(application);
    wxTheApp->OSXOnWillTerminate();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    wxUnusedVar(sender);
    // let wx do this, not cocoa
    return NO;
}

- (void)applicationDidBecomeActive:(NSNotification *)notification
{
    wxUnusedVar(notification);

    for ( wxWindowList::const_iterator i = wxTopLevelWindows.begin(),
         end = wxTopLevelWindows.end();
         i != end;
         ++i )
    {
        wxTopLevelWindow * const win = static_cast<wxTopLevelWindow *>(*i);
        wxNonOwnedWindowImpl* winimpl = win ? win->GetNonOwnedPeer() : NULL;
        WXWindow nswindow = win ? win->GetWXWindow() : nil;
        
        if ( nswindow && [nswindow hidesOnDeactivate] == NO && winimpl)
            winimpl->RestoreWindowLevel();
    }
    if ( wxTheApp )
        wxTheApp->SetActive( true , NULL ) ;
}

- (void)applicationWillResignActive:(NSNotification *)notification
{
    wxUnusedVar(notification);
    for ( wxWindowList::const_iterator i = wxTopLevelWindows.begin(),
         end = wxTopLevelWindows.end();
         i != end;
         ++i )
    {
        wxTopLevelWindow * const win = static_cast<wxTopLevelWindow *>(*i);
        WXWindow nswindow = win ? win->GetWXWindow() : nil;
        
        if ( nswindow && [nswindow level] == kCGFloatingWindowLevel && [nswindow hidesOnDeactivate] == NO )
            [nswindow setLevel:kCGNormalWindowLevel];
    }
}

- (void)applicationDidResignActive:(NSNotification *)notification
{
    wxUnusedVar(notification);
    if ( wxTheApp )
        wxTheApp->SetActive( false , NULL ) ;
}

@end

/*
    allows ShowModal to work when using sheets.
    see include/wx/osx/cocoa/private.h for more info
*/
@implementation ModalDialogDelegate
- (id)init
{
    self = [super init];
    sheetFinished = NO;
    resultCode = -1;
    impl = 0;
    return self;
}

- (void)setImplementation: (wxDialog *)dialog
{
    impl = dialog;
}

- (BOOL)finished
{
    return sheetFinished;
}

- (int)code
{
    return resultCode;
}

- (void)waitForSheetToFinish
{
    while (!sheetFinished)
    {
        wxSafeYield();
    }
}

- (void)sheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    wxUnusedVar(contextInfo);
    resultCode = returnCode;
    sheetFinished = YES;
    // NSAlerts don't need nor respond to orderOut
    if ([sheet respondsToSelector:@selector(orderOut:)])
        [sheet orderOut: self];
        
    if (impl)
        impl->ModalFinishedCallback(sheet, returnCode);
}
@end

// here we subclass NSApplication, for the purpose of being able to override sendEvent.
@interface wxNSApplication : NSApplication
{
    BOOL firstPass;
}

- (id)init;

- (void)sendEvent:(NSEvent *)anEvent;

@end

@implementation wxNSApplication

- (id)init
{
    self = [super init];
    firstPass = YES;
    return self;
}

/* This is needed because otherwise we don't receive any key-up events for command-key
 combinations (an AppKit bug, apparently)			*/
- (void)sendEvent:(NSEvent *)anEvent
{
    if ([anEvent type] == NSKeyUp && ([anEvent modifierFlags] & NSCommandKeyMask))
        [[self keyWindow] sendEvent:anEvent];
    else
        [super sendEvent:anEvent];
    
    if ( firstPass )
    {
        [NSApp stop:nil];
        firstPass = NO;
        return;
    }
}

@end

WX_NSObject appcontroller = nil;

NSLayoutManager* gNSLayoutManager = nil;

WX_NSObject wxApp::OSXCreateAppController()
{
    return [[wxNSAppController alloc] init];
}

bool wxApp::DoInitGui()
{
    wxMacAutoreleasePool pool;

    if (!sm_isEmbedded)
    {
        [wxNSApplication sharedApplication];

        appcontroller = OSXCreateAppController();
        [NSApp setDelegate:appcontroller];
        [NSColor setIgnoresAlpha:NO];

        // calling finishLaunching so early before running the loop seems to trigger some 'MenuManager compatibility' which leads
        // to the duplication of menus under 10.5 and a warning under 10.6
#if 0
        [NSApp finishLaunching];
#endif
    }
    gNSLayoutManager = [[NSLayoutManager alloc] init];
    
    return true;
}

bool wxApp::CallOnInit()
{
    wxMacAutoreleasePool autoreleasepool;
    m_onInitResult = false;
    [NSApp run];
    return m_onInitResult; 
}

void wxApp::DoCleanUp()
{
    if ( appcontroller != nil )
    {
        [NSApp setDelegate:nil];
        [appcontroller release];
        appcontroller = nil;
    }
    if ( gNSLayoutManager != nil )
    {
        [gNSLayoutManager release];
        gNSLayoutManager = nil;
    }
}

void wxClientDisplayRect(int *x, int *y, int *width, int *height)
{
    NSRect displayRect = [wxOSXGetMenuScreen() visibleFrame];
    wxRect r = wxFromNSRect( NULL, displayRect );
    if ( x )
        *x = r.x;
    if ( y )
        *y = r.y;
    if ( width )
        *width = r.GetWidth();
    if ( height )
        *height = r.GetHeight();

}

void wxGetMousePosition( int* x, int* y )
{
    wxPoint pt = wxFromNSPoint(NULL, [NSEvent mouseLocation]);
    if ( x )
        *x = pt.x;
    if ( y )
        *y = pt.y;
};

#if wxOSX_USE_COCOA && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_6

wxMouseState wxGetMouseState()
{
    wxMouseState ms;
    
    wxPoint pt = wxGetMousePosition();
    ms.SetX(pt.x);
    ms.SetY(pt.y);
    
    NSUInteger modifiers = [NSEvent modifierFlags];
    NSUInteger buttons = [NSEvent pressedMouseButtons];
    
    ms.SetLeftDown( (buttons & 0x01) != 0 );
    ms.SetMiddleDown( (buttons & 0x04) != 0 );
    ms.SetRightDown( (buttons & 0x02) != 0 );
    
    ms.SetRawControlDown(modifiers & NSControlKeyMask);
    ms.SetShiftDown(modifiers & NSShiftKeyMask);
    ms.SetAltDown(modifiers & NSAlternateKeyMask);
    ms.SetControlDown(modifiers & NSCommandKeyMask);
    
    return ms;
}


#endif

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
        NSEnumerator *enumerator = [[[NSApplication sharedApplication] windows] objectEnumerator];
        id object;
        
        while ((object = [enumerator nextObject])) {
            [(NSWindow*) object disableCursorRects];
        }        

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
        NSEnumerator *enumerator = [[[NSApplication sharedApplication] windows] objectEnumerator];
        id object;
        
        while ((object = [enumerator nextObject])) {
            [(NSWindow*) object enableCursorRects];
        }        

        wxSetCursor(wxNullCursor);

        gMacStoredActiveCursor.MacInstall();
        gMacStoredActiveCursor = wxNullCursor;
    }
}

// true if we're between the above two calls
bool wxIsBusy()
{
    return (gs_wxBusyCursorCount > 0);
}

wxBitmap wxWindowDCImpl::DoGetAsBitmap(const wxRect *subrect) const
{
    // wxScreenDC is derived from wxWindowDC, so a screen dc will
    // call this method when a Blit is performed with it as a source.
    if (!m_window)
        return wxNullBitmap;

    wxSize sz = m_window->GetSize();

    int width = subrect != NULL ? subrect->width : sz.x;
    int height = subrect !=  NULL ? subrect->height : sz.y ;

    wxBitmap bitmap(width, height);

    NSView* view = (NSView*) m_window->GetHandle();
    if ( [view isHiddenOrHasHiddenAncestor] == NO )
    {
        [view lockFocus];
        // we use this method as other methods force a repaint, and this method can be
        // called from OnPaint, even with the window's paint dc as source (see wxHTMLWindow)
        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithFocusedViewRect: [view bounds]];
        [view unlockFocus];
        if ( [rep respondsToSelector:@selector(CGImage)] )
        {
            CGImageRef cgImageRef = (CGImageRef)[rep CGImage];

            CGRect r = CGRectMake( 0 , 0 , CGImageGetWidth(cgImageRef)  , CGImageGetHeight(cgImageRef) );
            // since our context is upside down we dont use CGContextDrawImage
            wxMacDrawCGImage( (CGContextRef) bitmap.GetHBITMAP() , &r, cgImageRef ) ;
        }
        else
        {
            // TODO for 10.4 in case we can support this for osx_cocoa
        }
        [rep release];
    }

    return bitmap;
}

#endif // wxUSE_GUI

#endif // wxOSX_USE_COCOA
