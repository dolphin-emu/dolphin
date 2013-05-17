/////////////////////////////////////////////////////////////////////////////
// Name:        include/wx/msw/webviewie.h
// Purpose:     wxMSW IE wxWebView backend
// Author:      Marianne Gagnon
// Id:          $Id: webview_ie.h 70499 2012-02-02 20:32:08Z SJL $
// Copyright:   (c) 2010 Marianne Gagnon, 2011 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef wxWebViewIE_H
#define wxWebViewIE_H

#include "wx/setup.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_IE && defined(__WXMSW__)

#include "wx/control.h"
#include "wx/webview.h"
#include "wx/msw/ole/automtn.h"
#include "wx/msw/ole/activex.h"
#include "wx/msw/ole/oleutils.h"
#include "wx/msw/wrapwin.h"
#include "wx/msw/missing.h"
#include "wx/sharedptr.h"
#include "wx/vector.h"

/* Classes and definitions from urlmon.h vary in their
 * completeness between compilers and versions of compilers.
 * We implement our own versions here which should work
 * for all compilers. The definitions are taken from the
 * mingw-w64 headers which are public domain.
 */

#ifndef REFRESH_NORMAL
#define REFRESH_NORMAL 0
#endif

#ifndef REFRESH_COMPLETELY
#define REFRESH_COMPLETELY 3
#endif

typedef enum __wxMIDL_IBindStatusCallback_0006
{
    wxBSCF_FIRSTDATANOTIFICATION = 0x1,
    wxBSCF_INTERMEDIATEDATANOTIFICATION = 0x2,
    wxBSCF_LASTDATANOTIFICATION = 0x4,
    wxBSCF_DATAFULLYAVAILABLE = 0x8,
    wxBSCF_AVAILABLEDATASIZEUNKNOWN = 0x10
}   wxBSCF;

EXTERN_C const IID CLSID_FileProtocol;

typedef struct _tagwxBINDINFO
{
    ULONG cbSize;
    LPWSTR szExtraInfo;
    STGMEDIUM stgmedData;
    DWORD grfBindInfoF;
    DWORD dwBindVerb;
    LPWSTR szCustomVerb;
    DWORD cbstgmedData;
    DWORD dwOptions;
    DWORD dwOptionsFlags;
    DWORD dwCodePage;
    SECURITY_ATTRIBUTES securityAttributes;
    IID iid;
    IUnknown *pUnk;
    DWORD dwReserved;
}   wxBINDINFO;

typedef struct _tagwxPROTOCOLDATA
{
    DWORD grfFlags;
    DWORD dwState;
    LPVOID pData;
    ULONG cbData;
}   wxPROTOCOLDATA;

class wxIInternetBindInfo : public IUnknown
{
public:
    virtual HRESULT wxSTDCALL GetBindInfo(DWORD *grfBINDF,wxBINDINFO *pbindinfo) = 0;
    virtual HRESULT wxSTDCALL GetBindString(ULONG ulStringType,LPOLESTR *ppwzStr,
                                         ULONG cEl,ULONG *pcElFetched) = 0;
};

class wxIInternetProtocolSink : public IUnknown
{
public:
    virtual HRESULT wxSTDCALL Switch(wxPROTOCOLDATA *pProtocolData) = 0;
    virtual HRESULT wxSTDCALL ReportProgress(ULONG ulStatusCode,
                                          LPCWSTR szStatusText) = 0;
    virtual HRESULT wxSTDCALL ReportData(DWORD grfBSCF,ULONG ulProgress,
                                      ULONG ulProgressMax) = 0;
    virtual HRESULT wxSTDCALL ReportResult(HRESULT hrResult,DWORD dwError,
                                        LPCWSTR szResult) = 0;
};

class wxIInternetProtocolRoot : public IUnknown
{
public:
    virtual HRESULT wxSTDCALL Start(LPCWSTR szUrl,wxIInternetProtocolSink *pOIProtSink,
                                 wxIInternetBindInfo *pOIBindInfo,DWORD grfPI,
                                 HANDLE_PTR dwReserved) = 0;
    virtual HRESULT wxSTDCALL Continue(wxPROTOCOLDATA *pProtocolData) = 0;
    virtual HRESULT wxSTDCALL Abort(HRESULT hrReason,DWORD dwOptions) = 0;
    virtual HRESULT wxSTDCALL Terminate(DWORD dwOptions) = 0;
    virtual HRESULT wxSTDCALL Suspend(void) = 0;
    virtual HRESULT wxSTDCALL Resume(void) = 0;
};


