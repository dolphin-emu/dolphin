/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/webview_ie.cpp
// Purpose:     wxMSW wxWebViewIE class implementation for web view component
// Author:      Marianne Gagnon
// Id:          $Id: webview_ie.cpp 70499 2012-02-02 20:32:08Z SJL $
// Copyright:   (c) 2010 Marianne Gagnon, 2011 Steven Lamerton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#include "wx/msw/webview_ie.h"

#if wxUSE_WEBVIEW && wxUSE_WEBVIEW_IE

#include <olectl.h>
#include <oleidl.h>
#include <exdispid.h>
#include <exdisp.h>
#include <mshtml.h>
#include "wx/msw/registry.h"
#include "wx/msw/missing.h"
#include "wx/filesys.h"
#include "wx/dynlib.h"
#include <initguid.h>

/* These GUID definitions are our own implementation to support interfaces
 * normally in urlmon.h. See include/wx/msw/webview_ie.h
 */

namespace {

DEFINE_GUID(wxIID_IInternetProtocolRoot,0x79eac9e3,0xbaf9,0x11ce,0x8c,0x82,0,0xaa,0,0x4b,0xa9,0xb);
DEFINE_GUID(wxIID_IInternetProtocol,0x79eac9e4,0xbaf9,0x11ce,0x8c,0x82,0,0xaa,0,0x4b,0xa9,0xb);
DEFINE_GUID(wxIID_IDocHostUIHandler, 0xbd3f23c0, 0xd43e, 0x11cf, 0x89, 0x3b, 0x00, 0xaa, 0x00, 0xbd, 0xce, 0x1a);

}

wxIMPLEMENT_DYNAMIC_CLASS(wxWebViewIE, wxWebView);

BEGIN_EVENT_TABLE(wxWebViewIE, wxControl)
    EVT_ACTIVEX(wxID_ANY, wxWebViewIE::onActiveXEvent)
    EVT_ERASE_BACKGROUND(wxWebViewIE::onEraseBg)
END_EVENT_TABLE()

bool wxWebViewIE::Create(wxWindow* parent,
           wxWindowID id,
           const wxString& url,
           const wxPoint& pos,
           const wxSize& size,
           long style,
           const wxString& name)
{
    if (!wxControl::Create(parent, id, pos, size, style,
                           wxDefaultValidator, name))
    {
        return false;
    }

    m_webBrowser = NULL;
    m_isBusy = false;
    m_historyLoadingFromList = false;
    m_historyEnabled = true;
    m_historyPosition = -1;
    m_zoomType = wxWEB_VIEW_ZOOM_TYPE_TEXT;

    if (::CoCreateInstance(CLSID_WebBrowser, NULL,
                           CLSCTX_INPROC_SERVER, // CLSCTX_INPROC,
                           IID_IWebBrowser2 , (void**)&m_webBrowser) != 0)
    {
        wxLogError("Failed to initialize IE, CoCreateInstance returned an error");
        return false;
    }

    m_ie.SetDispatchPtr(m_webBrowser); // wxAutomationObject will release itself

    m_webBrowser->put_RegisterAsBrowser(VARIANT_TRUE);
    m_webBrowser->put_RegisterAsDropTarget(VARIANT_TRUE);

    m_uiHandler = new DocHostUIHandler;
    m_uiHandler->AddRef();

    m_container = new wxIEContainer(this, IID_IWebBrowser2, m_webBrowser, m_uiHandler);

    EnableControlFeature(21 /* FEATURE_DISABLE_NAVIGATION_SOUNDS */);

    LoadURL(url);
    return true;
}

wxWebViewIE::~wxWebViewIE()
{
    for(unsigned int i = 0; i < m_factories.size(); i++)
    {
        m_factories[i]->Release();
    }

    m_uiHandler->Release();
}

void wxWebViewIE::LoadURL(const wxString& url)
{
    m_ie.CallMethod("Navigate", wxConvertStringToOle(url));
}

void wxWebViewIE::SetPage(const wxString& html, const wxString& baseUrl)
{
    BSTR bstr = SysAllocString(OLESTR(""));
    SAFEARRAY *psaStrings = SafeArrayCreateVector(VT_VARIANT, 0, 1);
    if (psaStrings != NULL)
    {
        VARIANT *param;
        HRESULT hr = SafeArrayAccessData(psaStrings, (LPVOID*)&param);
        param->vt = VT_BSTR;
        param->bstrVal = bstr;

        hr = SafeArrayUnaccessData(psaStrings);

        IHTMLDocument2* document = GetDocument();
        document->write(psaStrings);
        document->close();
        document->Release();

        SafeArrayDestroy(psaStrings);

        bstr = SysAllocString(html.wc_str());

        // Creates a new one-dimensional array
        psaStrings = SafeArrayCreateVector(VT_VARIANT, 0, 1);
        if (psaStrings != NULL)
        {
            hr = SafeArrayAccessData(psaStrings, (LPVOID*)&param);
            param->vt = VT_BSTR;
            param->bstrVal = bstr;
            hr = SafeArrayUnaccessData(psaStrings);

            document = GetDocument();
            document->write(psaStrings);
            document->Release();

            // SafeArrayDestroy calls SysFreeString for each BSTR
            SafeArrayDestroy(psaStrings);

            //We send the events when we are done to mimic webkit
            //Navigated event
            wxWebViewEvent event(wxEVT_COMMAND_WEB_VIEW_NAVIGATED,
                                 GetId(), baseUrl, "");
            event.SetEventObject(this);
            HandleWindowEvent(event);

            //Document complete event
            event.SetEventType(wxEVT_COMMAND_WEB_VIEW_LOADED);
            event.SetEventObject(this);
            HandleWindowEvent(event);
        }
        else
        {
            wxLogError("wxWebViewIE::SetPage() : psaStrings is NULL");
        }
    }
    else
    {
        wxLogError("wxWebViewIE::SetPage() : psaStrings is NULL during clear");
    }
}

