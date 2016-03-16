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
#include "wx/platinfo.h"

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

    [appleEventManager setEventHandler:self andSelector:@selector(handleQuitAppEvent:withReplyEvent:)
                         forEventClass:kCoreEventClass andEventID:kAEQuitApplication];

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

    if ( wxTheApp->OSXInitWasCalled() )
        wxTheApp->MacOpenFiles(fileList);
    else
        wxTheApp->OSXStoreOpenFiles(fileList);
}

- (NSApplicationPrintReply)application:(NSApplication *)sender printFiles:(NSArray *)fileNames withSettings:(NSDictionary *)printSettings showPrintPanels:(BOOL)showPrintPanels
{
    wxUnusedVar(sender);
    wxArrayString fileList;
    size_t i;
    const size_t count = [fileNames count];
    for (i = 0; i < count; i++)
    {
        fileList.Add( wxCFStringRef::AsStringWithNormalizationFormC([fileNames objectAtIndex:i]) );
    }
    
    if ( wxTheApp->OSXInitWasCalled() )
        wxTheApp->MacPrintFiles(fileList);
    else
        wxTheApp->OSXStorePrintFiles(fileList);
    
    return NSPrintingSuccess;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
{
    wxUnusedVar(flag);
    wxUnusedVar(sender);
    if ( wxTheApp->OSXInitWasCalled() )
        wxTheApp->MacReopenApp();
    // else: It's possible that this function was called as the first thing.
    //       This can happen when OS X restores running apps when starting a new
    //       user session. Apps that were hidden (dock only) when the previous
    //       session terminated are only restored in a limited, resources-saving
    //       way. When the user clicks the icon, applicationShouldHandleReopen:
    //       is called, but we didn't call OnInit() yet. In this case, we
    //       shouldn't call MacReopenApp(), but should proceed with normal
    //       initialization.
    return NO;
}

- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event
           withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    wxUnusedVar(replyEvent);
    NSString* url = [[event descriptorAtIndex:1] stringValue];
    wxCFStringRef cf(wxCFRetain(url));
    if ( wxTheApp->OSXInitWasCalled() )
        wxTheApp->MacOpenURL(cf.AsString()) ;
    else
        wxTheApp->OSXStoreOpenURL(cf.AsString());
}

- (void)handleQuitAppEvent:(NSAppleEventDescriptor *)event
            withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    if ( wxTheApp->OSXOnShouldTerminate() )
    {
        wxTheApp->OSXOnWillTerminate();
        wxTheApp->ExitMainLoop();
    }
}

- (void)handleOpenAppEvent:(NSAppleEventDescriptor *)event
           withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    wxUnusedVar(replyEvent);
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


// more on bringing non-bundled apps to the foreground
// https://devforums.apple.com/thread/203753

#if 0 

// one possible solution is also quoted here
// from http://stackoverflow.com/questions/7596643/when-calling-transformprocesstype-the-app-menu-doesnt-show-up

@interface wxNSNonBundledAppHelper : NSObject {
    
}

+ (void)transformToForegroundApplication;

@end

@implementation wxNSNonBundledAppHelper

+ (void)transformToForegroundApplication {
    for (NSRunningApplication * app in [NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.apple.finder"]) {
        [app activateWithOptions:NSApplicationActivateIgnoringOtherApps];
        break;
    }
    [self performSelector:@selector(transformStep2) withObject:nil afterDelay:0.1];
}

+ (void)transformStep2
{
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    (void) TransformProcessType(&psn, kProcessTransformToForegroundApplication);
    
    [self performSelector:@selector(transformStep3) withObject:nil afterDelay:0.1];
}

+ (void)transformStep3
{
    [[NSRunningApplication currentApplication] activateWithOptions:NSApplicationActivateIgnoringOtherApps];
}

@end

#endif

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

- (void) transformToForegroundApplication {
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    TransformProcessType(&psn, kProcessTransformToForegroundApplication);
    
    if ( wxPlatformInfo::Get().CheckOSVersion(10, 9) )
    {
        [[NSRunningApplication currentApplication] activateWithOptions:
         (NSApplicationActivateAllWindows | NSApplicationActivateIgnoringOtherApps)];
    }
    else
    {
        [self deactivate];
        [self activateIgnoringOtherApps:YES];
    }
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
        
        if ( OSXIsGUIApplication() )
        {
            CFURLRef url = CFBundleCopyBundleURL(CFBundleGetMainBundle() ) ;
            CFStringRef path = CFURLCopyFileSystemPath ( url , kCFURLPOSIXPathStyle ) ;
            CFRelease( url ) ;
            wxString app = wxCFStringRef(path).AsString(wxLocale::GetSystemEncoding());
            
            // workaround is only needed for non-bundled apps
            if ( !app.EndsWith(".app") )
            {
                [(wxNSApplication*) [wxNSApplication sharedApplication] transformToForegroundApplication];
            }
        }

        appcontroller = OSXCreateAppController();
        [[NSApplication sharedApplication] setDelegate:(id <NSApplicationDelegate>)appcontroller];
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
    m_inited = false;

    // Feed the upcoming event loop with a dummy event. Without this,
    // [NSApp run] below wouldn't return, as we expect it to, if the
    // application was launched without being activated and would block
    // until the dock icon was clicked - delaying OnInit() call too.
    NSEvent *event = [NSEvent otherEventWithType:NSApplicationDefined
                                    location:NSMakePoint(0.0, 0.0)
                               modifierFlags:0
                                   timestamp:0
                                windowNumber:0
                                     context:nil
                                     subtype:0 data1:0 data2:0];
    [NSApp postEvent:event atStart:FALSE];
    [NSApp run];

    m_onInitResult = OnInit();
    m_inited = true;
    if ( m_onInitResult )
    {
        if ( m_openFiles.GetCount() > 0 )
            MacOpenFiles(m_openFiles);
        else if ( m_printFiles.GetCount() > 0 )
            MacPrintFiles(m_printFiles);
        else if ( m_getURL.Len() > 0 )
            MacOpenURL(m_getURL);
        else
            MacNewFile();
    }
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

    const wxSize bitmapSize(subrect ? subrect->GetSize() : m_window->GetSize());
    wxBitmap bitmap;
    bitmap.CreateScaled(bitmapSize.x, bitmapSize.y, -1, m_contentScaleFactor);

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

            // The bitmap created by wxBitmap::CreateScaled() above is scaled,
            // so we need to adjust the coordinates for it.
            r.size.width /= m_contentScaleFactor;
            r.size.height /= m_contentScaleFactor;

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

