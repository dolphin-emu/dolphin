/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/app.cpp
// Purpose:     wxApp
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/app.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/window.h"
    #include "wx/frame.h"
    #include "wx/dc.h"
    #include "wx/button.h"
    #include "wx/menu.h"
    #include "wx/pen.h"
    #include "wx/brush.h"
    #include "wx/palette.h"
    #include "wx/icon.h"
    #include "wx/cursor.h"
    #include "wx/dialog.h"
    #include "wx/msgdlg.h"
    #include "wx/textctrl.h"
    #include "wx/memory.h"
    #include "wx/gdicmn.h"
    #include "wx/module.h"
#endif

#include "wx/tooltip.h"
#include "wx/docview.h"
#include "wx/filename.h"
#include "wx/link.h"
#include "wx/thread.h"
#include "wx/evtloop.h"

#include <string.h>

// mac
#if wxOSX_USE_CARBON
#include "wx/osx/uma.h"
#else
#include "wx/osx/private.h"
#endif

#if defined(WXMAKINGDLL_CORE)
#   include <mach-o/dyld.h>
#endif

// Keep linker from discarding wxStockGDIMac
wxFORCE_LINK_MODULE(gdiobj)

IMPLEMENT_DYNAMIC_CLASS(wxApp, wxEvtHandler)
BEGIN_EVENT_TABLE(wxApp, wxEvtHandler)
    EVT_IDLE(wxApp::OnIdle)
    EVT_END_SESSION(wxApp::OnEndSession)
    EVT_QUERY_END_SESSION(wxApp::OnQueryEndSession)
END_EVENT_TABLE()


// platform specific static variables
static const short kwxMacAppleMenuId = 1 ;

wxWindow* wxApp::s_captureWindow = NULL ;
long      wxApp::s_lastModifiers = 0 ;

long      wxApp::s_macAboutMenuItemId = wxID_ABOUT ;
long      wxApp::s_macPreferencesMenuItemId = wxID_PREFERENCES ;
long      wxApp::s_macExitMenuItemId = wxID_EXIT ;
wxString  wxApp::s_macHelpMenuTitleName = wxT("&Help") ;

bool      wxApp::sm_isEmbedded = false; // Normally we're not a plugin

#if wxOSX_USE_CARBON

//----------------------------------------------------------------------
// Core Apple Event Support
//----------------------------------------------------------------------

AEEventHandlerUPP sODocHandler = NULL ;
AEEventHandlerUPP sGURLHandler = NULL ;
AEEventHandlerUPP sOAppHandler = NULL ;
AEEventHandlerUPP sPDocHandler = NULL ;
AEEventHandlerUPP sRAppHandler = NULL ;
AEEventHandlerUPP sQuitHandler = NULL ;

pascal OSErr AEHandleODoc( const AppleEvent *event , AppleEvent *reply , SRefCon refcon ) ;
pascal OSErr AEHandleOApp( const AppleEvent *event , AppleEvent *reply , SRefCon refcon ) ;
pascal OSErr AEHandlePDoc( const AppleEvent *event , AppleEvent *reply , SRefCon refcon ) ;
pascal OSErr AEHandleQuit( const AppleEvent *event , AppleEvent *reply , SRefCon refcon ) ;
pascal OSErr AEHandleRApp( const AppleEvent *event , AppleEvent *reply , SRefCon refcon ) ;
pascal OSErr AEHandleGURL( const AppleEvent *event , AppleEvent *reply , SRefCon refcon ) ;

pascal OSErr AEHandleODoc( const AppleEvent *event , AppleEvent *reply , SRefCon WXUNUSED(refcon) )
{
    return wxTheApp->MacHandleAEODoc( (AppleEvent*) event , reply) ;
}

pascal OSErr AEHandleOApp( const AppleEvent *event , AppleEvent *reply , SRefCon WXUNUSED(refcon) )
{
    return wxTheApp->MacHandleAEOApp( (AppleEvent*) event , reply ) ;
}

pascal OSErr AEHandlePDoc( const AppleEvent *event , AppleEvent *reply , SRefCon WXUNUSED(refcon) )
{
    return wxTheApp->MacHandleAEPDoc( (AppleEvent*) event , reply ) ;
}

pascal OSErr AEHandleQuit( const AppleEvent *event , AppleEvent *reply , SRefCon WXUNUSED(refcon) )
{
    return wxTheApp->MacHandleAEQuit( (AppleEvent*) event , reply) ;
}

pascal OSErr AEHandleRApp( const AppleEvent *event , AppleEvent *reply , SRefCon WXUNUSED(refcon) )
{
    return wxTheApp->MacHandleAERApp( (AppleEvent*) event , reply) ;
}

pascal OSErr AEHandleGURL( const AppleEvent *event , AppleEvent *reply , SRefCon WXUNUSED(refcon) )
{
    return wxTheApp->MacHandleAEGURL((WXEVENTREF *)event , reply) ;
}


// AEODoc Calls MacOpenFiles with all of the files passed

short wxApp::MacHandleAEODoc(const WXEVENTREF event, WXEVENTREF WXUNUSED(reply))
{
    AEDescList docList;
    AEKeyword keywd;
    DescType returnedType;
    Size actualSize;
    long itemsInList;
    OSErr err;
    short i;

    err = AEGetParamDesc((AppleEvent *)event, keyDirectObject, typeAEList,&docList);
    if (err != noErr)
        return err;

    err = AECountItems(&docList, &itemsInList);
    if (err != noErr)
        return err;

    ProcessSerialNumber PSN ;
    PSN.highLongOfPSN = 0 ;
    PSN.lowLongOfPSN = kCurrentProcess ;
    SetFrontProcess( &PSN ) ;

    wxString fName ;
    FSRef theRef ;

    wxArrayString fileNames;
    for (i = 1; i <= itemsInList; i++)
    {
        err = AEGetNthPtr(
            &docList, i, typeFSRef, &keywd, &returnedType,
            (Ptr)&theRef, sizeof(theRef), &actualSize);
        
        if ( err != noErr)
            return err;
        
        fName = wxMacFSRefToPath( &theRef ) ;

        fileNames.Add(fName);
    }

    MacOpenFiles(fileNames);

    return noErr;
}

// AEODoc Calls MacOpenURL on the url passed

short wxApp::MacHandleAEGURL(const WXEVENTREF event, WXEVENTREF WXUNUSED(reply))
{
    DescType returnedType;
    Size actualSize;
    char url[255];
    OSErr err = AEGetParamPtr((AppleEvent *)event, keyDirectObject, typeChar,
                              &returnedType, url, sizeof(url)-1,
                              &actualSize);
    if (err != noErr)
        return err;

    url[actualSize] = '\0';    // Terminate the C string

    ProcessSerialNumber PSN ;
    PSN.highLongOfPSN = 0 ;
    PSN.lowLongOfPSN = kCurrentProcess ;
    SetFrontProcess( &PSN ) ;

    MacOpenURL(wxString(url, wxConvUTF8));

    return noErr;
}

