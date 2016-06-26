/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/webview_webkit.mm
// Purpose:     wxWebViewWebKit - embeddable web kit control,
//                             OS X implementation of web view component
// Author:      Jethro Grassie / Kevin Ollivier / Marianne Gagnon
// Modified by:
// Created:     2004-4-16
// Copyright:   (c) Jethro Grassie / Kevin Ollivier / Marianne Gagnon
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// http://developer.apple.com/mac/library/documentation/Cocoa/Reference/WebKit/Classes/WebView_Class/Reference/Reference.html

#include "wx/osx/webview_webkit.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_WEBKIT && defined(__WXOSX__)

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/osx/private.h"
#include "wx/osx/core/cfref.h"

#include "wx/hashmap.h"
#include "wx/filesys.h"

#if wxOSX_USE_IPHONE
#include <UIKit/UIWebView.h>
#else
#include <WebKit/WebKit.h>
#include <WebKit/HIWebView.h>
#include <WebKit/CarbonUtils.h>
#endif
#include <Foundation/NSURLError.h>

// using native types to get compile errors and warnings

#define DEBUG_WEBKIT_SIZING 0

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxWebViewWebKit, wxWebView);

wxBEGIN_EVENT_TABLE(wxWebViewWebKit, wxControl)
wxEND_EVENT_TABLE()

@interface WebViewLoadDelegate : NSObject
{
    wxWebViewWebKit* webKitWindow;
}

- (id)initWithWxWindow: (wxWebViewWebKit*)inWindow;

@end

@interface WebViewPolicyDelegate : NSObject
{
    wxWebViewWebKit* webKitWindow;
}

- (id)initWithWxWindow: (wxWebViewWebKit*)inWindow;

@end

@interface WebViewUIDelegate : NSObject
{
    wxWebViewWebKit* webKitWindow;
}

- (id)initWithWxWindow: (wxWebViewWebKit*)inWindow;

@end

//We use a hash to map scheme names to wxWebViewHandler
WX_DECLARE_STRING_HASH_MAP(wxSharedPtr<wxWebViewHandler>, wxStringToWebHandlerMap);

static wxStringToWebHandlerMap g_stringHandlerMap;

@interface WebViewCustomProtocol : NSURLProtocol
{
}
@end

// ----------------------------------------------------------------------------
// creation/destruction
// ----------------------------------------------------------------------------

bool wxWebViewWebKit::Create(wxWindow *parent,
                                 wxWindowID winID,
                                 const wxString& strURL,
                                 const wxPoint& pos,
                                 const wxSize& size, long style,
                                 const wxString& name)
{
    m_busy = false;
    m_nextNavigationIsNewWindow = false;

    DontCreatePeer();
    wxControl::Create(parent, winID, pos, size, style, wxDefaultValidator, name);

#if wxOSX_USE_IPHONE
    CGRect r = wxOSXGetFrameForControl( this, pos , size ) ;
    m_webView = [[UIWebView alloc] initWithFrame:r];

    SetPeer( new wxWidgetIPhoneImpl( this, m_webView ) );

#else
    NSRect r = wxOSXGetFrameForControl( this, pos , size ) ;
    m_webView = [[WebView alloc] initWithFrame:r
                                     frameName:@"webkitFrame"
                                     groupName:@"webkitGroup"];
    SetPeer(new wxWidgetCocoaImpl( this, m_webView ));
#endif

    MacPostControlCreate(pos, size);

    [m_webView setHidden:false];


    // Register event listener interfaces
#if wxOSX_USE_IPHONE
#else
    WebViewLoadDelegate* loadDelegate =
            [[WebViewLoadDelegate alloc] initWithWxWindow: this];

    [m_webView setFrameLoadDelegate:loadDelegate];

    // this is used to veto page loads, etc.
    WebViewPolicyDelegate* policyDelegate =
            [[WebViewPolicyDelegate alloc] initWithWxWindow: this];

    [m_webView setPolicyDelegate:policyDelegate];

    WebViewUIDelegate* uiDelegate =
            [[WebViewUIDelegate alloc] initWithWxWindow: this];

    [m_webView setUIDelegate:uiDelegate];
#endif
    //Register our own class for custom scheme handling
    [NSURLProtocol registerClass:[WebViewCustomProtocol class]];

    LoadURL(strURL);
    return true;
}