wxString wxWebViewIE::GetPageSource() const
{
    IHTMLDocument2* document = GetDocument();
    IHTMLElement *bodyTag = NULL;
    IHTMLElement *htmlTag = NULL;
    wxString source;
    HRESULT hr = document->get_body(&bodyTag);
    if(SUCCEEDED(hr))
    {
        hr = bodyTag->get_parentElement(&htmlTag);
        if(SUCCEEDED(hr))
        {
            BSTR bstr;
            htmlTag->get_outerHTML(&bstr);
            source = wxString(bstr);
            htmlTag->Release();
        }
        bodyTag->Release();
    }

    document->Release();
    return source;
}

wxWebViewZoom wxWebViewIE::GetZoom() const
{
    switch( m_zoomType )
    {
        case wxWEB_VIEW_ZOOM_TYPE_LAYOUT:
            return GetIEOpticalZoom();
        case wxWEB_VIEW_ZOOM_TYPE_TEXT:
            return GetIETextZoom();
        default:
            wxFAIL;
    }

    //Dummy return to stop compiler warnings
    return wxWEB_VIEW_ZOOM_MEDIUM;

}

void wxWebViewIE::SetZoom(wxWebViewZoom zoom)
{
    switch( m_zoomType )
    {
        case wxWEB_VIEW_ZOOM_TYPE_LAYOUT:
            SetIEOpticalZoom(zoom);
            break;
        case wxWEB_VIEW_ZOOM_TYPE_TEXT:
            SetIETextZoom(zoom);
            break;
        default:
            wxFAIL;
    }
}

void wxWebViewIE::SetIETextZoom(wxWebViewZoom level)
{
    //We do not use OLECMDID_OPTICAL_GETZOOMRANGE as the docs say the range
    //is 0 to 4 so the check is unnecessary, these match exactly with the
    //enum values
    VARIANT zoomVariant;
    VariantInit (&zoomVariant);
    V_VT(&zoomVariant) = VT_I4;
    V_I4(&zoomVariant) = level;

#if wxDEBUG_LEVEL
    HRESULT result =
#endif
            m_webBrowser->ExecWB(OLECMDID_ZOOM,
                                 OLECMDEXECOPT_DONTPROMPTUSER,
                                 &zoomVariant, NULL);
    wxASSERT(result == S_OK);
}

wxWebViewZoom wxWebViewIE::GetIETextZoom() const
{
    VARIANT zoomVariant;
    VariantInit (&zoomVariant);
    V_VT(&zoomVariant) = VT_I4;

#if wxDEBUG_LEVEL
    HRESULT result =
#endif
            m_webBrowser->ExecWB(OLECMDID_ZOOM,
                                 OLECMDEXECOPT_DONTPROMPTUSER,
                                 NULL, &zoomVariant);
    wxASSERT(result == S_OK);

    //We can safely cast here as we know that the range matches our enum
    return static_cast<wxWebViewZoom>(V_I4(&zoomVariant));
}

void wxWebViewIE::SetIEOpticalZoom(wxWebViewZoom level)
{
    //We do not use OLECMDID_OPTICAL_GETZOOMRANGE as the docs say the range
    //is 10 to 1000 so the check is unnecessary
    VARIANT zoomVariant;
    VariantInit (&zoomVariant);
    V_VT(&zoomVariant) = VT_I4;

    //We make a somewhat arbitray map here, taken from values used by webkit
    switch(level)
    {
        case wxWEB_VIEW_ZOOM_TINY:
            V_I4(&zoomVariant) = 60;
            break;
        case wxWEB_VIEW_ZOOM_SMALL:
            V_I4(&zoomVariant) = 80;
            break;
        case wxWEB_VIEW_ZOOM_MEDIUM:
            V_I4(&zoomVariant) = 100;
            break;
        case wxWEB_VIEW_ZOOM_LARGE:
            V_I4(&zoomVariant) = 130;
            break;
        case wxWEB_VIEW_ZOOM_LARGEST:
            V_I4(&zoomVariant) = 160;
            break;
        default:
            wxFAIL;
    }

#if wxDEBUG_LEVEL
    HRESULT result =
#endif
            m_webBrowser->ExecWB((OLECMDID)63 /*OLECMDID_OPTICAL_ZOOM*/,
                                 OLECMDEXECOPT_DODEFAULT,
                                 &zoomVariant,
                                 NULL);
    wxASSERT(result == S_OK);
}

wxWebViewZoom wxWebViewIE::GetIEOpticalZoom() const
{
    VARIANT zoomVariant;
    VariantInit (&zoomVariant);
    V_VT(&zoomVariant) = VT_I4;

#if wxDEBUG_LEVEL
    HRESULT result =
#endif
            m_webBrowser->ExecWB((OLECMDID)63 /*OLECMDID_OPTICAL_ZOOM*/,
                                 OLECMDEXECOPT_DODEFAULT, NULL,
                                 &zoomVariant);
    wxASSERT(result == S_OK);

    const int zoom = V_I4(&zoomVariant);

    //We make a somewhat arbitray map here, taken from values used by webkit
    if (zoom <= 65)
    {
        return wxWEB_VIEW_ZOOM_TINY;
    }
    else if (zoom > 65 && zoom <= 90)
    {
        return wxWEB_VIEW_ZOOM_SMALL;
    }
    else if (zoom > 90 && zoom <= 115)
    {
        return wxWEB_VIEW_ZOOM_MEDIUM;
    }
    else if (zoom > 115 && zoom <= 145)
    {
        return wxWEB_VIEW_ZOOM_LARGE;
    }
    else /*if (zoom > 145) */ //Using else removes a compiler warning
    {
        return wxWEB_VIEW_ZOOM_LARGEST;
    }
}