class wxIInternetProtocol : public wxIInternetProtocolRoot
{
public:
    virtual HRESULT wxSTDCALL Read(void *pv,ULONG cb,ULONG *pcbRead) = 0;
    virtual HRESULT wxSTDCALL Seek(LARGE_INTEGER dlibMove,DWORD dwOrigin,
                                ULARGE_INTEGER *plibNewPosition) = 0;
    virtual HRESULT wxSTDCALL LockRequest(DWORD dwOptions) = 0;
    virtual HRESULT wxSTDCALL UnlockRequest(void) = 0;
};


class wxIInternetSession : public IUnknown
{
  public:
    virtual HRESULT wxSTDCALL RegisterNameSpace(IClassFactory *pCF,REFCLSID rclsid,
                                             LPCWSTR pwzProtocol,
                                             ULONG cPatterns,
                                             const LPCWSTR *ppwzPatterns,
                                             DWORD dwReserved) = 0;
    virtual HRESULT wxSTDCALL UnregisterNameSpace(IClassFactory *pCF,
                                               LPCWSTR pszProtocol) = 0;
    virtual HRESULT wxSTDCALL RegisterMimeFilter(IClassFactory *pCF,
                                              REFCLSID rclsid,
                                              LPCWSTR pwzType) = 0;
    virtual HRESULT wxSTDCALL UnregisterMimeFilter(IClassFactory *pCF,
                                                LPCWSTR pwzType) = 0;
    virtual HRESULT wxSTDCALL CreateBinding(LPBC pBC,LPCWSTR szUrl,
                                         IUnknown *pUnkOuter,IUnknown **ppUnk,
                                         wxIInternetProtocol **ppOInetProt,
                                         DWORD dwOption) = 0;
    virtual HRESULT wxSTDCALL SetSessionOption(DWORD dwOption,LPVOID pBuffer,
                                            DWORD dwBufferLength,
                                            DWORD dwReserved) = 0;
    virtual HRESULT wxSTDCALL GetSessionOption(DWORD dwOption,LPVOID pBuffer,
                                            DWORD *pdwBufferLength,
                                            DWORD dwReserved) = 0;
};

/* END OF URLMON.H implementation */

/* Same goes for the mshtmhst.h, these are also taken
 * from mingw-w64 headers.
 */

typedef enum _tagwxDOCHOSTUIFLAG
{
    DOCHOSTUIFLAG_DIALOG = 0x1,
    DOCHOSTUIFLAG_DISABLE_HELP_MENU = 0x2,
    DOCHOSTUIFLAG_NO3DBORDER = 0x4,
    DOCHOSTUIFLAG_SCROLL_NO = 0x8,
    DOCHOSTUIFLAG_DISABLE_SCRIPT_INACTIVE = 0x10,
    DOCHOSTUIFLAG_OPENNEWWIN = 0x20,
    DOCHOSTUIFLAG_DISABLE_OFFSCREEN = 0x40,
    DOCHOSTUIFLAG_FLAT_SCROLLBAR = 0x80,
    DOCHOSTUIFLAG_DIV_BLOCKDEFAULT = 0x100,
    DOCHOSTUIFLAG_ACTIVATE_CLIENTHIT_ONLY = 0x200,
    DOCHOSTUIFLAG_OVERRIDEBEHAVIORFACTORY = 0x400,
    DOCHOSTUIFLAG_CODEPAGELINKEDFONTS = 0x800,
    DOCHOSTUIFLAG_URL_ENCODING_DISABLE_UTF8 = 0x1000,
    DOCHOSTUIFLAG_URL_ENCODING_ENABLE_UTF8 = 0x2000,
    DOCHOSTUIFLAG_ENABLE_FORMS_AUTOCOMPLETE = 0x4000,
    DOCHOSTUIFLAG_ENABLE_INPLACE_NAVIGATION = 0x10000,
    DOCHOSTUIFLAG_IME_ENABLE_RECONVERSION = 0x20000,
    DOCHOSTUIFLAG_THEME = 0x40000,
    DOCHOSTUIFLAG_NOTHEME = 0x80000,
    DOCHOSTUIFLAG_NOPICS = 0x100000,
    DOCHOSTUIFLAG_NO3DOUTERBORDER = 0x200000,
    DOCHOSTUIFLAG_DISABLE_EDIT_NS_FIXUP = 0x400000,
    DOCHOSTUIFLAG_LOCAL_MACHINE_ACCESS_CHECK = 0x800000,
    DOCHOSTUIFLAG_DISABLE_UNTRUSTEDPROTOCOL = 0x1000000
} DOCHOSTUIFLAG;