// AEPDoc Calls MacPrintFile on each of the files passed

short wxApp::MacHandleAEPDoc(const WXEVENTREF event , WXEVENTREF WXUNUSED(reply))
{
    AEDescList docList;
    AEKeyword keywd;
    DescType returnedType;
    Size actualSize;
    long itemsInList;
    OSErr err;
    short i;

    err = AEGetParamDesc((AppleEvent *)event, keyDirectObject, typeAEList,&docList);
    if (err != noErr)
        return err;

    err = AECountItems(&docList, &itemsInList);
    if (err != noErr)
        return err;

    ProcessSerialNumber PSN ;
    PSN.highLongOfPSN = 0 ;
    PSN.lowLongOfPSN = kCurrentProcess ;
    SetFrontProcess( &PSN ) ;

    wxString fName ;
    FSRef theRef ;
    
    wxArrayString fileNames;

    for (i = 1; i <= itemsInList; i++)
    {
        err = AEGetNthPtr(
            &docList, i, typeFSRef, &keywd, &returnedType,
            (Ptr)&theRef, sizeof(theRef), &actualSize);
        
        if ( err != noErr)
            return err;
        
        fName = wxMacFSRefToPath( &theRef ) ;
        fileNames.Add( fName );
    }
    
    MacPrintFiles(fileNames);

    return noErr;
}

// AEOApp calls MacNewFile

short wxApp::MacHandleAEOApp(const WXEVENTREF WXUNUSED(event) , WXEVENTREF WXUNUSED(reply))
{
    MacNewFile() ;
    return noErr ;
}

// AEQuit attempts to quit the application

short wxApp::MacHandleAEQuit(const WXEVENTREF WXUNUSED(event) , WXEVENTREF WXUNUSED(reply))
{
    wxCloseEvent event;
    wxTheApp->OnQueryEndSession(event);
    if ( !event.GetVeto() )
    {
        wxCloseEvent event;
        wxTheApp->OnEndSession(event);
    }
    return noErr ;
}

// AEROApp calls MacReopenApp

short wxApp::MacHandleAERApp(const WXEVENTREF WXUNUSED(event) , WXEVENTREF WXUNUSED(reply))
{
    MacReopenApp() ;

    return noErr ;
}

#endif

//----------------------------------------------------------------------
// Support Routines linking the Mac...File Calls to the Document Manager
//----------------------------------------------------------------------

void wxApp::MacOpenFiles(const wxArrayString & fileNames )
{
    size_t i;
    const size_t fileCount = fileNames.GetCount();
    for (i = 0; i < fileCount; i++)
    {
        MacOpenFile(fileNames[i]);
    }
}

void wxApp::MacOpenFile(const wxString & fileName )
{
#if wxUSE_DOC_VIEW_ARCHITECTURE
    wxDocManager* dm = wxDocManager::GetDocumentManager() ;
    if ( dm )
        dm->CreateDocument(fileName , wxDOC_SILENT ) ;
#endif
}

void wxApp::MacOpenURL(const wxString & WXUNUSED(url) )
{
}

void wxApp::MacPrintFiles(const wxArrayString & fileNames )
{
    size_t i;
    const size_t fileCount = fileNames.GetCount();
    for (i = 0; i < fileCount; i++)
    {
        MacPrintFile(fileNames[i]);
    }
}

void wxApp::MacPrintFile(const wxString & fileName )
{
#if wxUSE_DOC_VIEW_ARCHITECTURE

#if wxUSE_PRINTING_ARCHITECTURE
    wxDocManager* dm = wxDocManager::GetDocumentManager() ;
    if ( dm )
    {
        wxDocument *doc = dm->CreateDocument(fileName , wxDOC_SILENT ) ;
        if ( doc )
        {
            wxView* view = doc->GetFirstView() ;
            if ( view )
            {
                wxPrintout *printout = view->OnCreatePrintout();
                if (printout)
                {
                    wxPrinter printer;
                    printer.Print(view->GetFrame(), printout, true);
                    delete printout;
                }
            }

            if (doc->Close())
            {
                doc->DeleteAllViews();
                dm->RemoveDocument(doc) ;
            }
        }
    }
#endif //print

#endif //docview
}



void wxApp::MacNewFile()
{
}

void wxApp::MacReopenApp()
{
    // HIG says :
    // if there is no open window -> create a new one
    // if all windows are hidden -> show the first
    // if some windows are not hidden -> do nothing

    wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst();
    if ( !node )
    {
        MacNewFile() ;
    }
    else
    {
        wxTopLevelWindow* firstIconized = NULL ;
        wxTopLevelWindow* firstHidden = NULL ;
        while (node)
        {
            wxTopLevelWindow* win = (wxTopLevelWindow*) node->GetData();
            if ( !win->IsShown() )
            {
                // make sure we don't show 'virtual toplevel windows' like wxTaskBarIconWindow
                if ( firstHidden == NULL && ( wxDynamicCast( win, wxFrame ) || wxDynamicCast( win, wxDialog ) ) )
                   firstHidden = win ;
            }
            else if ( win->IsIconized() )
            {
                if ( firstIconized == NULL )
                    firstIconized = win ;
            }
            else
            {
                // we do have a visible, non-iconized toplevelwindow -> do nothing
                return;
            }

            node = node->GetNext();
        }

        if ( firstIconized )
            firstIconized->Iconize( false ) ;
        
        // showing hidden windows is not really always a good solution, also non-modal dialogs when closed end up
        // as hidden tlws, we don't want to reshow those, so let's just reopen the minimized a.k.a. iconized tlws
        // unless we find a regression ...
#if 0
        else if ( firstHidden )
            firstHidden->Show( true );
#endif
    }
}

#if wxOSX_USE_COCOA_OR_IPHONE
void wxApp::OSXOnWillFinishLaunching()
{
}

void wxApp::OSXOnDidFinishLaunching()
{
}

void wxApp::OSXOnWillTerminate()
{
    wxCloseEvent event;
    event.SetCanVeto(false);
    wxTheApp->OnEndSession(event);
}

bool wxApp::OSXOnShouldTerminate()
{
    wxCloseEvent event;
    wxTheApp->OnQueryEndSession(event);
    return !event.GetVeto();
}
#endif

//----------------------------------------------------------------------
// Macintosh CommandID support - converting between native and wx IDs
//----------------------------------------------------------------------

// if no native match they just return the passed-in id

#if wxOSX_USE_CARBON

struct IdPair
{
    UInt32 macId ;
    int wxId ;
} ;