void wxWebViewIE::SetZoomType(wxWebViewZoomType type)
{
    m_zoomType = type;
}

wxWebViewZoomType wxWebViewIE::GetZoomType() const
{
    return m_zoomType;
}

bool wxWebViewIE::CanSetZoomType(wxWebViewZoomType type) const
{
    //IE 6 and below only support text zoom, so check the registry to see what
    //version we actually have
    wxRegKey key(wxRegKey::HKLM, "Software\\Microsoft\\Internet Explorer");
    wxString value;
    key.QueryValue("Version", value);

    long version = wxAtoi(value.Left(1));
    if(version <= 6 && type == wxWEB_VIEW_ZOOM_TYPE_LAYOUT)
        return false;
    else
        return true;
}

void wxWebViewIE::Print()
{
    m_webBrowser->ExecWB(OLECMDID_PRINTPREVIEW,
                         OLECMDEXECOPT_DODEFAULT, NULL, NULL);
}

bool wxWebViewIE::CanGoBack() const
{
    if(m_historyEnabled)
        return m_historyPosition > 0;
    else
        return false;
}

bool wxWebViewIE::CanGoForward() const
{
    if(m_historyEnabled)
        return m_historyPosition != static_cast<int>(m_historyList.size()) - 1;
    else
        return false;
}

void wxWebViewIE::LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item)
{
    int pos = -1;
    for(unsigned int i = 0; i < m_historyList.size(); i++)
    {
        //We compare the actual pointers to find the correct item
        if(m_historyList[i].get() == item.get())
            pos = i;
    }
    wxASSERT_MSG(pos != static_cast<int>(m_historyList.size()),
                 "invalid history item");
    m_historyLoadingFromList = true;
    LoadURL(item->GetUrl());
    m_historyPosition = pos;
}

wxVector<wxSharedPtr<wxWebViewHistoryItem> > wxWebViewIE::GetBackwardHistory()
{
    wxVector<wxSharedPtr<wxWebViewHistoryItem> > backhist;
    //As we don't have std::copy or an iterator constructor in the wxwidgets
    //native vector we construct it by hand
    for(int i = 0; i < m_historyPosition; i++)
    {
        backhist.push_back(m_historyList[i]);
    }
    return backhist;
}

wxVector<wxSharedPtr<wxWebViewHistoryItem> > wxWebViewIE::GetForwardHistory()
{
    wxVector<wxSharedPtr<wxWebViewHistoryItem> > forwardhist;
    //As we don't have std::copy or an iterator constructor in the wxwidgets
    //native vector we construct it by hand
    for(int i = m_historyPosition + 1; i < static_cast<int>(m_historyList.size()); i++)
    {
        forwardhist.push_back(m_historyList[i]);
    }
    return forwardhist;
}

void wxWebViewIE::GoBack()
{
    LoadHistoryItem(m_historyList[m_historyPosition - 1]);
}

void wxWebViewIE::GoForward()
{
    LoadHistoryItem(m_historyList[m_historyPosition + 1]);
}

void wxWebViewIE::Stop()
{
    m_ie.CallMethod("Stop");
}

void wxWebViewIE::ClearHistory()
{
    m_historyList.clear();
    m_historyPosition = -1;
}

void wxWebViewIE::EnableHistory(bool enable)
{
    m_historyEnabled = enable;
    m_historyList.clear();
    m_historyPosition = -1;
}

void wxWebViewIE::Reload(wxWebViewReloadFlags flags)
{
    VARIANTARG level;
    VariantInit(&level);
    V_VT(&level) = VT_I2;

    switch(flags)
    {
        case wxWEB_VIEW_RELOAD_DEFAULT:
            V_I2(&level) = REFRESH_NORMAL;
            break;
        case wxWEB_VIEW_RELOAD_NO_CACHE:
            V_I2(&level) = REFRESH_COMPLETELY;
            break;
        default:
            wxFAIL_MSG("Unexpected reload type");
    }

    m_webBrowser->Refresh2(&level);
}

bool wxWebViewIE::IsOfflineMode()
{
    wxVariant out = m_ie.GetProperty("Offline");

    wxASSERT(out.GetType() == "bool");

    return out.GetBool();
}

void wxWebViewIE::SetOfflineMode(bool offline)
{
    // FIXME: the wxWidgets docs do not really document what the return
    //        parameter of PutProperty is
#if wxDEBUG_LEVEL
    const bool success =
#endif
            m_ie.PutProperty("Offline", (offline ?
                                         VARIANT_TRUE :
                                         VARIANT_FALSE));
    wxASSERT(success);
}

bool wxWebViewIE::IsBusy() const
{
    if (m_isBusy) return true;

    wxVariant out = m_ie.GetProperty("Busy");

    wxASSERT(out.GetType() == "bool");

    return out.GetBool();
}

wxString wxWebViewIE::GetCurrentURL() const
{
    wxVariant out = m_ie.GetProperty("LocationURL");

    wxASSERT(out.GetType() == "string");
    return out.GetString();
}

wxString wxWebViewIE::GetCurrentTitle() const
{
    IHTMLDocument2* document = GetDocument();
    BSTR title;

    document->get_nameProp(&title);
    document->Release();
    return wxString(title);
}

bool wxWebViewIE::CanCut() const
{
    return CanExecCommand("Cut");
}

bool wxWebViewIE::CanCopy() const
{
    return CanExecCommand("Copy");
}
bool wxWebViewIE::CanPaste() const
{
    return CanExecCommand("Paste");
}