typedef struct _tagwxDOCHOSTUIINFO
{
    ULONG cbSize;
    DWORD dwFlags;
    DWORD dwDoubleClick;
    OLECHAR *pchHostCss;
    OLECHAR *pchHostNS;
} DOCHOSTUIINFO;

class wxIDocHostUIHandler : public IUnknown
{
public:
    virtual HRESULT wxSTDCALL ShowContextMenu(DWORD dwID, POINT *ppt,
                                              IUnknown *pcmdtReserved,
                                              IDispatch *pdispReserved) = 0;

    virtual HRESULT wxSTDCALL GetHostInfo(DOCHOSTUIINFO *pInfo) = 0;

    virtual HRESULT wxSTDCALL ShowUI(DWORD dwID, 
                                     IOleInPlaceActiveObject *pActiveObject,
                                     IOleCommandTarget *pCommandTarget,
                                     IOleInPlaceFrame *pFrame,
                                     IOleInPlaceUIWindow *pDoc) = 0;

    virtual HRESULT wxSTDCALL HideUI(void) = 0;

    virtual HRESULT wxSTDCALL UpdateUI(void) = 0;
    
    virtual HRESULT wxSTDCALL EnableModeless(BOOL fEnable) = 0;

    virtual HRESULT wxSTDCALL OnDocWindowActivate(BOOL fActivate) = 0;

    virtual HRESULT wxSTDCALL OnFrameWindowActivate(BOOL fActivate) = 0;

    virtual HRESULT wxSTDCALL ResizeBorder(LPCRECT prcBorder,
                                           IOleInPlaceUIWindow *pUIWindow,
                                           BOOL fRameWindow) = 0;

    virtual HRESULT wxSTDCALL TranslateAccelerator(LPMSG lpMsg, 
                                                   const GUID *pguidCmdGroup,
                                                   DWORD nCmdID) = 0;

    virtual HRESULT wxSTDCALL GetOptionKeyPath(LPOLESTR *pchKey, 
                                               DWORD dw) = 0;

    virtual HRESULT wxSTDCALL GetDropTarget(IDropTarget *pDropTarget,
                                            IDropTarget **ppDropTarget) = 0;

    virtual HRESULT wxSTDCALL GetExternal(IDispatch **ppDispatch) = 0;

    virtual HRESULT wxSTDCALL TranslateUrl(DWORD dwTranslate,
                                           OLECHAR *pchURLIn,
                                           OLECHAR **ppchURLOut) = 0;

    virtual HRESULT wxSTDCALL FilterDataObject(IDataObject *pDO,
                                               IDataObject **ppDORet) = 0;
};

/* END OF MSHTMHST.H implementation */

struct IHTMLDocument2;
class wxFSFile;
class ClassFactory;
class wxIEContainer;
class DocHostUIHandler;

class WXDLLIMPEXP_WEBVIEW wxWebViewIE : public wxWebView
{
public:

    wxWebViewIE() {}

    wxWebViewIE(wxWindow* parent,
           wxWindowID id,
           const wxString& url = wxWebViewDefaultURLStr,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = 0,
           const wxString& name = wxWebViewNameStr)
   {
       Create(parent, id, url, pos, size, style, name);
   }

    ~wxWebViewIE();

    bool Create(wxWindow* parent,
           wxWindowID id,
           const wxString& url = wxWebViewDefaultURLStr,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           long style = 0,
           const wxString& name = wxWebViewNameStr);

