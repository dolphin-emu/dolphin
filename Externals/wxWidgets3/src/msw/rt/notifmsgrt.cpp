/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/notifmsgrt.cpp
// Purpose:     WinRT implementation of wxNotificationMessageImpl
// Author:      Tobias Taschner
// Created:     2015-09-13
// Copyright:   (c) 2015 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/string.h"
#endif // WX_PRECOMP

#include "wx/msw/rt/private/notifmsg.h"

#if wxUSE_NOTIFICATION_MESSAGE && wxUSE_WINRT
#include "wx/notifmsg.h"
#include "wx/msw/rt/utils.h"
#include "wx/msw/private/comptr.h"
#include "wx/msw/wrapshl.h"
#include "wx/msw/ole/oleutils.h"

#include "wx/filename.h"
#include "wx/stdpaths.h"

#include <roapi.h>
#include <windows.ui.notifications.h>
#include <functiondiscoverykeys.h>
#include <propvarutil.h>
#include <wrl/implements.h>

using namespace ABI::Windows::UI::Notifications;
using namespace ABI::Windows::Data::Xml::Dom;

namespace rt = wxWinRT;

typedef ABI::Windows::Foundation::ITypedEventHandler<ToastNotification *, ::IInspectable *> DesktopToastActivatedEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ToastNotification *, ToastDismissedEventArgs *> DesktopToastDismissedEventHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<ToastNotification *, ToastFailedEventArgs *> DesktopToastFailedEventHandler;

class wxToastNotifMsgImpl;

class wxToastEventHandler :
    public Microsoft::WRL::Implements<DesktopToastActivatedEventHandler, DesktopToastDismissedEventHandler, DesktopToastFailedEventHandler>
{
public:
    wxToastEventHandler(wxToastNotifMsgImpl* toastImpl) :
        m_impl(toastImpl)
    {

    }

    void Detach()
    {
        m_impl = NULL;
    }

    // DesktopToastActivatedEventHandler
    IFACEMETHODIMP Invoke(IToastNotification *sender, IInspectable* args);

    // DesktopToastDismissedEventHandler
    IFACEMETHODIMP Invoke(IToastNotification *sender, IToastDismissedEventArgs *e);

    // DesktopToastFailedEventHandler
    IFACEMETHODIMP Invoke(IToastNotification *sender, IToastFailedEventArgs *e);

    // IUnknown
    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return ++m_cRef;
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        if ( --m_cRef == wxAutoULong(0) )
        {
            delete this;
            return 0;
        }
        else
            return m_cRef;
    }

    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if ( IsEqualIID(riid, IID_IUnknown) )
            *ppv = static_cast<IUnknown*>(static_cast<DesktopToastActivatedEventHandler*>(this));
        else if ( IsEqualIID(riid, __uuidof(DesktopToastActivatedEventHandler)) )
            *ppv = static_cast<DesktopToastActivatedEventHandler*>(this);
        else if ( IsEqualIID(riid, __uuidof(DesktopToastDismissedEventHandler)) )
            *ppv = static_cast<DesktopToastDismissedEventHandler*>(this);
        else if ( IsEqualIID(riid, __uuidof(DesktopToastFailedEventHandler)) )
            *ppv = static_cast<DesktopToastFailedEventHandler*>(this);
        else
            *ppv = NULL;

        if ( *ppv )
        {
            reinterpret_cast<IUnknown*>(*ppv)->AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }

private:
    wxAutoULong m_cRef;

    wxToastNotifMsgImpl* m_impl;
};

class wxToastNotifMsgImpl : public wxNotificationMessageImpl
{
public:
    wxToastNotifMsgImpl(wxNotificationMessageBase* notification) :
        wxNotificationMessageImpl(notification),
        m_toastEventHandler(NULL)
    {

    }

    virtual ~wxToastNotifMsgImpl()
    {
        if ( m_toastEventHandler )
            m_toastEventHandler->Detach();
    }

    virtual bool Show(int WXUNUSED(timeout)) wxOVERRIDE
    {
        wxCOMPtr<IXmlDocument> toastXml;
        HRESULT hr = CreateToastXML(&toastXml);
        if ( SUCCEEDED(hr) )
        {
            hr = CreateToast(toastXml);
        }

        return SUCCEEDED(hr);
    }