void wxWebViewIE::Cut()
{
    ExecCommand("Cut");
}

void wxWebViewIE::Copy()
{
    ExecCommand("Copy");
}

void wxWebViewIE::Paste()
{
    ExecCommand("Paste");
}

bool wxWebViewIE::CanUndo() const
{
    return CanExecCommand("Undo");
}
bool wxWebViewIE::CanRedo() const
{
    return CanExecCommand("Redo");
}

void wxWebViewIE::Undo()
{
    ExecCommand("Undo");
}

void wxWebViewIE::Redo()
{
    ExecCommand("Redo");
}

void wxWebViewIE::SetEditable(bool enable)
{
    IHTMLDocument2* document = GetDocument();
    if( enable )
        document->put_designMode(SysAllocString(L"On"));
    else
        document->put_designMode(SysAllocString(L"Off"));

    document->Release();
}

bool wxWebViewIE::IsEditable() const
{
    IHTMLDocument2* document = GetDocument();
    BSTR mode;
    document->get_designMode(&mode);
    document->Release();
    if(wxString(mode) == "On")
        return true;
    else
        return false;
}

void wxWebViewIE::SelectAll()
{
    ExecCommand("SelectAll");
}

bool wxWebViewIE::HasSelection() const
{
    IHTMLDocument2* document = GetDocument();
    IHTMLSelectionObject* selection;
    wxString sel;
    HRESULT hr = document->get_selection(&selection);
    if(SUCCEEDED(hr))
    {
        BSTR type;
        selection->get_type(&type);
        sel = wxString(type);
        selection->Release();
    }
    document->Release();
    return sel != "None";
}

void wxWebViewIE::DeleteSelection()
{
    ExecCommand("Delete");
}

wxString wxWebViewIE::GetSelectedText() const
{
    IHTMLDocument2* document = GetDocument();
    IHTMLSelectionObject* selection;
    wxString selected;
    HRESULT hr = document->get_selection(&selection);
    if(SUCCEEDED(hr))
    {
        IDispatch* disrange;
        hr = selection->createRange(&disrange);
        if(SUCCEEDED(hr))
        {
            IHTMLTxtRange* range;
            hr = disrange->QueryInterface(IID_IHTMLTxtRange, (void**)&range);
            if(SUCCEEDED(hr))
            {
                BSTR text;
                range->get_text(&text);
                selected = wxString(text);
                range->Release();
            }
            disrange->Release();
        }
        selection->Release();
    }
    document->Release();
    return selected;
}

wxString wxWebViewIE::GetSelectedSource() const
{
    IHTMLDocument2* document = GetDocument();
    IHTMLSelectionObject* selection;
    wxString selected;
    HRESULT hr = document->get_selection(&selection);
    if(SUCCEEDED(hr))
    {
        IDispatch* disrange;
        hr = selection->createRange(&disrange);
        if(SUCCEEDED(hr))
        {
            IHTMLTxtRange* range;
            hr = disrange->QueryInterface(IID_IHTMLTxtRange, (void**)&range);
            if(SUCCEEDED(hr))
            {
                BSTR text;
                range->get_htmlText(&text);
                selected = wxString(text);
                range->Release();
            }
            disrange->Release();
        }
        selection->Release();
    }
    document->Release();
    return selected;
}

void wxWebViewIE::ClearSelection()
{
    IHTMLDocument2* document = GetDocument();
    IHTMLSelectionObject* selection;
    wxString selected;
    HRESULT hr = document->get_selection(&selection);
    if(SUCCEEDED(hr))
    {
        selection->empty();
        selection->Release();
    }
    document->Release();
}

wxString wxWebViewIE::GetPageText() const
{
    IHTMLDocument2* document = GetDocument();
    wxString text;
    IHTMLElement* body;
    HRESULT hr = document->get_body(&body);
    if(SUCCEEDED(hr))
    {
        BSTR out;
        body->get_innerText(&out);
        text = wxString(out);
        body->Release();
    }
    document->Release();
    return text;
}

void wxWebViewIE::RunScript(const wxString& javascript)
{
    IHTMLDocument2* document = GetDocument();
    IHTMLWindow2* window;
    wxString language = "javascript";
    HRESULT hr = document->get_parentWindow(&window);
    if(SUCCEEDED(hr))
    {
        VARIANT level;
        VariantInit(&level);
        V_VT(&level) = VT_EMPTY;
        window->execScript(SysAllocString(javascript.wc_str()),
                           SysAllocString(language.wc_str()),
                           &level);
    }
    document->Release();
}

void wxWebViewIE::RegisterHandler(wxSharedPtr<wxWebViewHandler> handler)
{
    wxDynamicLibrary urlMon(wxT("urlmon.dll"));
    if(urlMon.HasSymbol(wxT("CoInternetGetSession")))
    {
        typedef HRESULT (WINAPI *CoInternetGetSession_t)(DWORD, wxIInternetSession**, DWORD);
        wxDYNLIB_FUNCTION(CoInternetGetSession_t, CoInternetGetSession, urlMon);

        ClassFactory* cf = new ClassFactory(handler);
        wxIInternetSession* session;
        HRESULT res = (*pfnCoInternetGetSession)(0, &session, 0);
        if(FAILED(res))
        {
            wxFAIL_MSG("Could not retrive internet session");
        }

        HRESULT hr = session->RegisterNameSpace(cf, CLSID_FileProtocol,
                                                handler->GetName().wc_str(),
                                                0, NULL, 0);
        if(FAILED(hr))
        {
            wxFAIL_MSG("Could not register protocol");
        }
        m_factories.push_back(cf);
    }
    else
    {
        wxFAIL_MSG("urlmon does not contain CoInternetGetSession");
    }
}