wxWebViewWebKit::~wxWebViewWebKit()
{
#if wxOSX_USE_IPHONE
#else
    WebViewLoadDelegate* loadDelegate = [m_webView frameLoadDelegate];
    WebViewPolicyDelegate* policyDelegate = [m_webView policyDelegate];
    WebViewUIDelegate* uiDelegate = [m_webView UIDelegate];
    [m_webView setFrameLoadDelegate: nil];
    [m_webView setPolicyDelegate: nil];
    [m_webView setUIDelegate: nil];

    if (loadDelegate)
        [loadDelegate release];

    if (policyDelegate)
        [policyDelegate release];

    if (uiDelegate)
        [uiDelegate release];
#endif
}

// ----------------------------------------------------------------------------
// public methods
// ----------------------------------------------------------------------------

bool wxWebViewWebKit::CanGoBack() const
{
    if ( !m_webView )
        return false;

    return [m_webView canGoBack];
}

bool wxWebViewWebKit::CanGoForward() const
{
    if ( !m_webView )
        return false;

    return [m_webView canGoForward];
}

void wxWebViewWebKit::GoBack()
{
    if ( !m_webView )
        return;

    [m_webView goBack];
}

void wxWebViewWebKit::GoForward()
{
    if ( !m_webView )
        return;

    [m_webView goForward];
}

void wxWebViewWebKit::Reload(wxWebViewReloadFlags flags)
{
    if ( !m_webView )
        return;

    if (flags & wxWEBVIEW_RELOAD_NO_CACHE)
    {
        // TODO: test this indeed bypasses the cache
        [[m_webView preferences] setUsesPageCache:NO];
        [[m_webView mainFrame] reload];
        [[m_webView preferences] setUsesPageCache:YES];
    }
    else
    {
        [[m_webView mainFrame] reload];
    }
}

void wxWebViewWebKit::Stop()
{
    if ( !m_webView )
        return;

    [[m_webView mainFrame] stopLoading];
}

bool wxWebViewWebKit::CanGetPageSource() const
{
    if ( !m_webView )
        return false;

    WebDataSource* dataSource = [[m_webView mainFrame] dataSource];
    return ( [[dataSource representation] canProvideDocumentSource] );
}

wxString wxWebViewWebKit::GetPageSource() const
{

    if (CanGetPageSource())
    {
        WebDataSource* dataSource = [[m_webView mainFrame] dataSource];
        wxASSERT (dataSource != nil);

        id<WebDocumentRepresentation> representation = [dataSource representation];
        wxASSERT (representation != nil);

        NSString* source = [representation documentSource];
        if (source == nil)
        {
            return wxEmptyString;
        }

        return wxCFStringRef::AsString( source );
    }

    return wxEmptyString;
}

bool wxWebViewWebKit::CanIncreaseTextSize() const
{
    if ( !m_webView )
        return false;

    if ([m_webView canMakeTextLarger])
        return true;
    else
        return false;
}

void wxWebViewWebKit::IncreaseTextSize()
{
    if ( !m_webView )
        return;

    if (CanIncreaseTextSize())
        [m_webView makeTextLarger:(WebView*)m_webView];
}

bool wxWebViewWebKit::CanDecreaseTextSize() const
{
    if ( !m_webView )
        return false;

    if ([m_webView canMakeTextSmaller])
        return true;
    else
        return false;
}

void wxWebViewWebKit::DecreaseTextSize()
{
    if ( !m_webView )
        return;

    if (CanDecreaseTextSize())
        [m_webView makeTextSmaller:(WebView*)m_webView];
}

void wxWebViewWebKit::Print()
{

    // TODO: allow specifying the "show prompt" parameter in Print() ?
    bool showPrompt = true;

    if ( !m_webView )
        return;

    id view = [[[m_webView mainFrame] frameView] documentView];
    NSPrintOperation *op = [NSPrintOperation printOperationWithView:view
                                 printInfo: [NSPrintInfo sharedPrintInfo]];
    if (showPrompt)
    {
        [op setShowsPrintPanel: showPrompt];
        // in my tests, the progress bar always freezes and it stops the whole
        // print operation. do not turn this to true unless there is a
        // workaround for the bug.
        [op setShowsProgressPanel: false];
    }
    // Print it.
    [op runOperation];
}

void wxWebViewWebKit::SetEditable(bool enable)
{
    if ( !m_webView )
        return;

    [m_webView setEditable:enable ];
}