IdPair gCommandIds [] =
{
    { kHICommandCut ,           wxID_CUT } ,
    { kHICommandCopy ,          wxID_COPY } ,
    { kHICommandPaste ,         wxID_PASTE } ,
    { kHICommandSelectAll ,     wxID_SELECTALL } ,
    { kHICommandClear ,         wxID_CLEAR } ,
    { kHICommandUndo ,          wxID_UNDO } ,
    { kHICommandRedo ,          wxID_REDO } ,
} ;

int wxMacCommandToId( UInt32 macCommandId )
{
    int wxid = 0 ;

    switch ( macCommandId )
    {
        case kHICommandPreferences :
            wxid = wxApp::s_macPreferencesMenuItemId ;
            break ;

        case kHICommandQuit :
            wxid = wxApp::s_macExitMenuItemId ;
            break ;

        case kHICommandAbout :
            wxid = wxApp::s_macAboutMenuItemId ;
            break ;

        default :
            {
                for ( size_t i = 0 ; i < WXSIZEOF(gCommandIds) ; ++i )
                {
                    if ( gCommandIds[i].macId == macCommandId )
                    {
                        wxid = gCommandIds[i].wxId ;
                        break ;
                    }
                }
            }
            break ;
    }

    if ( wxid == 0 )
        wxid = (int) macCommandId ;

    return wxid ;
}

UInt32 wxIdToMacCommand( int wxId )
{
    UInt32 macId = 0 ;

    if ( wxId == wxApp::s_macPreferencesMenuItemId )
        macId = kHICommandPreferences ;
    else if (wxId == wxApp::s_macExitMenuItemId)
        macId = kHICommandQuit ;
    else if (wxId == wxApp::s_macAboutMenuItemId)
        macId = kHICommandAbout ;
    else
    {
        for ( size_t i = 0 ; i < WXSIZEOF(gCommandIds) ; ++i )
        {
            if ( gCommandIds[i].wxId == wxId )
            {
                macId = gCommandIds[i].macId ;
                break ;
            }
        }
    }

    if ( macId == 0 )
        macId = (int) wxId ;

    return macId ;
}

wxMenu* wxFindMenuFromMacCommand( const HICommand &command , wxMenuItem* &item )
{
    wxMenu* itemMenu = NULL ;
#ifndef __WXUNIVERSAL__
    int id = 0 ;

    // for 'standard' commands which don't have a wx-menu
    if ( command.commandID == kHICommandPreferences || command.commandID == kHICommandQuit || command.commandID == kHICommandAbout )
    {
        id = wxMacCommandToId( command.commandID ) ;

        wxMenuBar* mbar = wxMenuBar::MacGetInstalledMenuBar() ;
        if ( mbar )
            item = mbar->FindItem( id , &itemMenu ) ;
    }
    else if ( command.commandID != 0 && command.menu.menuRef != 0 && command.menu.menuItemIndex != 0 )
    {
        id = wxMacCommandToId( command.commandID ) ;
        // make sure it is one of our own menus, or of the 'synthetic' apple and help menus , otherwise don't touch
        MenuItemIndex firstUserHelpMenuItem ;
        static MenuHandle helpMenuHandle = NULL ;
        if ( helpMenuHandle == NULL )
        {
            if ( UMAGetHelpMenuDontCreate( &helpMenuHandle , &firstUserHelpMenuItem) != noErr )
                helpMenuHandle = NULL ;
        }

        // is it part of the application or the Help menu, then look for the id directly
        if ( ( GetMenuHandle( kwxMacAppleMenuId ) != NULL && command.menu.menuRef == GetMenuHandle( kwxMacAppleMenuId ) ) ||
             ( helpMenuHandle != NULL && command.menu.menuRef == helpMenuHandle ) ||
             wxMenuBar::MacGetWindowMenuHMenu() != NULL && command.menu.menuRef == wxMenuBar::MacGetWindowMenuHMenu() )
        {
            wxMenuBar* mbar = wxMenuBar::MacGetInstalledMenuBar() ;
            if ( mbar )
                item = mbar->FindItem( id , &itemMenu ) ;
        }
        else
        {
            URefCon refCon = 0 ;

            GetMenuItemRefCon( command.menu.menuRef , command.menu.menuItemIndex , &refCon ) ;
            itemMenu = wxFindMenuFromMacMenu( command.menu.menuRef ) ;
            if ( itemMenu != NULL && refCon != 0)
                item = (wxMenuItem*) refCon;
        }
    }
#endif
    return itemMenu ;
}

#endif

//----------------------------------------------------------------------
// Carbon Event Handler
//----------------------------------------------------------------------

#if wxOSX_USE_CARBON

static const EventTypeSpec eventList[] =
{
    { kEventClassCommand, kEventProcessCommand } ,
    { kEventClassCommand, kEventCommandUpdateStatus } ,

    { kEventClassMenu, kEventMenuOpening },
    { kEventClassMenu, kEventMenuClosed },
    { kEventClassMenu, kEventMenuTargetItem },

    { kEventClassApplication , kEventAppActivated } ,
    { kEventClassApplication , kEventAppDeactivated } ,
    // handling the quit event is not recommended by apple
    // rather using the quit apple event - which we do

    { kEventClassAppleEvent , kEventAppleEvent } ,

    { kEventClassMouse , kEventMouseDown } ,
    { kEventClassMouse , kEventMouseMoved } ,
    { kEventClassMouse , kEventMouseUp } ,
    { kEventClassMouse , kEventMouseDragged } ,
    { 'WXMC' , 'WXMC' }
} ;

static pascal OSStatus
wxMacAppMenuEventHandler( EventHandlerCallRef WXUNUSED(handler),
                          EventRef event,
                          void *WXUNUSED(data) )
{
    wxMacCarbonEvent cEvent( event ) ;
    MenuRef menuRef = cEvent.GetParameter<MenuRef>(kEventParamDirectObject) ;
#ifndef __WXUNIVERSAL__
    wxMenu* menu = wxFindMenuFromMacMenu( menuRef ) ;

    if ( menu )
    {
        switch (GetEventKind(event))
        {
            case kEventMenuOpening:
                menu->HandleMenuOpened();
                break;

            case kEventMenuClosed:
                menu->HandleMenuClosed();
                break;

            case kEventMenuTargetItem:
                {
                    HICommand command ;

                    command.menu.menuRef = menuRef;
                    command.menu.menuItemIndex = cEvent.GetParameter<MenuItemIndex>(kEventParamMenuItemIndex,typeMenuItemIndex) ;
                    command.commandID = cEvent.GetParameter<MenuCommand>(kEventParamMenuCommand,typeMenuCommand) ;
                    if (command.commandID != 0)
                    {
                        wxMenuItem* item = NULL ;
                        wxMenu* itemMenu = wxFindMenuFromMacCommand( command , item ) ;
                        if ( itemMenu && item )
                            itemMenu->HandleMenuItemHighlighted( item );
                    }
                }
                break;

            default:
                wxFAIL_MSG(wxT("Unexpected menu event kind"));
                break;
        }

    }
#endif
    return eventNotHandledErr;
}