bool wxWebViewIE::CanExecCommand(wxString command) const
{
    IHTMLDocument2* document = GetDocument();
    VARIANT_BOOL enabled;

    document->queryCommandEnabled(SysAllocString(command.wc_str()), &enabled);
    document->Release();

    return (enabled == VARIANT_TRUE);
}

void wxWebViewIE::ExecCommand(wxString command)
{
    IHTMLDocument2* document = GetDocument();
    document->execCommand(SysAllocString(command.wc_str()), VARIANT_FALSE, VARIANT(), NULL);
    document->Release();
}

IHTMLDocument2* wxWebViewIE::GetDocument() const
{
    wxVariant variant = m_ie.GetProperty("Document");
    IHTMLDocument2* document = (IHTMLDocument2*)variant.GetVoidPtr();

    wxASSERT(document);

    return document;
}

bool wxWebViewIE::EnableControlFeature(long flag, bool enable)
{
#if wxUSE_DYNLIB_CLASS

    wxDynamicLibrary urlMon(wxT("urlmon.dll"));
    if( urlMon.IsLoaded() && 
        urlMon.HasSymbol("CoInternetSetFeatureEnabled") && 
        urlMon.HasSymbol("CoInternetIsFeatureEnabled"))
    {
        typedef HRESULT (WINAPI *CoInternetSetFeatureEnabled_t)(DWORD, DWORD, BOOL);
        typedef HRESULT (WINAPI *CoInternetIsFeatureEnabled_t)(DWORD, DWORD);

        wxDYNLIB_FUNCTION(CoInternetSetFeatureEnabled_t, CoInternetSetFeatureEnabled, urlMon);
        wxDYNLIB_FUNCTION(CoInternetIsFeatureEnabled_t, CoInternetIsFeatureEnabled, urlMon);

        HRESULT hr = (*pfnCoInternetIsFeatureEnabled)(flag,
                                                      0x2 /* SET_FEATURE_ON_PROCESS */);
        if((hr == S_OK && enable) || (hr == S_FALSE && !enable))
            return true;

        hr = (*pfnCoInternetSetFeatureEnabled)(flag,
                                               0x2/* SET_FEATURE_ON_PROCESS */,
                                               (enable ? TRUE : FALSE));
        if ( FAILED(hr) )
        {
            wxLogApiError(wxT("CoInternetSetFeatureEnabled"), hr);
            return false;
        }
        return true;
    }
    return false;
#else
    wxUnusedVar(flag);
    wxUnusedVar(enable);
    return false;
#endif // wxUSE_DYNLIB_CLASS/!wxUSE_DYNLIB_CLASS
}