    virtual bool Close() wxOVERRIDE
    {
        if ( m_notifier.get() && m_toast.get() )
        {
            bool success = SUCCEEDED(m_notifier->Hide(m_toast));
            ReleaseToast();
            return success;
        }
        else
            return false;
    }

    virtual void SetTitle(const wxString& title) wxOVERRIDE
    {
        m_title = title;
    }

    virtual void SetMessage(const wxString& message) wxOVERRIDE
    {
        m_message = message;
    }

    virtual void SetParent(wxWindow *WXUNUSED(parent)) wxOVERRIDE
    {

    }

    virtual void SetFlags(int WXUNUSED(flags)) wxOVERRIDE
    {

    }

    virtual void SetIcon(const wxIcon& WXUNUSED(icon)) wxOVERRIDE
    {
        // Icon would have to be saved to disk (temporarily?)
        // to be used as a file:// url in the notifications XML
    }

    virtual bool AddAction(wxWindowID WXUNUSED(actionid), const wxString &WXUNUSED(label)) wxOVERRIDE
    {
        return false;
    }

    void ReleaseToast()
    {
        if ( m_toastEventHandler )
            m_toastEventHandler->Detach();
        m_notifier = NULL;
        m_toast = NULL;
    }

    HRESULT CreateToast(IXmlDocument *xml)
    {
        HRESULT hr = ms_toastMgr->CreateToastNotifierWithId(rt::TempStringRef::Make(ms_appId), &m_notifier);
        if ( SUCCEEDED(hr) )
        {
            wxCOMPtr<IToastNotificationFactory> factory;
            hr = rt::GetActivationFactory(RuntimeClass_Windows_UI_Notifications_ToastNotification,
                IID_IToastNotificationFactory, reinterpret_cast<void**>(&factory));
            if ( SUCCEEDED(hr) )
            {
                hr = factory->CreateToastNotification(xml, &m_toast);
                if ( SUCCEEDED(hr) )
                {
                    // Register the event handlers
                    EventRegistrationToken activatedToken, dismissedToken, failedToken;
                    m_toastEventHandler = new wxToastEventHandler(this);
                    wxCOMPtr<wxToastEventHandler> eventHandler(m_toastEventHandler);

                    hr = m_toast->add_Activated(eventHandler, &activatedToken);
                    if ( SUCCEEDED(hr) )
                    {
                        hr = m_toast->add_Dismissed(eventHandler, &dismissedToken);
                        if ( SUCCEEDED(hr) )
                        {
                            hr = m_toast->add_Failed(eventHandler, &failedToken);
                            if ( SUCCEEDED(hr) )
                            {
                                hr = m_notifier->Show(m_toast);
                            }
                        }
                    }
                }
            }
        }

        if ( FAILED(hr) )
            ReleaseToast();

        return hr;
    }

    HRESULT CreateToastXML(IXmlDocument** toastXml) const
    {
        HRESULT hr = ms_toastMgr->GetTemplateContent(ToastTemplateType_ToastText02, toastXml);
        if ( SUCCEEDED(hr) )
        {
            wxCOMPtr<IXmlNodeList> nodeList;
            hr = (*toastXml)->GetElementsByTagName(rt::TempStringRef::Make("text"), &nodeList);
            if ( SUCCEEDED(hr) )
            {
                hr = SetNodeListValueString(0, m_title, nodeList, *toastXml);
                if ( SUCCEEDED(hr) )
                    hr = SetNodeListValueString(1, m_message, nodeList, *toastXml);
            }
        }

        return hr;
    }

    static HRESULT SetNodeListValueString(UINT32 index, const wxString& str, IXmlNodeList* nodeList, IXmlDocument *toastXml)
    {
        wxCOMPtr<IXmlNode> textNode;
        // Set title node
        HRESULT hr = nodeList->Item(index, &textNode);
        if ( SUCCEEDED(hr) )
        {
            hr = SetNodeValueString(str, textNode, toastXml);
        }

        return hr;
    }