    virtual void LoadURL(const wxString& url);
    virtual void LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item);
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetBackwardHistory();
    virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetForwardHistory();

    virtual bool CanGoForward() const;
    virtual bool CanGoBack() const;
    virtual void GoBack();
    virtual void GoForward();
    virtual void ClearHistory();
    virtual void EnableHistory(bool enable = true);
    virtual void Stop();
    virtual void Reload(wxWebViewReloadFlags flags = wxWEB_VIEW_RELOAD_DEFAULT);

    virtual wxString GetPageSource() const;
    virtual wxString GetPageText() const;

    virtual bool IsBusy() const;
    virtual wxString GetCurrentURL() const;
    virtual wxString GetCurrentTitle() const;

    virtual void SetZoomType(wxWebViewZoomType);
    virtual wxWebViewZoomType GetZoomType() const;
    virtual bool CanSetZoomType(wxWebViewZoomType) const;

    virtual void Print();

    virtual void SetPage(const wxString& html, const wxString& baseUrl);

    virtual wxWebViewZoom GetZoom() const;
    virtual void SetZoom(wxWebViewZoom zoom);

    //Clipboard functions
    virtual bool CanCut() const;
    virtual bool CanCopy() const;
    virtual bool CanPaste() const;
    virtual void Cut();
    virtual void Copy();
    virtual void Paste();

    //Undo / redo functionality
    virtual bool CanUndo() const;
    virtual bool CanRedo() const;
    virtual void Undo();
    virtual void Redo();

    //Editing functions
    virtual void SetEditable(bool enable = true);
    virtual bool IsEditable() const;

    //Selection
    virtual void SelectAll();
    virtual bool HasSelection() const;
    virtual void DeleteSelection();
    virtual wxString GetSelectedText() const;
    virtual wxString GetSelectedSource() const;
    virtual void ClearSelection();

    virtual void RunScript(const wxString& javascript);

    //Virtual Filesystem Support
    virtual void RegisterHandler(wxSharedPtr<wxWebViewHandler> handler);

    // ---- IE-specific methods

    // FIXME: I seem to be able to access remote webpages even in offline mode...
    bool IsOfflineMode();
    void SetOfflineMode(bool offline);

    wxWebViewZoom GetIETextZoom() const;
    void SetIETextZoom(wxWebViewZoom level);

    wxWebViewZoom GetIEOpticalZoom() const;
    void SetIEOpticalZoom(wxWebViewZoom level);

    void onActiveXEvent(wxActiveXEvent& evt);
    void onEraseBg(wxEraseEvent&) {}

    DECLARE_EVENT_TABLE();

private:
    wxIEContainer* m_container;
    wxAutomationObject m_ie;
    IWebBrowser2* m_webBrowser;
    DWORD m_dwCookie;
    DocHostUIHandler* m_uiHandler;

    //We store the current zoom type;
    wxWebViewZoomType m_zoomType;

    /** The "Busy" property of IWebBrowser2 does not always return busy when
     *  we'd want it to; this variable may be set to true in cases where the
     *  Busy property is false but should be true.
     */
    bool m_isBusy;
    //We manage our own history, the history list contains the history items
    //which are added as documentcomplete events arrive, unless we are loading
    //an item from the history. The position is stored as an int, and reflects
    //where we are in the history list.
    wxVector<wxSharedPtr<wxWebViewHistoryItem> > m_historyList;
    wxVector<ClassFactory*> m_factories;
    int m_historyPosition;
    bool m_historyLoadingFromList;
    bool m_historyEnabled;

    //Generic helper functions for IHtmlDocument commands
    bool CanExecCommand(wxString command) const;
    void ExecCommand(wxString command);
    IHTMLDocument2* GetDocument() const;
    //Toggles control features see INTERNETFEATURELIST for values.
    bool EnableControlFeature(long flag, bool enable = true);

    wxDECLARE_DYNAMIC_CLASS(wxWebViewIE);
};

class VirtualProtocol : public wxIInternetProtocol
{
protected:
    wxIInternetProtocolSink* m_protocolSink;
    wxString m_html;
    VOID * fileP;

    wxFSFile* m_file;
    wxSharedPtr<wxWebViewHandler> m_handler;

public:
    VirtualProtocol(wxSharedPtr<wxWebViewHandler> handler);
    ~VirtualProtocol() {}

    //IUnknown
    DECLARE_IUNKNOWN_METHODS;