void wxWebViewIE::onActiveXEvent(wxActiveXEvent& evt)
{
    if (m_webBrowser == NULL) return;

    switch (evt.GetDispatchId())
    {
        case DISPID_BEFORENAVIGATE2:
        {
            m_isBusy = true;

            wxString url = evt[1].GetString();
            wxString target = evt[3].GetString();

            wxWebViewEvent event(wxEVT_COMMAND_WEB_VIEW_NAVIGATING,
                                 GetId(), url, target);

            //skip empty javascript events.
            if(url == "javascript:\"\"" && target.IsEmpty())
            {
                event.Veto();
            }
            else
            {
                event.SetEventObject(this);
                HandleWindowEvent(event);
            }

            if (!event.IsAllowed())
            {
                wxActiveXEventNativeMSW* nativeParams =
                    evt.GetNativeParameters();
                *V_BOOLREF(&nativeParams->pDispParams->rgvarg[0]) = VARIANT_TRUE;
            }

            // at this point, either the navigation event has been cancelled
            // and we're not busy, either it was accepted and IWebBrowser2's
            // Busy property will be true; so we don't need our override
            // flag anymore.
            m_isBusy = false;

            break;
        }

        case DISPID_NAVIGATECOMPLETE2:
        {
            wxString url = evt[1].GetString();
            // TODO: set target parameter if possible
            wxString target = wxEmptyString;
            wxWebViewEvent event(wxEVT_COMMAND_WEB_VIEW_NAVIGATED,
                                 GetId(), url, target);
            event.SetEventObject(this);
            HandleWindowEvent(event);
            break;
        }

        case DISPID_PROGRESSCHANGE:
        {
            // download progress
            break;
        }

        case DISPID_DOCUMENTCOMPLETE:
        {
            //Only send a complete even if we are actually finished, this brings
            //the event in to line with webkit
            READYSTATE rs;
            m_webBrowser->get_ReadyState( &rs );
            if(rs != READYSTATE_COMPLETE)
                break;

            wxString url = evt[1].GetString();

            //As we are complete we also add to the history list, but not if the
            //page is not the main page, ie it is a subframe
            //We also have to check if we are loading a file:// url, if so we
            //need to change the comparison as ie passes back a different style
            //of url
            if(m_historyEnabled && !m_historyLoadingFromList &&
              (url == GetCurrentURL() ||
              (GetCurrentURL().substr(0, 4) == "file" &&
               wxFileSystem::URLToFileName(GetCurrentURL()).GetFullPath() == url)))
            {
                //If we are not at the end of the list, then erase everything
                //between us and the end before adding the new page
                if(m_historyPosition != static_cast<int>(m_historyList.size()) - 1)
                {
                    m_historyList.erase(m_historyList.begin() + m_historyPosition + 1,
                                        m_historyList.end());
                }
                wxSharedPtr<wxWebViewHistoryItem> item(new wxWebViewHistoryItem(url, GetCurrentTitle()));
                m_historyList.push_back(item);
                m_historyPosition++;
            }
            //Reset as we are done now
            m_historyLoadingFromList = false;
            // TODO: set target parameter if possible
            wxString target = wxEmptyString;
            wxWebViewEvent event(wxEVT_COMMAND_WEB_VIEW_LOADED, GetId(),
                                 url, target);
            event.SetEventObject(this);
            HandleWindowEvent(event);
            break;
        }

        case DISPID_STATUSTEXTCHANGE:
        {
            break;
        }

        case DISPID_TITLECHANGE:
        {
            wxString title = evt[0].GetString();

            wxWebViewEvent event(wxEVT_COMMAND_WEB_VIEW_TITLE_CHANGED,
                                 GetId(), GetCurrentURL(), "");
            event.SetString(title);
            event.SetEventObject(this);
            HandleWindowEvent(event);
            break;
        }

        case DISPID_NAVIGATEERROR:
        {
            wxWebViewNavigationError errorType = wxWEB_NAV_ERR_OTHER;
            wxString errorCode = "?";
            switch (evt[3].GetLong())
            {
            case INET_E_INVALID_URL: // (0x800C0002L or -2146697214)
                errorCode = "INET_E_INVALID_URL";
                errorType = wxWEB_NAV_ERR_REQUEST;
                break;
            case INET_E_NO_SESSION: // (0x800C0003L or -2146697213)
                errorCode = "INET_E_NO_SESSION";
                errorType = wxWEB_NAV_ERR_CONNECTION;
                break;
            case INET_E_CANNOT_CONNECT: // (0x800C0004L or -2146697212)
                errorCode = "INET_E_CANNOT_CONNECT";
                errorType = wxWEB_NAV_ERR_CONNECTION;
                break;
            case INET_E_RESOURCE_NOT_FOUND: // (0x800C0005L or -2146697211)
                errorCode = "INET_E_RESOURCE_NOT_FOUND";
                errorType = wxWEB_NAV_ERR_NOT_FOUND;
                break;
            case INET_E_OBJECT_NOT_FOUND: // (0x800C0006L or -2146697210)
                errorCode = "INET_E_OBJECT_NOT_FOUND";
                errorType = wxWEB_NAV_ERR_NOT_FOUND;
                break;
            case INET_E_DATA_NOT_AVAILABLE: // (0x800C0007L or -2146697209)
                errorCode = "INET_E_DATA_NOT_AVAILABLE";
                errorType = wxWEB_NAV_ERR_NOT_FOUND;
                break;
            case INET_E_DOWNLOAD_FAILURE: // (0x800C0008L or -2146697208)
                errorCode = "INET_E_DOWNLOAD_FAILURE";
                errorType = wxWEB_NAV_ERR_CONNECTION;
                break;
            case INET_E_AUTHENTICATION_REQUIRED: // (0x800C0009L or -2146697207)
                errorCode = "INET_E_AUTHENTICATION_REQUIRED";
                errorType = wxWEB_NAV_ERR_AUTH;
                break;
            case INET_E_NO_VALID_MEDIA: // (0x800C000AL or -2146697206)
                errorCode = "INET_E_NO_VALID_MEDIA";
                errorType = wxWEB_NAV_ERR_REQUEST;
                break;
            case INET_E_CONNECTION_TIMEOUT: // (0x800C000BL or -2146697205)
                errorCode = "INET_E_CONNECTION_TIMEOUT";
                errorType = wxWEB_NAV_ERR_CONNECTION;
                break;
            case INET_E_INVALID_REQUEST: // (0x800C000CL or -2146697204)
                errorCode = "INET_E_INVALID_REQUEST";
                errorType = wxWEB_NAV_ERR_REQUEST;
                break;
            case INET_E_UNKNOWN_PROTOCOL: // (0x800C000DL or -2146697203)
                errorCode = "INET_E_UNKNOWN_PROTOCOL";
                errorType = wxWEB_NAV_ERR_REQUEST;
                break;
            case INET_E_SECURITY_PROBLEM: // (0x800C000EL or -2146697202)
                errorCode = "INET_E_SECURITY_PROBLEM";
                errorType = wxWEB_NAV_ERR_SECURITY;
                break;
            case INET_E_CANNOT_LOAD_DATA: // (0x800C000FL or -2146697201)
                errorCode = "INET_E_CANNOT_LOAD_DATA";
                errorType = wxWEB_NAV_ERR_OTHER;
                break;
            case INET_E_CANNOT_INSTANTIATE_OBJECT:
                // CoCreateInstance will return an error code if this happens,
                // we'll handle this above.
                return;
                break;
            case INET_E_REDIRECT_FAILED: // (0x800C0014L or -2146697196)
                errorCode = "INET_E_REDIRECT_FAILED";
                errorType = wxWEB_NAV_ERR_OTHER;
                break;
            case INET_E_REDIRECT_TO_DIR: // (0x800C0015L or -2146697195)
                errorCode = "INET_E_REDIRECT_TO_DIR";
                errorType = wxWEB_NAV_ERR_REQUEST;
                break;
            case INET_E_CANNOT_LOCK_REQUEST: // (0x800C0016L or -2146697194)
                errorCode = "INET_E_CANNOT_LOCK_REQUEST";
                errorType = wxWEB_NAV_ERR_OTHER;
                break;
            case INET_E_USE_EXTEND_BINDING: // (0x800C0017L or -2146697193)
                errorCode = "INET_E_USE_EXTEND_BINDING";
                errorType = wxWEB_NAV_ERR_OTHER;
                break;
            case INET_E_TERMINATED_BIND: // (0x800C0018L or -2146697192)
                errorCode = "INET_E_TERMINATED_BIND";
                errorType = wxWEB_NAV_ERR_OTHER;
                break;
            case INET_E_INVALID_CERTIFICATE: // (0x800C0019L or -2146697191)
                errorCode = "INET_E_INVALID_CERTIFICATE";
                errorType = wxWEB_NAV_ERR_CERTIFICATE;
                break;
            case INET_E_CODE_DOWNLOAD_DECLINED: // (0x800C0100L or -2146696960)
                errorCode = "INET_E_CODE_DOWNLOAD_DECLINED";
                errorType = wxWEB_NAV_ERR_USER_CANCELLED;
                break;
            case INET_E_RESULT_DISPATCHED: // (0x800C0200L or -2146696704)
                // cancel request cancelled...
                errorCode = "INET_E_RESULT_DISPATCHED";
                errorType = wxWEB_NAV_ERR_OTHER;
                break;
            case INET_E_CANNOT_REPLACE_SFP_FILE: // (0x800C0300L or -2146696448)
                errorCode = "INET_E_CANNOT_REPLACE_SFP_FILE";
                errorType = wxWEB_NAV_ERR_SECURITY;
                break;
            case INET_E_CODE_INSTALL_BLOCKED_BY_HASH_POLICY:
                errorCode = "INET_E_CODE_INSTALL_BLOCKED_BY_HASH_POLICY";
                errorType = wxWEB_NAV_ERR_SECURITY;
                break;
            case INET_E_CODE_INSTALL_SUPPRESSED:
                errorCode = "INET_E_CODE_INSTALL_SUPPRESSED";
                errorType = wxWEB_NAV_ERR_SECURITY;
                break;
            }

            wxString url = evt[1].GetString();
            wxString target = evt[2].GetString();
            wxWebViewEvent event(wxEVT_COMMAND_WEB_VIEW_ERROR, GetId(),
                                 url, target);
            event.SetEventObject(this);
            event.SetInt(errorType);
            event.SetString(errorCode);
            HandleWindowEvent(event);
            break;
        }
        case DISPID_NEWWINDOW3:
        {
            wxString url = evt[4].GetString();

            wxWebViewEvent event(wxEVT_COMMAND_WEB_VIEW_NEWWINDOW,
                                 GetId(), url, wxEmptyString);
            event.SetEventObject(this);
            HandleWindowEvent(event);

            //We always cancel this event otherwise an Internet Exporer window
            //is opened for the url
            wxActiveXEventNativeMSW* nativeParams = evt.GetNativeParameters();
            *V_BOOLREF(&nativeParams->pDispParams->rgvarg[3]) = VARIANT_TRUE;
            break;
        }
    }

    evt.Skip();
}