static pascal OSStatus
wxMacAppCommandEventHandler( EventHandlerCallRef WXUNUSED(handler) ,
                             EventRef event ,
                             void *WXUNUSED(data) )
{
    OSStatus result = eventNotHandledErr ;

    HICommand command ;

    wxMacCarbonEvent cEvent( event ) ;
    cEvent.GetParameter<HICommand>(kEventParamDirectObject,typeHICommand,&command) ;

    wxMenuItem* item = NULL ;
    wxMenu* itemMenu = wxFindMenuFromMacCommand( command , item ) ;

    if ( item )
    {
        wxASSERT( itemMenu != NULL ) ;

        switch ( cEvent.GetKind() )
        {
            case kEventProcessCommand :
                if ( itemMenu->HandleCommandProcess( item ) )
                    result = noErr;
            break ;

        case kEventCommandUpdateStatus:
            if ( itemMenu->HandleCommandUpdateStatus( item ) )
                    result = noErr;
            break ;

        default :
            break ;
        }
    }
    return result ;
}

static pascal OSStatus
wxMacAppApplicationEventHandler( EventHandlerCallRef WXUNUSED(handler) ,
                                 EventRef event ,
                                 void *WXUNUSED(data) )
{
    OSStatus result = eventNotHandledErr ;
    switch ( GetEventKind( event ) )
    {
        case kEventAppActivated :
            if ( wxTheApp )
                wxTheApp->SetActive( true , NULL ) ;
            result = noErr ;
            break ;

        case kEventAppDeactivated :
            if ( wxTheApp )
                wxTheApp->SetActive( false , NULL ) ;
            result = noErr ;
            break ;

        default :
            break ;
    }

    return result ;
}

pascal OSStatus wxMacAppEventHandler( EventHandlerCallRef handler , EventRef event , void *data )
{
    EventRef formerEvent = (EventRef) wxTheApp->MacGetCurrentEvent() ;
    EventHandlerCallRef formerEventHandlerCallRef = (EventHandlerCallRef) wxTheApp->MacGetCurrentEventHandlerCallRef() ;
    wxTheApp->MacSetCurrentEvent( event , handler ) ;

    OSStatus result = eventNotHandledErr ;
    switch ( GetEventClass( event ) )
    {
#ifndef __LP64__
        case kEventClassCommand :
            result = wxMacAppCommandEventHandler( handler , event , data ) ;
            break ;
#endif
        case kEventClassApplication :
            result = wxMacAppApplicationEventHandler( handler , event , data ) ;
            break ;
#ifndef __LP64__
        case kEventClassMenu :
            result = wxMacAppMenuEventHandler( handler , event , data ) ;
            break ;

        case kEventClassMouse :
            {
                wxMacCarbonEvent cEvent( event ) ;

                WindowRef window ;
                Point screenMouseLocation = cEvent.GetParameter<Point>(kEventParamMouseLocation) ;
                ::FindWindow(screenMouseLocation, &window);
                // only send this event in case it had not already been sent to a tlw, as we get
                // double events otherwise (in case event.skip) was called
                if ( window == NULL )
                    result = wxMacTopLevelMouseEventHandler( handler , event , NULL ) ;
            }
            break ;
#endif
        case kEventClassAppleEvent :
            result = AEProcessEvent(event);
            break ;

        default :
            break ;
    }

    wxTheApp->MacSetCurrentEvent( formerEvent, formerEventHandlerCallRef ) ;

    return result ;
}

DEFINE_ONE_SHOT_HANDLER_GETTER( wxMacAppEventHandler )
#endif

#if wxDEBUG_LEVEL && wxOSX_USE_COCOA_OR_CARBON

pascal static void
wxMacAssertOutputHandler(OSType WXUNUSED(componentSignature),
                         UInt32 WXUNUSED(options),
                         const char *assertionString,
                         const char *exceptionLabelString,
                         const char *errorString,
                         const char *fileName,
                         long lineNumber,
                         void *value,
                         ConstStr255Param WXUNUSED(outputMsg))
{
    // flow into assert handling
    wxString fileNameStr ;
    wxString assertionStr ;
    wxString exceptionStr ;
    wxString errorStr ;

#if wxUSE_UNICODE
    fileNameStr = wxString(fileName, wxConvLocal);
    assertionStr = wxString(assertionString, wxConvLocal);
    exceptionStr = wxString((exceptionLabelString!=0) ? exceptionLabelString : "", wxConvLocal) ;
    errorStr = wxString((errorString!=0) ? errorString : "", wxConvLocal) ;
#else
    fileNameStr = fileName;
    assertionStr = assertionString;
    exceptionStr = (exceptionLabelString!=0) ? exceptionLabelString : "" ;
    errorStr = (errorString!=0) ? errorString : "" ;
#endif

#if 1
    // flow into log
    wxLogDebug( wxT("AssertMacros: %s %s %s file: %s, line: %ld (value %p)\n"),
        assertionStr.c_str() ,
        exceptionStr.c_str() ,
        errorStr.c_str(),
        fileNameStr.c_str(), lineNumber ,
        value ) ;
#else

    wxOnAssert(fileNameStr, lineNumber , assertionStr ,
        wxString::Format( wxT("%s %s value (%p)") , exceptionStr, errorStr , value ) ) ;
#endif
}

#endif // wxDEBUG_LEVEL

bool wxApp::Initialize(int& argc, wxChar **argv)
{
    // Mac-specific

#if wxDEBUG_LEVEL && wxOSX_USE_COCOA_OR_CARBON
    InstallDebugAssertOutputHandler( NewDebugAssertOutputHandlerUPP( wxMacAssertOutputHandler ) );
#endif

    /*
     Cocoa supports -Key value options which set the user defaults key "Key"
     to the value "value"  Some of them are very handy for debugging like
     -NSShowAllViews YES.  Cocoa picks these up from the real argv so
     our removal of them from the wx copy of it does not affect Cocoa's
     ability to see them.
     
     We basically just assume that any "-NS" option and its following
     argument needs to be removed from argv.  We hope that user code does
     not expect to see -NS options and indeed it's probably a safe bet
     since most user code accepting options is probably using the
     double-dash GNU-style syntax.
     */
    for(int i=1; i < argc; ++i)
    {
        static const wxChar *ARG_NS = wxT("-NS");
        if( wxStrncmp(argv[i], ARG_NS, wxStrlen(ARG_NS)) == 0 )
        {
            // Only eat this option if it has an argument
            if( (i + 1) < argc )
            {
                memmove(argv + i, argv + i + 2, (argc-i-1)*sizeof(wxChar*));
                argc -= 2;
                // drop back one position so the next run through the loop
                // reprocesses the argument at our current index.
                --i;
            }
        }
    }

    if ( !wxAppBase::Initialize(argc, argv) )
        return false;

#if wxUSE_INTL
    wxFont::SetDefaultEncoding(wxLocale::GetSystemEncoding());
#endif

    // these might be the startup dirs, set them to the 'usual' dir containing the app bundle
    wxString startupCwd = wxGetCwd() ;
    if ( startupCwd == wxT("/") || startupCwd.Right(15) == wxT("/Contents/MacOS") )
    {
        CFURLRef url = CFBundleCopyBundleURL(CFBundleGetMainBundle() ) ;
        CFURLRef urlParent = CFURLCreateCopyDeletingLastPathComponent( kCFAllocatorDefault , url ) ;
        CFRelease( url ) ;
        CFStringRef path = CFURLCopyFileSystemPath ( urlParent , kCFURLPOSIXPathStyle ) ;
        CFRelease( urlParent ) ;
        wxString cwd = wxCFStringRef(path).AsString(wxLocale::GetSystemEncoding());
        wxSetWorkingDirectory( cwd ) ;
    }

    return true;
}