bool wxWebViewWebKit::IsEditable() const
{
    if ( !m_webView )
        return false;

    return [m_webView isEditable];
}

void wxWebViewWebKit::SetZoomType(wxWebViewZoomType zoomType)
{
    // there is only one supported zoom type at the moment so this setter
    // does nothing beyond checking sanity
    wxASSERT(zoomType == wxWEBVIEW_ZOOM_TYPE_TEXT);
}

wxWebViewZoomType wxWebViewWebKit::GetZoomType() const
{
    // for now that's the only one that is supported
    // FIXME: does the default zoom type change depending on webkit versions? :S
    //        Then this will be wrong
    return wxWEBVIEW_ZOOM_TYPE_TEXT;
}

bool wxWebViewWebKit::CanSetZoomType(wxWebViewZoomType type) const
{
    switch (type)
    {
        // for now that's the only one that is supported
        // TODO: I know recent versions of webkit support layout zoom too,
        //       check if we can support it
        case wxWEBVIEW_ZOOM_TYPE_TEXT:
            return true;

        default:
            return false;
    }
}

int wxWebViewWebKit::GetScrollPos()
{
    id result = [[m_webView windowScriptObject]
                    evaluateWebScript:@"document.body.scrollTop"];
    return [result intValue];
}

void wxWebViewWebKit::SetScrollPos(int pos)
{
    if ( !m_webView )
        return;

    wxString javascript;
    javascript.Printf(wxT("document.body.scrollTop = %d;"), pos);
    [[m_webView windowScriptObject] evaluateWebScript:
            wxCFStringRef( javascript ).AsNSString()];
}

wxString wxWebViewWebKit::GetSelectedText() const
{
    DOMRange* dr = [m_webView selectedDOMRange];
    if ( !dr )
        return wxString();

    return wxCFStringRef::AsString([dr toString]);
}

void wxWebViewWebKit::RunScript(const wxString& javascript)
{
    if ( !m_webView )
        return;

    [[m_webView windowScriptObject] evaluateWebScript:
                    wxCFStringRef( javascript ).AsNSString()];
}

void wxWebViewWebKit::OnSize(wxSizeEvent &event)
{
}

void wxWebViewWebKit::MacVisibilityChanged(){
}

void wxWebViewWebKit::LoadURL(const wxString& url)
{
    [[m_webView mainFrame] loadRequest:[NSURLRequest requestWithURL:
            [NSURL URLWithString:wxCFStringRef(url).AsNSString()]]];
}

wxString wxWebViewWebKit::GetCurrentURL() const
{
    return wxCFStringRef::AsString([m_webView mainFrameURL]);
}

wxString wxWebViewWebKit::GetCurrentTitle() const
{
    return wxCFStringRef::AsString([m_webView mainFrameTitle]);
}

float wxWebViewWebKit::GetWebkitZoom() const
{
    return [m_webView textSizeMultiplier];
}

void wxWebViewWebKit::SetWebkitZoom(float zoom)
{
    [m_webView setTextSizeMultiplier:zoom];
}

wxWebViewZoom wxWebViewWebKit::GetZoom() const
{
    float zoom = GetWebkitZoom();

    // arbitrary way to map float zoom to our common zoom enum
    if (zoom <= 0.55)
    {
        return wxWEBVIEW_ZOOM_TINY;
    }
    else if (zoom > 0.55 && zoom <= 0.85)
    {
        return wxWEBVIEW_ZOOM_SMALL;
    }
    else if (zoom > 0.85 && zoom <= 1.15)
    {
        return wxWEBVIEW_ZOOM_MEDIUM;
    }
    else if (zoom > 1.15 && zoom <= 1.45)
    {
        return wxWEBVIEW_ZOOM_LARGE;
    }
    else if (zoom > 1.45)
    {
        return wxWEBVIEW_ZOOM_LARGEST;
    }

    // to shut up compilers, this can never be reached logically
    wxASSERT(false);
    return wxWEBVIEW_ZOOM_MEDIUM;
}

void wxWebViewWebKit::SetZoom(wxWebViewZoom zoom)
{
    // arbitrary way to map our common zoom enum to float zoom
    switch (zoom)
    {
        case wxWEBVIEW_ZOOM_TINY:
            SetWebkitZoom(0.4f);
            break;

        case wxWEBVIEW_ZOOM_SMALL:
            SetWebkitZoom(0.7f);
            break;

        case wxWEBVIEW_ZOOM_MEDIUM:
            SetWebkitZoom(1.0f);
            break;

        case wxWEBVIEW_ZOOM_LARGE:
            SetWebkitZoom(1.3);
            break;

        case wxWEBVIEW_ZOOM_LARGEST:
            SetWebkitZoom(1.6);
            break;

        default:
            wxASSERT(false);
    }

}