VirtualProtocol::VirtualProtocol(wxSharedPtr<wxWebViewHandler> handler)
{
    m_file = NULL;
    m_handler = handler;
}

BEGIN_IID_TABLE(VirtualProtocol)
    ADD_IID(Unknown)
    ADD_RAW_IID(wxIID_IInternetProtocolRoot)
    ADD_RAW_IID(wxIID_IInternetProtocol)
END_IID_TABLE;

IMPLEMENT_IUNKNOWN_METHODS(VirtualProtocol)

HRESULT VirtualProtocol::Start(LPCWSTR szUrl, wxIInternetProtocolSink *pOIProtSink,
                               wxIInternetBindInfo *pOIBindInfo, DWORD grfPI,
                               HANDLE_PTR dwReserved)
{
    wxUnusedVar(szUrl);
    wxUnusedVar(pOIBindInfo);
    wxUnusedVar(grfPI);
    wxUnusedVar(dwReserved);
    m_protocolSink = pOIProtSink;

    //We get the file itself from the protocol handler
    m_file = m_handler->GetFile(szUrl);


    if(!m_file)
        return INET_E_RESOURCE_NOT_FOUND;

    //We return the stream length for current and total size as we can always
    //read the whole file from the stream
    wxFileOffset length = m_file->GetStream()->GetLength();
    m_protocolSink->ReportData(wxBSCF_FIRSTDATANOTIFICATION |
                               wxBSCF_DATAFULLYAVAILABLE |
                               wxBSCF_LASTDATANOTIFICATION,
                               length, length);
    return S_OK;
}

HRESULT VirtualProtocol::Read(void *pv, ULONG cb, ULONG *pcbRead)
{
    //If the file is null we return false to indicte it is finished
    if(!m_file)
        return S_FALSE;

    wxStreamError err = m_file->GetStream()->Read(pv, cb).GetLastError();
    *pcbRead = m_file->GetStream()->LastRead();

    if(err == wxSTREAM_NO_ERROR)
    {
        if(*pcbRead < cb)
        {
            wxDELETE(m_file);
            m_protocolSink->ReportResult(S_OK, 0, NULL);
        }
        //As we are not eof there is more data
        return S_OK;
    }
    else if(err == wxSTREAM_EOF)
    {
        wxDELETE(m_file);
        m_protocolSink->ReportResult(S_OK, 0, NULL);
        //We are eof and so finished
        return S_OK;
    }
    else if(err ==  wxSTREAM_READ_ERROR)
    {
        wxDELETE(m_file);
        return INET_E_DOWNLOAD_FAILURE;
    }
    else
    {
        //Dummy return to surpress a compiler warning
        wxFAIL;
        return INET_E_DOWNLOAD_FAILURE;
    }
}

BEGIN_IID_TABLE(ClassFactory)
    ADD_IID(Unknown)
    ADD_IID(ClassFactory)
END_IID_TABLE;

IMPLEMENT_IUNKNOWN_METHODS(ClassFactory)

HRESULT ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid,
                                     void ** ppvObject)
{
    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;
    VirtualProtocol* vp = new VirtualProtocol(m_handler);
    vp->AddRef();
    HRESULT hr = vp->QueryInterface(riid, ppvObject);
    vp->Release();
    return hr;

}