#if wxOSX_USE_CARBON
bool wxApp::CallOnInit()
{
    wxMacAutoreleasePool autoreleasepool;
    return OnInit();
}
#endif

bool wxApp::OnInitGui()
{
    if ( !wxAppBase::OnInitGui() )
        return false ;

    if ( !DoInitGui() )
        return false;

    return true ;
}

bool wxApp::ProcessIdle()
{
    wxMacAutoreleasePool autoreleasepool;
    return wxAppBase::ProcessIdle();
}

int wxApp::OnRun()
{
    wxMacAutoreleasePool pool;
    return wxAppBase::OnRun();
}

#if wxOSX_USE_CARBON
bool wxApp::DoInitGui()
{
    InstallStandardEventHandler( GetApplicationEventTarget() ) ;
    if (!sm_isEmbedded)
    {
        InstallApplicationEventHandler(
            GetwxMacAppEventHandlerUPP(),
            GetEventTypeCount(eventList), eventList, wxTheApp, (EventHandlerRef *)&(wxTheApp->m_macEventHandler));
    }

    if (!sm_isEmbedded)
    {
        sODocHandler = NewAEEventHandlerUPP(AEHandleODoc) ;
        sGURLHandler = NewAEEventHandlerUPP(AEHandleGURL) ;
        sOAppHandler = NewAEEventHandlerUPP(AEHandleOApp) ;
        sPDocHandler = NewAEEventHandlerUPP(AEHandlePDoc) ;
        sRAppHandler = NewAEEventHandlerUPP(AEHandleRApp) ;
        sQuitHandler = NewAEEventHandlerUPP(AEHandleQuit) ;

        AEInstallEventHandler( kCoreEventClass , kAEOpenDocuments ,
                               sODocHandler , 0 , FALSE ) ;
        AEInstallEventHandler( kInternetEventClass, kAEGetURL,
                               sGURLHandler , 0 , FALSE ) ;
        AEInstallEventHandler( kCoreEventClass , kAEOpenApplication ,
                               sOAppHandler , 0 , FALSE ) ;
        AEInstallEventHandler( kCoreEventClass , kAEPrintDocuments ,
                               sPDocHandler , 0 , FALSE ) ;
        AEInstallEventHandler( kCoreEventClass , kAEReopenApplication ,
                               sRAppHandler , 0 , FALSE ) ;
        AEInstallEventHandler( kCoreEventClass , kAEQuitApplication ,
                               sQuitHandler , 0 , FALSE ) ;
    }

    if ( !wxMacInitCocoa() )
        return false;

    return true;
}

void wxApp::DoCleanUp()
{
    if (!sm_isEmbedded)
        RemoveEventHandler( (EventHandlerRef)(wxTheApp->m_macEventHandler) );

    if (!sm_isEmbedded)
    {
        AERemoveEventHandler( kCoreEventClass , kAEOpenDocuments ,
                               sODocHandler , FALSE ) ;
        AERemoveEventHandler( kInternetEventClass, kAEGetURL,
                               sGURLHandler , FALSE ) ;
        AERemoveEventHandler( kCoreEventClass , kAEOpenApplication ,
                               sOAppHandler , FALSE ) ;
        AERemoveEventHandler( kCoreEventClass , kAEPrintDocuments ,
                               sPDocHandler , FALSE ) ;
        AERemoveEventHandler( kCoreEventClass , kAEReopenApplication ,
                               sRAppHandler , FALSE ) ;
        AERemoveEventHandler( kCoreEventClass , kAEQuitApplication ,
                               sQuitHandler , FALSE ) ;

        DisposeAEEventHandlerUPP( sODocHandler ) ;
        DisposeAEEventHandlerUPP( sGURLHandler ) ;
        DisposeAEEventHandlerUPP( sOAppHandler ) ;
        DisposeAEEventHandlerUPP( sPDocHandler ) ;
        DisposeAEEventHandlerUPP( sRAppHandler ) ;
        DisposeAEEventHandlerUPP( sQuitHandler ) ;
    }
}

#endif

void wxApp::CleanUp()
{
    wxMacAutoreleasePool autoreleasepool;
#if wxUSE_TOOLTIPS
    wxToolTip::RemoveToolTips() ;
#endif

    DoCleanUp();

    wxAppBase::CleanUp();
}

//----------------------------------------------------------------------
// misc initialization stuff
//----------------------------------------------------------------------

wxApp::wxApp()
{
    m_printMode = wxPRINT_WINDOWS;

    m_macCurrentEvent = NULL ;
    m_macCurrentEventHandlerCallRef = NULL ;
    m_macPool = new wxMacAutoreleasePool();
}

wxApp::~wxApp()
{
    if (m_macPool)
        delete m_macPool;
}

CFMutableArrayRef GetAutoReleaseArray()
{
    static CFMutableArrayRef array = 0;
    if ( array == 0)
        array= CFArrayCreateMutable(kCFAllocatorDefault,0,&kCFTypeArrayCallBacks);
    return array;
}

void wxApp::MacAddToAutorelease( void* cfrefobj )
{
    CFArrayAppendValue( GetAutoReleaseArray(), cfrefobj );
}

void wxApp::MacReleaseAutoreleasePool()
{
    if (m_macPool)
        delete m_macPool;
    m_macPool = new wxMacAutoreleasePool();
}

void wxApp::OnIdle(wxIdleEvent& WXUNUSED(event))
{
    // If they are pending events, we must process them: pending events are
    // either events to the threads other than main or events posted with
    // wxPostEvent() functions
#ifndef __WXUNIVERSAL__
#if wxUSE_MENUS
  if (!wxMenuBar::MacGetInstalledMenuBar() && wxMenuBar::MacGetCommonMenuBar())
    wxMenuBar::MacGetCommonMenuBar()->MacInstallMenuBar();
#endif
#endif
    CFArrayRemoveAllValues( GetAutoReleaseArray() );
}