    static HRESULT SetNodeValueString(const wxString& str, IXmlNode *node, IXmlDocument *xml)
    {
        wxCOMPtr<IXmlText> inputText;

        HRESULT hr = xml->CreateTextNode(rt::TempStringRef::Make(str), &inputText);
        if ( SUCCEEDED(hr) )
        {
            wxCOMPtr<IXmlNode> inputTextNode;

            hr = inputText->QueryInterface(IID_IXmlNode, reinterpret_cast<void**>(&inputTextNode));
            if ( SUCCEEDED(hr) )
            {
                wxCOMPtr<IXmlNode> pAppendedChild;
                hr = node->AppendChild(inputTextNode, &pAppendedChild);
            }
        }

        return hr;
    }

    static bool IsEnabled()
    {
        return ms_enabled;
    }

    static wxString BuildAppId()
    {
        // Build a Application User Model IDs based on app info
        wxString vendorId = wxTheApp->GetVendorName();
        if ( vendorId.empty() )
            vendorId = "wxWidgetsApp";
        wxString appId = vendorId + "." + wxTheApp->GetAppName();
        // Remove potential spaces
        appId.Replace(" ", "", true);
        return appId;
    }

    static bool CheckShortcut(const wxFileName& filename)
    {
        // Prepare interfaces
        wxCOMPtr<IShellLink> shellLink;
        if ( FAILED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
            IID_IShellLinkW, reinterpret_cast<void**>(&shellLink))) )
            return false;
        wxCOMPtr<IPersistFile> persistFile;
        if ( FAILED(shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persistFile))) )
            return false;
        wxCOMPtr<IPropertyStore> propertyStore;
        if ( FAILED(shellLink->QueryInterface(IID_IPropertyStore, reinterpret_cast<void**>(&propertyStore))) )
            return false;

        bool writeShortcut = false;

        if ( filename.Exists() )
        {
            // Check existing shortcut for application id
            if ( SUCCEEDED(persistFile->Load(filename.GetFullPath().wc_str(), 0)) )
            {
                PROPVARIANT appIdPropVar;
                if ( SUCCEEDED(propertyStore->GetValue(PKEY_AppUserModel_ID, &appIdPropVar)) )
                {
                    wxString appId;
                    if ( appIdPropVar.vt == VT_LPWSTR )
                        appId = appIdPropVar.pwszVal;
                    if ( appId.empty() || (!ms_appId.empty() && ms_appId != appId) )
                    {
                        // Update shortcut if app id does not match or is empty
                        writeShortcut = true;
                    }
                    else if ( ms_appId.empty() )
                    {
                        // Use if no app id has been set
                        ms_appId = appId;
                    }
                }
            }
            else
                return false;
        }
        else
        {
            // Create new shortcut
            if ( FAILED(shellLink->SetPath(wxStandardPaths::Get().GetExecutablePath().t_str())) )
                return false;
            if ( FAILED(shellLink->SetArguments(wxT(""))) )
                return false;

            writeShortcut = true;
        }

        if ( writeShortcut )
        {
            if ( ms_appId.empty() )
                ms_appId = BuildAppId();

            // Set application id in shortcut
            PROPVARIANT appIdPropVar;
            if ( FAILED(InitPropVariantFromString(ms_appId.wc_str(), &appIdPropVar)) )
                return false;
            if ( FAILED(propertyStore->SetValue(PKEY_AppUserModel_ID, appIdPropVar)) )
                return false;
            if ( FAILED(propertyStore->Commit()) )
                return false;
            if ( FAILED(persistFile->Save(filename.GetFullPath().wc_str(), TRUE)) )
                return false;
        }

        return true;
    }

    static bool UseToasts(
        const wxString& shortcutPath,
        const wxString& appId)
    {
        ms_enabled = false;

        // WinRT runtime is required (available since Win8)
        if ( !rt::IsAvailable() )
            return false;

        // Toast notification manager has to be available
        if ( ms_toastStaticsInitialized == -1 )
        {
            if ( SUCCEEDED(rt::GetActivationFactory(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager,
                IID_IToastNotificationManagerStatics, reinterpret_cast<void**>(&ms_toastMgr))) )
            {
                ms_toastStaticsInitialized = 1;
            }
            else
                ms_toastStaticsInitialized = 0;
        }

        if ( ms_toastStaticsInitialized != 1 )
            return false;

        // Build/complete shortcut path
        wxFileName shortcutFilename(shortcutPath);
        if ( !shortcutFilename.HasName() )
            shortcutFilename.SetName(wxTheApp->GetAppDisplayName());
        if ( !shortcutFilename.HasExt() )
            shortcutFilename.SetExt("lnk");
        if ( shortcutFilename.IsRelative() )
            shortcutFilename.MakeAbsolute(wxStandardPaths::MSWGetShellDir(CSIDL_STARTMENU));

        ms_appId = appId;

        if ( CheckShortcut(shortcutFilename) )
            ms_enabled = true;

        return ms_enabled;
    }

    static void Uninitalize()
    {
        if (ms_toastStaticsInitialized == 1)
        {
            ms_toastMgr = NULL;
            ms_toastStaticsInitialized = -1;
        }
    }