void wxWebViewWebKit::DoSetPage(const wxString& src, const wxString& baseUrl)
{
   if ( !m_webView )
        return;

    [[m_webView mainFrame] loadHTMLString:wxCFStringRef( src ).AsNSString()
                                  baseURL:[NSURL URLWithString:
                                    wxCFStringRef( baseUrl ).AsNSString()]];
}

void wxWebViewWebKit::Cut()
{
    if ( !m_webView )
        return;

    [m_webView cut:m_webView];
}

void wxWebViewWebKit::Copy()
{
    if ( !m_webView )
        return;

    [m_webView copy:m_webView];
}

void wxWebViewWebKit::Paste()
{
    if ( !m_webView )
        return;

    [m_webView paste:m_webView];
}

void wxWebViewWebKit::DeleteSelection()
{
    if ( !m_webView )
        return;

    [m_webView deleteSelection];
}

bool wxWebViewWebKit::HasSelection() const
{
    DOMRange* range = [m_webView selectedDOMRange];
    if(!range)
    {
        return false;
    }
    else
    {
        return true;
    }
}

void wxWebViewWebKit::ClearSelection()
{
    //We use javascript as selection isn't exposed at the moment in webkit
    RunScript("window.getSelection().removeAllRanges();");
}

void wxWebViewWebKit::SelectAll()
{
    RunScript("window.getSelection().selectAllChildren(document.body);");
}

wxString wxWebViewWebKit::GetSelectedSource() const
{
    DOMRange* dr = [m_webView selectedDOMRange];
    if ( !dr )
        return wxString();

    return wxCFStringRef::AsString([dr markupString]);
}

wxString wxWebViewWebKit::GetPageText() const
{
    NSString *result = [m_webView stringByEvaluatingJavaScriptFromString:
                                  @"document.body.textContent"];
    return wxCFStringRef::AsString(result);
}

void wxWebViewWebKit::EnableHistory(bool enable)
{
    if ( !m_webView )
        return;

    [m_webView setMaintainsBackForwardList:enable];
}

void wxWebViewWebKit::ClearHistory()
{
    [m_webView setMaintainsBackForwardList:NO];
    [m_webView setMaintainsBackForwardList:YES];
}

wxVector<wxSharedPtr<wxWebViewHistoryItem> > wxWebViewWebKit::GetBackwardHistory()
{
    wxVector<wxSharedPtr<wxWebViewHistoryItem> > backhist;
    WebBackForwardList* history = [m_webView backForwardList];
    int count = [history backListCount];
    for(int i = -count; i < 0; i++)
    {
        WebHistoryItem* item = [history itemAtIndex:i];
        wxString url = wxCFStringRef::AsString([item URLString]);
        wxString title = wxCFStringRef::AsString([item title]);
        wxWebViewHistoryItem* wxitem = new wxWebViewHistoryItem(url, title);
        wxitem->m_histItem = item;
        wxSharedPtr<wxWebViewHistoryItem> itemptr(wxitem);
        backhist.push_back(itemptr);
    }
    return backhist;
}

wxVector<wxSharedPtr<wxWebViewHistoryItem> > wxWebViewWebKit::GetForwardHistory()
{
    wxVector<wxSharedPtr<wxWebViewHistoryItem> > forwardhist;
    WebBackForwardList* history = [m_webView backForwardList];
    int count = [history forwardListCount];
    for(int i = 1; i <= count; i++)
    {
        WebHistoryItem* item = [history itemAtIndex:i];
        wxString url = wxCFStringRef::AsString([item URLString]);
        wxString title = wxCFStringRef::AsString([item title]);
        wxWebViewHistoryItem* wxitem = new wxWebViewHistoryItem(url, title);
        wxitem->m_histItem = item;
        wxSharedPtr<wxWebViewHistoryItem> itemptr(wxitem);
        forwardhist.push_back(itemptr);
    }
    return forwardhist;
}

void wxWebViewWebKit::LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item)
{
    [m_webView goToBackForwardItem:item->m_histItem];
}

bool wxWebViewWebKit::CanUndo() const
{
    return [[m_webView undoManager] canUndo];
}