void wxApp::WakeUpIdle()
{
    wxEventLoopBase * const loop = wxEventLoopBase::GetActive();

    if ( loop )
        loop->WakeUp();
}

void wxApp::OnEndSession(wxCloseEvent& WXUNUSED(event))
{
    if (GetTopWindow())
        GetTopWindow()->Close(true);
}

// Default behaviour: close the application with prompts. The
// user can veto the close, and therefore the end session.
void wxApp::OnQueryEndSession(wxCloseEvent& event)
{
    if ( !wxDialog::OSXHasModalDialogsOpen() )
    {
        if (GetTopWindow())
        {
            if (!GetTopWindow()->Close(!event.CanVeto()))
                event.Veto(true);
        }
    }
    else
    {
        event.Veto(true);
    }
}

extern "C" void wxCYield() ;
void wxCYield()
{
    wxYield() ;
}

// virtual
void wxApp::MacHandleUnhandledEvent( WXEVENTREF WXUNUSED(evr) )
{
    // Override to process unhandled events as you please
}

#if wxOSX_USE_COCOA_OR_CARBON

CGKeyCode wxCharCodeWXToOSX(wxKeyCode code)
{
    CGKeyCode keycode;
    
    switch (code)
    {
        // Clang warns about switch values not of the same type as (enumerated)
        // switch controlling expression. This is generally useful but here we
        // really want to be able to use letters and digits without making them
        // part of wxKeyCode enum.
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wswitch"
#endif // __clang__

        case 'a': case 'A':   keycode = kVK_ANSI_A; break;
        case 'b': case 'B':   keycode = kVK_ANSI_B; break;
        case 'c': case 'C':   keycode = kVK_ANSI_C; break;
        case 'd': case 'D':   keycode = kVK_ANSI_D; break;
        case 'e': case 'E':   keycode = kVK_ANSI_E; break;
        case 'f': case 'F':   keycode = kVK_ANSI_F; break;
        case 'g': case 'G':   keycode = kVK_ANSI_G; break;
        case 'h': case 'H':   keycode = kVK_ANSI_H; break;
        case 'i': case 'I':   keycode = kVK_ANSI_I; break;
        case 'j': case 'J':   keycode = kVK_ANSI_J; break;
        case 'k': case 'K':   keycode = kVK_ANSI_K; break;
        case 'l': case 'L':   keycode = kVK_ANSI_L; break;
        case 'm': case 'M':   keycode = kVK_ANSI_M; break;
        case 'n': case 'N':   keycode = kVK_ANSI_N; break;
        case 'o': case 'O':   keycode = kVK_ANSI_O; break;
        case 'p': case 'P':   keycode = kVK_ANSI_P; break;
        case 'q': case 'Q':   keycode = kVK_ANSI_Q; break;
        case 'r': case 'R':   keycode = kVK_ANSI_R; break;
        case 's': case 'S':   keycode = kVK_ANSI_S; break;
        case 't': case 'T':   keycode = kVK_ANSI_T; break;
        case 'u': case 'U':   keycode = kVK_ANSI_U; break;
        case 'v': case 'V':   keycode = kVK_ANSI_V; break;
        case 'w': case 'W':   keycode = kVK_ANSI_W; break;
        case 'x': case 'X':   keycode = kVK_ANSI_X; break;
        case 'y': case 'Y':   keycode = kVK_ANSI_Y; break;
        case 'z': case 'Z':   keycode = kVK_ANSI_Z; break;
            
        case '0':             keycode = kVK_ANSI_0; break;
        case '1':             keycode = kVK_ANSI_1; break;
        case '2':             keycode = kVK_ANSI_2; break;
        case '3':             keycode = kVK_ANSI_3; break;
        case '4':             keycode = kVK_ANSI_4; break;
        case '5':             keycode = kVK_ANSI_5; break;
        case '6':             keycode = kVK_ANSI_6; break;
        case '7':             keycode = kVK_ANSI_7; break;
        case '8':             keycode = kVK_ANSI_8; break;
        case '9':             keycode = kVK_ANSI_9; break;

#ifdef __clang__
    #pragma clang diagnostic pop
#endif // __clang__
            
        case WXK_BACK:        keycode = kVK_Delete; break;
        case WXK_TAB:         keycode = kVK_Tab; break;
        case WXK_RETURN:      keycode = kVK_Return; break;
        case WXK_ESCAPE:      keycode = kVK_Escape; break;
        case WXK_SPACE:       keycode = kVK_Space; break;
        case WXK_DELETE:      keycode = kVK_ForwardDelete; break;
            
        case WXK_SHIFT:       keycode = kVK_Shift; break;
        case WXK_ALT:         keycode = kVK_Option; break;
        case WXK_RAW_CONTROL: keycode = kVK_Control; break;
        case WXK_CONTROL:     keycode = kVK_Command; break;
            
        case WXK_CAPITAL:     keycode = kVK_CapsLock; break;
        case WXK_END:         keycode = kVK_End; break;
        case WXK_HOME:        keycode = kVK_Home; break;
        case WXK_LEFT:        keycode = kVK_LeftArrow; break;
        case WXK_UP:          keycode = kVK_UpArrow; break;
        case WXK_RIGHT:       keycode = kVK_RightArrow; break;
        case WXK_DOWN:        keycode = kVK_DownArrow; break;
            
        case WXK_HELP:        keycode = kVK_Help; break;
            
            
        case WXK_NUMPAD0:     keycode = kVK_ANSI_Keypad0; break;
        case WXK_NUMPAD1:     keycode = kVK_ANSI_Keypad1; break;
        case WXK_NUMPAD2:     keycode = kVK_ANSI_Keypad2; break;
        case WXK_NUMPAD3:     keycode = kVK_ANSI_Keypad3; break;
        case WXK_NUMPAD4:     keycode = kVK_ANSI_Keypad4; break;
        case WXK_NUMPAD5:     keycode = kVK_ANSI_Keypad5; break;
        case WXK_NUMPAD6:     keycode = kVK_ANSI_Keypad6; break;
        case WXK_NUMPAD7:     keycode = kVK_ANSI_Keypad7; break;
        case WXK_NUMPAD8:     keycode = kVK_ANSI_Keypad8; break;
        case WXK_NUMPAD9:     keycode = kVK_ANSI_Keypad9; break;
        case WXK_F1:          keycode = kVK_F1; break;
        case WXK_F2:          keycode = kVK_F2; break;
        case WXK_F3:          keycode = kVK_F3; break;
        case WXK_F4:          keycode = kVK_F4; break;
        case WXK_F5:          keycode = kVK_F5; break;
        case WXK_F6:          keycode = kVK_F6; break;
        case WXK_F7:          keycode = kVK_F7; break;
        case WXK_F8:          keycode = kVK_F8; break;
        case WXK_F9:          keycode = kVK_F9; break;
        case WXK_F10:         keycode = kVK_F10; break;
        case WXK_F11:         keycode = kVK_F11; break;
        case WXK_F12:         keycode = kVK_F12; break;
        case WXK_F13:         keycode = kVK_F13; break;
        case WXK_F14:         keycode = kVK_F14; break;
        case WXK_F15:         keycode = kVK_F15; break;
        case WXK_F16:         keycode = kVK_F16; break;
        case WXK_F17:         keycode = kVK_F17; break;
        case WXK_F18:         keycode = kVK_F18; break;
        case WXK_F19:         keycode = kVK_F19; break;
        case WXK_F20:         keycode = kVK_F20; break;
            
        case WXK_PAGEUP:      keycode = kVK_PageUp; break;
        case WXK_PAGEDOWN:    keycode = kVK_PageDown; break;
            
        case WXK_NUMPAD_DELETE:    keycode = kVK_ANSI_KeypadClear; break;
        case WXK_NUMPAD_EQUAL:     keycode = kVK_ANSI_KeypadEquals; break;
        case WXK_NUMPAD_MULTIPLY:  keycode = kVK_ANSI_KeypadMultiply; break;
        case WXK_NUMPAD_ADD:       keycode = kVK_ANSI_KeypadPlus; break;
        case WXK_NUMPAD_SUBTRACT:  keycode = kVK_ANSI_KeypadMinus; break;
        case WXK_NUMPAD_DECIMAL:   keycode = kVK_ANSI_KeypadDecimal; break;
        case WXK_NUMPAD_DIVIDE:    keycode = kVK_ANSI_KeypadDivide; break;
            
        default:
            wxLogDebug( "Unrecognised keycode %d", code );
            keycode = static_cast<CGKeyCode>(-1);
    }
    
    return keycode;
}