private:
    wxString m_title;
    wxString m_message;
    wxCOMPtr<IToastNotifier> m_notifier;
    wxCOMPtr<IToastNotification> m_toast;
    wxToastEventHandler* m_toastEventHandler;

    static bool ms_enabled;
    static wxString ms_appId;
    static int ms_toastStaticsInitialized;
    static wxCOMPtr<IToastNotificationManagerStatics> ms_toastMgr;

    friend class wxToastEventHandler;
};

bool wxToastNotifMsgImpl::ms_enabled = false;
int wxToastNotifMsgImpl::ms_toastStaticsInitialized = -1;
wxString wxToastNotifMsgImpl::ms_appId;
wxCOMPtr<IToastNotificationManagerStatics> wxToastNotifMsgImpl::ms_toastMgr;

HRESULT wxToastEventHandler::Invoke(
    IToastNotification *WXUNUSED(sender),
    IInspectable *WXUNUSED(args))
{
    if ( m_impl )
    {
        wxCommandEvent evt(wxEVT_NOTIFICATION_MESSAGE_CLICK);
        m_impl->ProcessNotificationEvent(evt);
    }

    return S_OK;
}

HRESULT wxToastEventHandler::Invoke(
    IToastNotification *WXUNUSED(sender),
    IToastDismissedEventArgs *WXUNUSED(e))
{
    if ( m_impl )
    {
        wxCommandEvent evt(wxEVT_NOTIFICATION_MESSAGE_DISMISSED);
        m_impl->ProcessNotificationEvent(evt);
    }

    return S_OK;
}

HRESULT wxToastEventHandler::Invoke(IToastNotification *WXUNUSED(sender),
    IToastFailedEventArgs *WXUNUSED(e))
{
    //TODO: Handle toast failed event
    return S_OK;
}

//
// wxToastNotifMsgModule
//

class wxToastNotifMsgModule : public wxModule
{
public:
    wxToastNotifMsgModule()
    {
    }

    virtual bool OnInit() wxOVERRIDE
    {
        return true;
    }

    virtual void OnExit() wxOVERRIDE
    {
        wxToastNotifMsgImpl::Uninitalize();
    }

private:
    wxDECLARE_DYNAMIC_CLASS(wxToastNotifMsgModule);
};

wxIMPLEMENT_DYNAMIC_CLASS(wxToastNotifMsgModule, wxModule);

#endif // wxUSE_NOTIFICATION_MESSAGE && wxUSE_WINRT

//
// wxToastNotificationHelper
//

bool wxToastNotificationHelper::UseToasts(const wxString& shortcutPath,
    const wxString& appId)
{
#if wxUSE_NOTIFICATION_MESSAGE && wxUSE_WINRT
    return wxToastNotifMsgImpl::UseToasts(shortcutPath, appId);
#else
    wxUnusedVar(shortcutPath);
    wxUnusedVar(appId);
    return false;
#endif
}

bool wxToastNotificationHelper::IsEnabled()
{
#if wxUSE_NOTIFICATION_MESSAGE && wxUSE_WINRT
    return wxToastNotifMsgImpl::IsEnabled();
#else
    return false;
#endif
}

wxNotificationMessageImpl* wxToastNotificationHelper::CreateInstance(wxNotificationMessageBase* notification)
{
#if wxUSE_NOTIFICATION_MESSAGE && wxUSE_WINRT
    return new wxToastNotifMsgImpl(notification);
#else
    wxUnusedVar(notification);
    return NULL;
#endif
}