bool wxWebViewWebKit::CanRedo() const
{
    return [[m_webView undoManager] canRedo];
}

void wxWebViewWebKit::Undo()
{
    [[m_webView undoManager] undo];
}

void wxWebViewWebKit::Redo()
{
    [[m_webView undoManager] redo];
}

void wxWebViewWebKit::RegisterHandler(wxSharedPtr<wxWebViewHandler> handler)
{
    g_stringHandlerMap[handler->GetName()] = handler;
}

//------------------------------------------------------------
// Listener interfaces
//------------------------------------------------------------

#if wxOSX_USE_IPHONE
#else

// NB: I'm still tracking this down, but it appears the Cocoa window
// still has these events fired on it while the Carbon control is being
// destroyed. Therefore, we must be careful to check both the existence
// of the Carbon control and the event handler before firing events.

@implementation WebViewLoadDelegate

- (id)initWithWxWindow: (wxWebViewWebKit*)inWindow
{
    [super init];
    webKitWindow = inWindow;    // non retained
    return self;
}

- (void)webView:(WebView *)sender
    didStartProvisionalLoadForFrame:(WebFrame *)frame
{
    webKitWindow->m_busy = true;
}

- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame
{
    webKitWindow->m_busy = true;

    if (webKitWindow && frame == [sender mainFrame]){
        NSString *url = [[[[frame dataSource] request] URL] absoluteString];
        wxString target = wxCFStringRef::AsString([frame name]);
        wxWebViewEvent event(wxEVT_WEBVIEW_NAVIGATED,
                             webKitWindow->GetId(),
                             wxCFStringRef::AsString( url ),
                             target);

        if (webKitWindow && webKitWindow->GetEventHandler())
            webKitWindow->GetEventHandler()->ProcessEvent(event);
    }
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    webKitWindow->m_busy = false;

    if (webKitWindow && frame == [sender mainFrame]){
        NSString *url = [[[[frame dataSource] request] URL] absoluteString];

        wxString target = wxCFStringRef::AsString([frame name]);
        wxWebViewEvent event(wxEVT_WEBVIEW_LOADED,
                             webKitWindow->GetId(),
                             wxCFStringRef::AsString( url ),
                             target);

        if (webKitWindow && webKitWindow->GetEventHandler())
            webKitWindow->GetEventHandler()->ProcessEvent(event);
    }
}

wxString nsErrorToWxHtmlError(NSError* error, wxWebViewNavigationError* out)
{
    *out = wxWEBVIEW_NAV_ERR_OTHER;

    if ([[error domain] isEqualToString:NSURLErrorDomain])
    {
        switch ([error code])
        {
            case NSURLErrorCannotFindHost:
            case NSURLErrorFileDoesNotExist:
            case NSURLErrorRedirectToNonExistentLocation:
                *out = wxWEBVIEW_NAV_ERR_NOT_FOUND;
                break;

            case NSURLErrorResourceUnavailable:
            case NSURLErrorHTTPTooManyRedirects:
            case NSURLErrorDataLengthExceedsMaximum:
            case NSURLErrorBadURL:
            case NSURLErrorFileIsDirectory:
                *out = wxWEBVIEW_NAV_ERR_REQUEST;
                break;

            case NSURLErrorTimedOut:
            case NSURLErrorDNSLookupFailed:
            case NSURLErrorNetworkConnectionLost:
            case NSURLErrorCannotConnectToHost:
            case NSURLErrorNotConnectedToInternet:
            //case NSURLErrorInternationalRoamingOff:
            //case NSURLErrorCallIsActive:
            //case NSURLErrorDataNotAllowed:
                *out = wxWEBVIEW_NAV_ERR_CONNECTION;
                break;

            case NSURLErrorCancelled:
            case NSURLErrorUserCancelledAuthentication:
                *out = wxWEBVIEW_NAV_ERR_USER_CANCELLED;
                break;

            case NSURLErrorCannotDecodeRawData:
            case NSURLErrorCannotDecodeContentData:
            case NSURLErrorCannotParseResponse:
            case NSURLErrorBadServerResponse:
                *out = wxWEBVIEW_NAV_ERR_REQUEST;
                break;

            case NSURLErrorUserAuthenticationRequired:
            case NSURLErrorSecureConnectionFailed:
            case NSURLErrorClientCertificateRequired:
                *out = wxWEBVIEW_NAV_ERR_AUTH;
                break;

            case NSURLErrorNoPermissionsToReadFile:
                *out = wxWEBVIEW_NAV_ERR_SECURITY;
                break;

            case NSURLErrorServerCertificateHasBadDate:
            case NSURLErrorServerCertificateUntrusted:
            case NSURLErrorServerCertificateHasUnknownRoot:
            case NSURLErrorServerCertificateNotYetValid:
            case NSURLErrorClientCertificateRejected:
                *out = wxWEBVIEW_NAV_ERR_CERTIFICATE;
                break;
        }
    }

    wxString message = wxCFStringRef::AsString([error localizedDescription]);
    NSString* detail = [error localizedFailureReason];
    if (detail != NULL)
    {
        message = message + " (" + wxCFStringRef::AsString(detail) + ")";
    }
    return message;
}