long wxMacTranslateKey(unsigned char key, unsigned char code)
{
    long retval = key ;
    switch (key)
    {
        case kHomeCharCode :
            retval = WXK_HOME;
            break;

        case kEnterCharCode :
            retval = WXK_RETURN;
            break;
        case kEndCharCode :
            retval = WXK_END;
            break;

        case kHelpCharCode :
            retval = WXK_HELP;
            break;

        case kBackspaceCharCode :
            retval = WXK_BACK;
            break;

        case kTabCharCode :
            retval = WXK_TAB;
            break;

        case kPageUpCharCode :
            retval = WXK_PAGEUP;
            break;

        case kPageDownCharCode :
            retval = WXK_PAGEDOWN;
            break;

        case kReturnCharCode :
            retval = WXK_RETURN;
            break;

        case kFunctionKeyCharCode :
        {
            switch ( code )
            {
                case 0x7a :
                    retval = WXK_F1 ;
                    break;

                case 0x78 :
                    retval = WXK_F2 ;
                    break;

                case 0x63 :
                    retval = WXK_F3 ;
                    break;

                case 0x76 :
                    retval = WXK_F4 ;
                    break;

                case 0x60 :
                    retval = WXK_F5 ;
                    break;

                case 0x61 :
                    retval = WXK_F6 ;
                    break;

                case 0x62:
                    retval = WXK_F7 ;
                    break;

                case 0x64 :
                    retval = WXK_F8 ;
                    break;

                case 0x65 :
                    retval = WXK_F9 ;
                    break;

                case 0x6D :
                    retval = WXK_F10 ;
                    break;

                case 0x67 :
                    retval = WXK_F11 ;
                    break;

                case 0x6F :
                    retval = WXK_F12 ;
                    break;

                case 0x69 :
                    retval = WXK_F13 ;
                    break;

                case 0x6B :
                    retval = WXK_F14 ;
                    break;

                case 0x71 :
                    retval = WXK_F15 ;
                    break;

                default:
                    break;
            }
        }
        break ;

        case kEscapeCharCode :
            retval = WXK_ESCAPE ;
            break ;

        case kLeftArrowCharCode :
            retval = WXK_LEFT ;
            break ;

        case kRightArrowCharCode :
            retval = WXK_RIGHT ;
            break ;

        case kUpArrowCharCode :
            retval = WXK_UP ;
            break ;

        case kDownArrowCharCode :
            retval = WXK_DOWN ;
            break ;

        case kDeleteCharCode :
            retval = WXK_DELETE ;
            break ;

        default:
            break ;
     } // end switch

    return retval;
}

int wxMacKeyCodeToModifier(wxKeyCode key)
{
    switch (key)
    {
    case WXK_START:
    case WXK_MENU:
    case WXK_COMMAND:
        return cmdKey;

    case WXK_SHIFT:
        return shiftKey;

    case WXK_CAPITAL:
        return alphaLock;

    case WXK_ALT:
        return optionKey;

    case WXK_RAW_CONTROL:
        return controlKey;

    default:
        return 0;
    }
}
#endif

#if wxOSX_USE_COCOA && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_6

// defined in utils.mm

#elif wxOSX_USE_COCOA_OR_CARBON

wxMouseState wxGetMouseState()
{
    wxMouseState ms;

    wxPoint pt = wxGetMousePosition();
    ms.SetX(pt.x);
    ms.SetY(pt.y);

    UInt32 buttons = GetCurrentButtonState();
    ms.SetLeftDown( (buttons & 0x01) != 0 );
    ms.SetMiddleDown( (buttons & 0x04) != 0 );
    ms.SetRightDown( (buttons & 0x02) != 0 );

    UInt32 modifiers = GetCurrentKeyModifiers();
    ms.SetRawControlDown(modifiers & controlKey);
    ms.SetShiftDown(modifiers & shiftKey);
    ms.SetAltDown(modifiers & optionKey);
    ms.SetControlDown(modifiers & cmdKey);

    return ms;
}

#endif

// TODO : once the new key/char handling is tested, move all the code to wxWindow

bool wxApp::MacSendKeyDownEvent( wxWindow* focus , long keymessage , long modifiers , long when , wxChar uniChar )
{
    if ( !focus )
        return false ;

    wxKeyEvent event(wxEVT_KEY_DOWN) ;
    MacCreateKeyEvent( event, focus , keymessage , modifiers , when , uniChar ) ;

    return focus->OSXHandleKeyEvent(event);
}

bool wxApp::MacSendKeyUpEvent( wxWindow* focus , long keymessage , long modifiers , long when , wxChar uniChar )
{
    if ( !focus )
        return false ;

    wxKeyEvent event( wxEVT_KEY_UP ) ;
    MacCreateKeyEvent( event, focus , keymessage , modifiers , when , uniChar ) ;

    return focus->OSXHandleKeyEvent(event) ;
}