STDMETHODIMP ClassFactory::LockServer(BOOL fLock)
{
    wxUnusedVar(fLock);
    return S_OK;
}

wxIEContainer::wxIEContainer(wxWindow *parent, REFIID iid, IUnknown *pUnk, 
                             DocHostUIHandler* uiHandler) :
    wxActiveXContainer(parent,iid,pUnk)
{
    m_uiHandler = uiHandler;
}

wxIEContainer::~wxIEContainer()
{
}

bool wxIEContainer::QueryClientSiteInterface(REFIID iid, void **_interface, 
                                             const char *&desc)
{
    if (m_uiHandler && IsEqualIID(iid, wxIID_IDocHostUIHandler))
    {
        *_interface = (IUnknown *) (wxIDocHostUIHandler *) m_uiHandler;
        desc = "IDocHostUIHandler";
        return true;
    }
    return false;
}

HRESULT DocHostUIHandler::ShowContextMenu(DWORD dwID, POINT *ppt, 
                                          IUnknown *pcmdtReserved, 
                                          IDispatch *pdispReserved)
{
    wxUnusedVar(dwID);
    wxUnusedVar(ppt);
    wxUnusedVar(pcmdtReserved);
    wxUnusedVar(pdispReserved);
    return E_NOTIMPL;
}

HRESULT DocHostUIHandler::GetHostInfo(DOCHOSTUIINFO *pInfo)
{
    //don't show 3d border and ebales themes.
    pInfo->dwFlags = pInfo->dwFlags | DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_THEME;
    return S_OK;
}

HRESULT DocHostUIHandler::ShowUI(DWORD dwID,
                                 IOleInPlaceActiveObject *pActiveObject, 
                                 IOleCommandTarget *pCommandTarget,
                                 IOleInPlaceFrame *pFrame,
                                 IOleInPlaceUIWindow *pDoc)
{
    wxUnusedVar(dwID);
    wxUnusedVar(pActiveObject);
    wxUnusedVar(pCommandTarget);
    wxUnusedVar(pFrame);
    wxUnusedVar(pDoc);
    return S_FALSE;
}

HRESULT DocHostUIHandler::HideUI(void)
{
    return E_NOTIMPL;
}

HRESULT DocHostUIHandler::UpdateUI(void)
{
    return E_NOTIMPL;
}

HRESULT DocHostUIHandler::EnableModeless(BOOL fEnable)
{
    wxUnusedVar(fEnable);
    return E_NOTIMPL;
}

HRESULT DocHostUIHandler::OnDocWindowActivate(BOOL fActivate)
{
    wxUnusedVar(fActivate);
    return E_NOTIMPL;
}

HRESULT DocHostUIHandler::OnFrameWindowActivate(BOOL fActivate)
{
    wxUnusedVar(fActivate);
    return E_NOTIMPL;
}

HRESULT DocHostUIHandler::ResizeBorder(LPCRECT prcBorder, 
                                       IOleInPlaceUIWindow *pUIWindow,
                                       BOOL fFrameWindow)
{
    wxUnusedVar(prcBorder);
    wxUnusedVar(pUIWindow);
    wxUnusedVar(fFrameWindow);
    return E_NOTIMPL;
}

HRESULT DocHostUIHandler::TranslateAccelerator(LPMSG lpMsg, 
                                               const GUID *pguidCmdGroup,
                                               DWORD nCmdID)
{
    if(lpMsg && lpMsg->message == WM_KEYDOWN)
    {
        //control is down?
        if((GetKeyState(VK_CONTROL) & 0x8000 ))
        {
            //skip CTRL-N, CTRL-F and CTRL-P
            if(lpMsg->wParam == 'N' || lpMsg->wParam == 'P' || lpMsg->wParam == 'F')
            {
                return S_OK;
            }
        }
        //skip F5
        if(lpMsg->wParam == VK_F5)
        {
            return S_OK;
        }
    }

    wxUnusedVar(pguidCmdGroup);
    wxUnusedVar(nCmdID);
    return E_NOTIMPL;
}

HRESULT DocHostUIHandler::GetOptionKeyPath(LPOLESTR *pchKey,DWORD dw)
{
    wxUnusedVar(pchKey);
    wxUnusedVar(dw);
    return E_NOTIMPL;
}

HRESULT DocHostUIHandler::GetDropTarget(IDropTarget *pDropTarget,
                                        IDropTarget **ppDropTarget)
{
    wxUnusedVar(pDropTarget);
    wxUnusedVar(ppDropTarget);
    return E_NOTIMPL;
}

HRESULT DocHostUIHandler::GetExternal(IDispatch **ppDispatch)
{
    wxUnusedVar(ppDispatch);
    return E_NOTIMPL;
}

HRESULT DocHostUIHandler::TranslateUrl(DWORD dwTranslate,
                                       OLECHAR *pchURLIn,
                                       OLECHAR **ppchURLOut)
{
    wxUnusedVar(dwTranslate);
    wxUnusedVar(pchURLIn);
    wxUnusedVar(ppchURLOut);
    return E_NOTIMPL;
}

HRESULT DocHostUIHandler::FilterDataObject(IDataObject *pDO, IDataObject **ppDORet)
{
    wxUnusedVar(pDO);
    wxUnusedVar(ppDORet);
    return E_NOTIMPL;
}

BEGIN_IID_TABLE(DocHostUIHandler)
    ADD_IID(Unknown)
    ADD_RAW_IID(wxIID_IDocHostUIHandler)
END_IID_TABLE;

IMPLEMENT_IUNKNOWN_METHODS(DocHostUIHandler)

#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_IE