- (void)webView:(WebView *)sender didFailLoadWithError:(NSError*) error
        forFrame:(WebFrame *)frame
{
    webKitWindow->m_busy = false;

    if (webKitWindow && frame == [sender mainFrame]){
        NSString *url = [[[[frame dataSource] request] URL] absoluteString];

        wxWebViewNavigationError type;
        wxString description = nsErrorToWxHtmlError(error, &type);
        wxWebViewEvent event(wxEVT_WEBVIEW_ERROR,
                             webKitWindow->GetId(),
                             wxCFStringRef::AsString( url ),
                             wxEmptyString);
        event.SetString(description);
        event.SetInt(type);

        if (webKitWindow && webKitWindow->GetEventHandler())
        {
            webKitWindow->GetEventHandler()->ProcessEvent(event);
        }
    }
}

- (void)webView:(WebView *)sender
        didFailProvisionalLoadWithError:(NSError*)error
                               forFrame:(WebFrame *)frame
{
    webKitWindow->m_busy = false;

    if (webKitWindow && frame == [sender mainFrame]){
        NSString *url = [[[[frame provisionalDataSource] request] URL]
                            absoluteString];

        wxWebViewNavigationError type;
        wxString description = nsErrorToWxHtmlError(error, &type);
        wxWebViewEvent event(wxEVT_WEBVIEW_ERROR,
                             webKitWindow->GetId(),
                             wxCFStringRef::AsString( url ),
                             wxEmptyString);
        event.SetString(description);
        event.SetInt(type);

        if (webKitWindow && webKitWindow->GetEventHandler())
            webKitWindow->GetEventHandler()->ProcessEvent(event);
    }
}

- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title
                                         forFrame:(WebFrame *)frame
{
    wxString target = wxCFStringRef::AsString([frame name]);
    wxWebViewEvent event(wxEVT_WEBVIEW_TITLE_CHANGED,
                         webKitWindow->GetId(),
                         webKitWindow->GetCurrentURL(),
                         target);

    event.SetString(wxCFStringRef::AsString(title));

    if (webKitWindow && webKitWindow->GetEventHandler())
        webKitWindow->GetEventHandler()->ProcessEvent(event);
}
@end

@implementation WebViewPolicyDelegate

- (id)initWithWxWindow: (wxWebViewWebKit*)inWindow
{
    [super init];
    webKitWindow = inWindow;    // non retained
    return self;
}

- (void)webView:(WebView *)sender
        decidePolicyForNavigationAction:(NSDictionary *)actionInformation
                                request:(NSURLRequest *)request
                                  frame:(WebFrame *)frame
                       decisionListener:(id<WebPolicyDecisionListener>)listener
{
    wxUnusedVar(frame);

    NSString *url = [[request URL] absoluteString];
    if (webKitWindow->m_nextNavigationIsNewWindow)
    {
        // This navigation has been marked as a new window
        // cancel the request here and send an apropriate event
        // to the application code
        webKitWindow->m_nextNavigationIsNewWindow = false;

        wxWebViewEvent event(wxEVT_WEBVIEW_NEWWINDOW,
                             webKitWindow->GetId(),
                             wxCFStringRef::AsString( url ), "");

        if (webKitWindow && webKitWindow->GetEventHandler())
            webKitWindow->GetEventHandler()->ProcessEvent(event);

        [listener ignore];
        return;
    }

    webKitWindow->m_busy = true;
    wxString target = wxCFStringRef::AsString([frame name]);
    wxWebViewEvent event(wxEVT_WEBVIEW_NAVIGATING,
                         webKitWindow->GetId(),
                         wxCFStringRef::AsString( url ), target);

    if (webKitWindow && webKitWindow->GetEventHandler())
        webKitWindow->GetEventHandler()->ProcessEvent(event);

    if (!event.IsAllowed())
    {
        webKitWindow->m_busy = false;
        [listener ignore];
    }
    else
    {
        [listener use];
    }
}