    //IInternetProtocolRoot
    HRESULT STDMETHODCALLTYPE Abort(HRESULT WXUNUSED(hrReason),
                                    DWORD WXUNUSED(dwOptions))
                                   { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Continue(wxPROTOCOLDATA *WXUNUSED(pProtocolData))
                                       { return S_OK; }
    HRESULT STDMETHODCALLTYPE Resume() { return S_OK; }
    HRESULT STDMETHODCALLTYPE Start(LPCWSTR szUrl,
                                    wxIInternetProtocolSink *pOIProtSink,
                                    wxIInternetBindInfo *pOIBindInfo,
                                    DWORD grfPI,
                                    HANDLE_PTR dwReserved);
    HRESULT STDMETHODCALLTYPE Suspend() { return S_OK; }
    HRESULT STDMETHODCALLTYPE Terminate(DWORD WXUNUSED(dwOptions)) { return S_OK; }

    //IInternetProtocol
    HRESULT STDMETHODCALLTYPE LockRequest(DWORD WXUNUSED(dwOptions))
                                          { return S_OK; }
    HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead);
    HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER WXUNUSED(dlibMove),
                                   DWORD WXUNUSED(dwOrigin),
                                   ULARGE_INTEGER* WXUNUSED(plibNewPosition))
                                   { return E_FAIL; }
    HRESULT STDMETHODCALLTYPE UnlockRequest() { return S_OK; }
};

class ClassFactory : public IClassFactory
{
public:
    ClassFactory(wxSharedPtr<wxWebViewHandler> handler) : m_handler(handler) {}

    //IClassFactory
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter,
                                             REFIID riid, void** ppvObject);
    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock);

    //IUnknown
    DECLARE_IUNKNOWN_METHODS;

private:
    wxSharedPtr<wxWebViewHandler> m_handler;
};

class wxIEContainer : public wxActiveXContainer
{
public:
    wxIEContainer(wxWindow *parent, REFIID iid, IUnknown *pUnk, DocHostUIHandler* uiHandler = NULL);
    virtual ~wxIEContainer();
    virtual bool QueryClientSiteInterface(REFIID iid, void **_interface, const char *&desc);
private:
    DocHostUIHandler* m_uiHandler;
};

class DocHostUIHandler : public wxIDocHostUIHandler
{
public:
    DocHostUIHandler() {};
    ~DocHostUIHandler() {};
    virtual HRESULT wxSTDCALL ShowContextMenu(DWORD dwID, POINT *ppt,
                                              IUnknown *pcmdtReserved,
                                              IDispatch *pdispReserved);
    
    virtual HRESULT wxSTDCALL GetHostInfo(DOCHOSTUIINFO *pInfo);

    virtual HRESULT wxSTDCALL ShowUI(DWORD dwID,
                                     IOleInPlaceActiveObject *pActiveObject,
                                     IOleCommandTarget *pCommandTarget,
                                     IOleInPlaceFrame *pFrame,
                                     IOleInPlaceUIWindow *pDoc);

    virtual HRESULT wxSTDCALL HideUI(void);

    virtual HRESULT wxSTDCALL UpdateUI(void);

    virtual HRESULT wxSTDCALL EnableModeless(BOOL fEnable);

    virtual HRESULT wxSTDCALL OnDocWindowActivate(BOOL fActivate);

    virtual HRESULT wxSTDCALL OnFrameWindowActivate(BOOL fActivate);

    virtual HRESULT wxSTDCALL ResizeBorder(LPCRECT prcBorder,
                                           IOleInPlaceUIWindow *pUIWindow,
                                           BOOL fRameWindow);

    virtual HRESULT wxSTDCALL TranslateAccelerator(LPMSG lpMsg,
                                                   const GUID *pguidCmdGroup,
                                                   DWORD nCmdID);

    virtual HRESULT wxSTDCALL GetOptionKeyPath(LPOLESTR *pchKey, 
                                               DWORD dw);

    virtual HRESULT wxSTDCALL GetDropTarget(IDropTarget *pDropTarget,
                                            IDropTarget **ppDropTarget);

    virtual HRESULT wxSTDCALL GetExternal(IDispatch **ppDispatch);

    virtual HRESULT wxSTDCALL TranslateUrl(DWORD dwTranslate,
                                           OLECHAR *pchURLIn,
                                           OLECHAR **ppchURLOut);

    virtual HRESULT wxSTDCALL FilterDataObject(IDataObject *pDO,
                                               IDataObject **ppDORet);
    //IUnknown
    DECLARE_IUNKNOWN_METHODS;
};

#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_IE && defined(__WXMSW__)

#endif // wxWebViewIE_H