bool wxApp::MacSendCharEvent( wxWindow* focus , long keymessage , long modifiers , long when , wxChar uniChar )
{
    if ( !focus )
        return false ;
    wxKeyEvent event(wxEVT_CHAR) ;
    MacCreateKeyEvent( event, focus , keymessage , modifiers , when , uniChar ) ;

    bool handled = false ;

#if wxOSX_USE_CARBON
    long keyval = event.m_keyCode ;

    {
        wxKeyEvent eventCharHook(wxEVT_CHAR_HOOK, event);
        handled = focus->HandleWindowEvent( eventCharHook );
        if ( handled && eventCharHook.IsNextEventAllowed() )
            handled = false ;
    }

    if ( !handled )
    {
        handled = focus->HandleWindowEvent( event ) ;
    }

    if ( !handled && (keyval == WXK_TAB) )
    {
        wxWindow* iter = focus->GetParent() ;
        while ( iter && !handled )
        {
            if ( iter->HasFlag( wxTAB_TRAVERSAL ) )
            {
                wxNavigationKeyEvent new_event;
                new_event.SetEventObject( focus );
                new_event.SetDirection( !event.ShiftDown() );
                /* CTRL-TAB changes the (parent) window, i.e. switch notebook page */
                new_event.SetWindowChange( event.ControlDown() );
                new_event.SetCurrentFocus( focus );
                handled = focus->GetParent()->HandleWindowEvent( new_event );
                if ( handled && new_event.GetSkipped() )
                    handled = false ;
            }

            iter = iter->GetParent() ;
        }
    }

    // backdoor handler for default return and command escape
    if ( !handled && (!focus->IsKindOf(CLASSINFO(wxControl) ) || !focus->AcceptsFocus() ) )
    {
        // if window is not having a focus still testing for default enter or cancel
        // TODO: add the UMA version for ActiveNonFloatingWindow
#ifndef __LP64__
        wxWindow* focus = wxNonOwnedWindow::GetFromWXWindow( (WXWindow) FrontWindow() ) ;
        if ( focus )
        {
            if ( keyval == WXK_RETURN || keyval == WXK_NUMPAD_ENTER )
            {
                wxTopLevelWindow *tlw = wxDynamicCast(wxGetTopLevelParent(focus), wxTopLevelWindow);
                if ( tlw && tlw->GetDefaultItem() )
                {
                    wxButton *def = wxDynamicCast(tlw->GetDefaultItem(), wxButton);
                    if ( def && def->IsEnabled() )
                    {
                        wxCommandEvent event(wxEVT_BUTTON, def->GetId() );
                        event.SetEventObject(def);
                        def->Command(event);

                        return true ;
                    }
                }
            }
            else if (keyval == WXK_ESCAPE || (keyval == '.' && modifiers & cmdKey ) )
            {
                // generate wxID_CANCEL if command-. or <esc> has been pressed (typically in dialogs)
                wxCommandEvent new_event(wxEVT_BUTTON,wxID_CANCEL);
                new_event.SetEventObject( focus );
                handled = focus->HandleWindowEvent( new_event );
            }
        }
#endif
    }
#endif
    return handled ;
}

// This method handles common code for SendKeyDown, SendKeyUp, and SendChar events.
void wxApp::MacCreateKeyEvent( wxKeyEvent& event, wxWindow* focus , long keymessage , long modifiers , long when , wxChar uniChar )
{
#if wxOSX_USE_COCOA_OR_CARBON
    
    short keycode, keychar ;

    keychar = short(keymessage & charCodeMask);
    keycode = short(keymessage & keyCodeMask) >> 8 ;
    if ( !(event.GetEventType() == wxEVT_CHAR) && (modifiers & (controlKey | shiftKey | optionKey) ) )
    {
        // control interferes with some built-in keys like pgdown, return etc. therefore we remove the controlKey modifier
        // and look at the character after
#ifdef __LP64__
        // TODO new implementation using TextInputSources
#else
        UInt32 state = 0;
        UInt32 keyInfo = KeyTranslate((Ptr)GetScriptManagerVariable(smKCHRCache), ( modifiers & (~(controlKey | shiftKey | optionKey))) | keycode, &state);
        keychar = short(keyInfo & charCodeMask);
#endif
    }

    long keyval = wxMacTranslateKey(keychar, keycode) ;
    if ( keyval == keychar && ( event.GetEventType() == wxEVT_KEY_UP || event.GetEventType() == wxEVT_KEY_DOWN ) )
        keyval = wxToupper( keyval ) ;

    // Check for NUMPAD keys.  For KEY_UP/DOWN events we need to use the
    // WXK_NUMPAD constants, but for the CHAR event we want to use the
    // standard ascii values
    if ( event.GetEventType() != wxEVT_CHAR )
    {
        if (keyval >= '0' && keyval <= '9' && keycode >= 82 && keycode <= 92)
        {
            keyval = (keyval - '0') + WXK_NUMPAD0;
        }
        else if (keycode >= 65 && keycode <= 81)
        {
            switch (keycode)
            {
                case 76 :
                    keyval = WXK_NUMPAD_ENTER;
                    break;

                case 81:
                    keyval = WXK_NUMPAD_EQUAL;
                    break;

                case 67:
                    keyval = WXK_NUMPAD_MULTIPLY;
                    break;

                case 75:
                    keyval = WXK_NUMPAD_DIVIDE;
                    break;

                case 78:
                    keyval = WXK_NUMPAD_SUBTRACT;
                    break;

                case 69:
                    keyval = WXK_NUMPAD_ADD;
                    break;

                case 65:
                    keyval = WXK_NUMPAD_DECIMAL;
                    break;
                default:
                    break;
            }
        }
    }

    event.m_shiftDown = modifiers & shiftKey;
    event.m_rawControlDown = modifiers & controlKey;
    event.m_altDown = modifiers & optionKey;
    event.m_controlDown = modifiers & cmdKey;
    event.m_keyCode = keyval ;
#if wxUSE_UNICODE
    event.m_uniChar = uniChar ;
#endif

    event.m_rawCode = keymessage;
    event.m_rawFlags = modifiers;
    event.SetTimestamp(when);
    event.SetEventObject(focus);
#else
    wxUnusedVar(event);
    wxUnusedVar(focus);
    wxUnusedVar(keymessage);
    wxUnusedVar(modifiers);
    wxUnusedVar(when);
    wxUnusedVar(uniChar);
#endif
}


void wxApp::MacHideApp()
{
#if wxOSX_USE_CARBON
    wxMacCarbonEvent event( kEventClassCommand , kEventCommandProcess );
    HICommand command;
    memset( &command, 0 , sizeof(command) );
    command.commandID = kHICommandHide ;
    event.SetParameter<HICommand>(kEventParamDirectObject, command );
    SendEventToApplication( event );
#endif
}