- (void)webView:(WebView *)sender
      decidePolicyForNewWindowAction:(NSDictionary *)actionInformation
                             request:(NSURLRequest *)request
                        newFrameName:(NSString *)frameName
                    decisionListener:(id < WebPolicyDecisionListener >)listener
{
    wxUnusedVar(actionInformation);

    NSString *url = [[request URL] absoluteString];
    wxWebViewEvent event(wxEVT_WEBVIEW_NEWWINDOW,
                         webKitWindow->GetId(),
                         wxCFStringRef::AsString( url ), "");

    if (webKitWindow && webKitWindow->GetEventHandler())
        webKitWindow->GetEventHandler()->ProcessEvent(event);

    [listener ignore];
}
@end

#endif

@implementation WebViewCustomProtocol

+ (BOOL)canInitWithRequest:(NSURLRequest *)request
{
    NSString *scheme = [[request URL] scheme];

    wxStringToWebHandlerMap::const_iterator it;
    for( it = g_stringHandlerMap.begin(); it != g_stringHandlerMap.end(); ++it )
    {
        if(it->first.IsSameAs(wxCFStringRef::AsString(scheme)))
        {
            return YES;
        }
    }

    return NO;
}

+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request
{
    //We don't do any processing here as the wxWebViewHandler classes do it
    return request;
}

- (void)startLoading
{
    NSURLRequest *request = [self request];
    NSString* path = [[request URL] absoluteString];

    id<NSURLProtocolClient> client = [self client];

    wxString wxpath = wxCFStringRef::AsString(path);
    wxString scheme = wxCFStringRef::AsString([[request URL] scheme]);
    wxFSFile* file = g_stringHandlerMap[scheme]->GetFile(wxpath);

    if (!file)
    {
        NSError *error = [[NSError alloc] initWithDomain:NSURLErrorDomain
                            code:NSURLErrorFileDoesNotExist
                            userInfo:nil];

        [client URLProtocol:self didFailWithError:error];

        return;
    }

    size_t length = file->GetStream()->GetLength();


    NSURLResponse *response =  [[NSURLResponse alloc] initWithURL:[request URL]
                               MIMEType:wxCFStringRef(file->GetMimeType()).AsNSString()
                               expectedContentLength:length
                               textEncodingName:nil];

    //Load the data, we malloc it so it is tidied up properly
    void* buffer = malloc(length);
    file->GetStream()->Read(buffer, length);
    NSData *data = [[NSData alloc] initWithBytesNoCopy:buffer length:length];

    //We do not support caching anything yet
    [client URLProtocol:self didReceiveResponse:response
            cacheStoragePolicy:NSURLCacheStorageNotAllowed];

    //Set the data
    [client URLProtocol:self didLoadData:data];

    //Notify that we have finished
    [client URLProtocolDidFinishLoading:self];

    [response release];
}

- (void)stopLoading
{

}

@end


@implementation WebViewUIDelegate

- (id)initWithWxWindow: (wxWebViewWebKit*)inWindow
{
    [super init];
    webKitWindow = inWindow;    // non retained
    return self;
}

- (WebView *)webView:(WebView *)sender createWebViewWithRequest:(NSURLRequest *)request
{
    // This method is called when window.open() is used in javascript with a target != _self
    // request is always nil, so it can't be used for event generation
    // Mark the next navigation as "new window"
    webKitWindow->m_nextNavigationIsNewWindow = true;
    return sender;
}

- (void)webView:(WebView *)sender printFrameView:(WebFrameView *)frameView
{
    wxUnusedVar(sender);
    wxUnusedVar(frameView);

    webKitWindow->Print();
}

- (NSArray *)webView:(WebView *)sender contextMenuItemsForElement:(NSDictionary *)element
                                                 defaultMenuItems:(NSArray *) defaultMenuItems
{
    if(webKitWindow->IsContextMenuEnabled())
        return defaultMenuItems;
    else
        return nil;
}
@end

#endif //wxUSE_WEBVIEW && wxUSE_WEBVIEW_WEBKIT
