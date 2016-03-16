/////////////////////////////////////////////////////////////////////////
// File:        src/osx/cocoa/taskbar.mm
// Purpose:     Implements wxTaskBarIcon class
// Author:      David Elliott, Stefan Csomor
// Modified by:
// Created:     2004/01/24
// Copyright:   (c) 2004 David Elliott, Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"
#if wxUSE_TASKBARICON

#ifndef WX_PRECOMP
    #include "wx/toplevel.h"
    #include "wx/menu.h"
    #include "wx/icon.h"
    #include "wx/log.h"
    #include "wx/dcclient.h"
#endif

#include "wx/taskbar.h"

#include "wx/osx/private.h"

class wxTaskBarIconWindow;

//-----------------------------------------------------------------------------
//
//  wxTaskBarIconWindow
//
//  Event handler for menus
//  NB: Since wxWindows in Mac HAVE to have parents we need this to be
//  a top level window...
//-----------------------------------------------------------------------------

class wxTaskBarIconWindow : public wxTopLevelWindow
{
public:
    wxTaskBarIconWindow(wxTaskBarIconImpl *impl);

    void OnMenuEvent(wxCommandEvent& event);
    void OnUpdateUIEvent(wxUpdateUIEvent& event);
    
private:
    wxTaskBarIconImpl *m_impl;
    wxDECLARE_EVENT_TABLE();
};

// ============================================================================
// wxTaskBarIconImpl
//     Base class for the various Cocoa implementations.
// ============================================================================
class wxTaskBarIconImpl
{
public:
    wxTaskBarIconImpl(wxTaskBarIcon *taskBarIcon);
    
    virtual bool IsStatusItem() const { return false; }

    virtual bool SetIcon(const wxIcon& icon, const wxString& tooltip = wxEmptyString) = 0;
    virtual bool RemoveIcon() = 0;
    
    bool IsIconInstalled() const { return m_icon.IsOk(); }
        
    virtual bool PopupMenu(wxMenu *menu) = 0;
    virtual ~wxTaskBarIconImpl();
    inline wxTaskBarIcon* GetTaskBarIcon() { return m_taskBarIcon; }
    wxMenu * CreatePopupMenu()
    { return m_taskBarIcon->CreatePopupMenu(); }

    wxDECLARE_NO_COPY_CLASS(wxTaskBarIconImpl);

protected:
    wxTaskBarIcon *m_taskBarIcon;
    wxBitmap m_icon;
    wxTaskBarIconWindow *m_eventWindow;
private:
    wxTaskBarIconImpl();
};

// ============================================================================
// wxTaskBarIconDockImpl
//     An implementation using the Dock icon.
// ============================================================================
class wxTaskBarIconDockImpl: public wxTaskBarIconImpl
{
public:
    wxTaskBarIconDockImpl(wxTaskBarIcon *taskBarIcon);
    virtual ~wxTaskBarIconDockImpl();
    virtual bool SetIcon(const wxIcon& icon, const wxString& tooltip = wxEmptyString) wxOVERRIDE;
    virtual bool RemoveIcon() wxOVERRIDE;
    virtual bool PopupMenu(wxMenu *menu) wxOVERRIDE;

    static WX_NSMenu OSXGetDockHMenu();
protected:
    WX_NSMenu OSXDoGetDockHMenu();
    // There can be only one Dock icon, so make sure we keep it that way
    static wxTaskBarIconDockImpl *sm_dockIcon;
private:
    wxTaskBarIconDockImpl();
    wxMenu             *m_pMenu;
};

class wxTaskBarIconCustomStatusItemImpl;

@interface wxOSXStatusItemTarget : NSObject
{
    wxTaskBarIconCustomStatusItemImpl* impl;
}
@end

// ============================================================================
// wxTaskBarIconCustomStatusItemImpl
//     An implementation using an NSStatusItem with a custom NSView
// ============================================================================
class wxTaskBarIconCustomStatusItemImpl: public wxTaskBarIconImpl
{
public:
    wxTaskBarIconCustomStatusItemImpl(wxTaskBarIcon *taskBarIcon);
    virtual ~wxTaskBarIconCustomStatusItemImpl();
    
    virtual bool IsStatusItem() const wxOVERRIDE { return true; }

    virtual bool SetIcon(const wxIcon& icon, const wxString& tooltip = wxEmptyString) wxOVERRIDE;
    virtual bool RemoveIcon() wxOVERRIDE;
    virtual bool PopupMenu(wxMenu *menu) wxOVERRIDE;
protected:
    NSStatusItem *m_statusItem;
    wxOSXStatusItemTarget *m_target;
private:
    wxTaskBarIconCustomStatusItemImpl();
};

// ============================================================================
// wxTaskBarIcon implementation
//     The facade class.
// ============================================================================
wxIMPLEMENT_DYNAMIC_CLASS(wxTaskBarIcon, wxEvtHandler);

wxTaskBarIcon::wxTaskBarIcon(wxTaskBarIconType iconType)
{
    if(iconType == wxTBI_DOCK)
        m_impl = new wxTaskBarIconDockImpl(this);
    else if(iconType == wxTBI_CUSTOM_STATUSITEM)
        m_impl = new wxTaskBarIconCustomStatusItemImpl(this);
    else
    {   m_impl = NULL;
        wxFAIL_MSG(wxT("Invalid wxTaskBarIcon type"));
    }
}

wxTaskBarIcon::~wxTaskBarIcon()
{
    if ( m_impl )
    {
        if ( m_impl->IsIconInstalled() )
            m_impl->RemoveIcon();
        delete m_impl;
        m_impl = NULL;
    }
}

bool wxTaskBarIcon::OSXIsStatusItem()
{
    if ( m_impl )
        return m_impl->IsStatusItem();

    return false;
}

// Operations

bool wxTaskBarIcon::IsIconInstalled() const
{
    if ( m_impl )
        return m_impl->IsIconInstalled();
    
    return false;
}

bool wxTaskBarIcon::SetIcon(const wxIcon& icon, const wxString& tooltip)
{
    if ( m_impl )
        return m_impl->SetIcon(icon,tooltip);

    return false;
}

bool wxTaskBarIcon::RemoveIcon()
{
    if ( m_impl )
        return m_impl->RemoveIcon();

    return false;
}

bool wxTaskBarIcon::PopupMenu(wxMenu *menu)
{
    if ( m_impl )
        return m_impl->PopupMenu(menu);

    return false;
}

// ============================================================================
// wxTaskBarIconImpl
// ============================================================================

wxTaskBarIconImpl::wxTaskBarIconImpl(wxTaskBarIcon* taskBarIcon)
 : m_taskBarIcon(taskBarIcon), m_eventWindow(new wxTaskBarIconWindow(this))
{
}

wxTaskBarIconImpl::~wxTaskBarIconImpl()
{
    delete m_eventWindow;
}

// ============================================================================
// wxTaskBarIconDockImpl
// ============================================================================
wxTaskBarIconDockImpl *wxTaskBarIconDockImpl::sm_dockIcon = NULL;

wxTaskBarIconDockImpl::wxTaskBarIconDockImpl(wxTaskBarIcon *taskBarIcon)
:   wxTaskBarIconImpl(taskBarIcon)
{
    wxASSERT_MSG(!sm_dockIcon, wxT("You should never have more than one dock icon!"));
    sm_dockIcon = this;
    m_pMenu = NULL;
}

wxTaskBarIconDockImpl::~wxTaskBarIconDockImpl()
{
    if(sm_dockIcon == this)
        sm_dockIcon = NULL;
}

WX_NSMenu wxTaskBarIconDockImpl::OSXGetDockHMenu()
{
    if(sm_dockIcon)
        return sm_dockIcon->OSXDoGetDockHMenu();
    
    return nil;
}

WX_NSMenu wxTaskBarIconDockImpl::OSXDoGetDockHMenu()
{
    wxMenu *dockMenu = CreatePopupMenu();

    if(!dockMenu)
        return nil;
    
    wxDELETE(m_pMenu);

    m_pMenu = dockMenu;
    
    m_pMenu->SetInvokingWindow(m_eventWindow);
    
    m_pMenu->UpdateUI();
    
    return (WX_NSMenu)dockMenu->GetHMenu();
}

bool wxTaskBarIconDockImpl::SetIcon(const wxIcon& icon, const wxString& WXUNUSED(tooltip))
{
    m_icon.CopyFromIcon(icon);
    [[NSApplication sharedApplication] setApplicationIconImage:m_icon.GetNSImage()];
    return true;
}

bool wxTaskBarIconDockImpl::RemoveIcon()
{
    wxDELETE(m_pMenu);
    m_icon = wxBitmap();
    [[NSApplication sharedApplication] setApplicationIconImage:nil];
    return true;
}

bool wxTaskBarIconDockImpl::PopupMenu(wxMenu *WXUNUSED(menu))
{
    wxFAIL_MSG(wxT("You cannot force the Dock icon menu to popup"));
    return false;
}

@interface wxNSAppController(wxTaskBarIconNSApplicationDelegateCategory)
- (NSMenu*)applicationDockMenu:(NSApplication *)sender;
@end

@implementation wxNSAppController(wxTaskBarIconNSApplicationDelegateCategory)
- (NSMenu*)applicationDockMenu:(NSApplication *)sender
{
    wxUnusedVar(sender);
    
    return wxTaskBarIconDockImpl::OSXGetDockHMenu();
}
@end

// ============================================================================
// wxTaskBarIconCustomStatusItemImpl
// ============================================================================

@implementation wxOSXStatusItemTarget

- (void) clickedAction: (id) sender
{
    wxUnusedVar(sender);
    wxMenu *menu = impl->CreatePopupMenu();
    if (menu)
    {
        impl->PopupMenu(menu);
        delete menu;
    }    
}

- (void)setImplementation: (wxTaskBarIconCustomStatusItemImpl *) theImplementation
{
    impl = theImplementation;
}

- (wxTaskBarIconCustomStatusItemImpl*) implementation
{
    return impl;
}

@end


wxTaskBarIconCustomStatusItemImpl::wxTaskBarIconCustomStatusItemImpl(wxTaskBarIcon *taskBarIcon)
:   wxTaskBarIconImpl(taskBarIcon)
{
    m_statusItem = nil;
    m_target = nil;
}

wxTaskBarIconCustomStatusItemImpl::~wxTaskBarIconCustomStatusItemImpl()
{
}

bool wxTaskBarIconCustomStatusItemImpl::SetIcon(const wxIcon& icon, const wxString& tooltip)
{
    if(!m_statusItem)
    {
        m_statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
        [m_statusItem retain];

        m_target = [[wxOSXStatusItemTarget alloc] init];
        [m_target setImplementation:this];
        [m_statusItem setHighlightMode:YES];
        [m_statusItem setTarget:m_target];
        [m_statusItem setAction:@selector(clickedAction:)];
        [m_statusItem sendActionOn:NSLeftMouseDownMask];
    }

    m_icon.CopyFromIcon(icon);
    
    // status item doesn't scale automatically
    // first scale to optimal pixel resolution
    
    int dimension = wxMax( m_icon.GetHeight(), m_icon.GetWidth() );
    int target_dimension = 16 * wxOSXGetMainScreenContentScaleFactor();
    if ( dimension > target_dimension )
    {
        wxImage img = m_icon.ConvertToImage();
        int factor = (dimension+(target_dimension-1))/target_dimension;
        m_icon = img.ShrinkBy(factor, factor);
    }
    
    NSImage* nsimage = m_icon.GetNSImage();
    NSSize size = [nsimage size];
    
    // then scale to optimal point resolution
    
    dimension = wxMax(size.width,size.height);
    if ( dimension > 16 )
    {
        int factor = (dimension+15)/16;
        size.width /= factor;
        size.height /= factor;
        [nsimage setSize:size];
    }
    [m_statusItem setImage:nsimage];
    
    wxCFStringRef cfTooltip(tooltip);
    [m_statusItem setToolTip:cfTooltip.AsNSString()];
    return true;
}

bool wxTaskBarIconCustomStatusItemImpl::RemoveIcon()
{
    [m_statusItem release];
    m_statusItem = nil;
    [m_target release];
    m_target = nil;
    
    m_icon = wxBitmap();
    
    return true;
}

bool wxTaskBarIconCustomStatusItemImpl::PopupMenu(wxMenu *menu)
{
    wxASSERT(menu);

    menu->SetInvokingWindow(m_eventWindow);
    menu->UpdateUI();

    [m_statusItem popUpStatusItemMenu:(NSMenu*)menu->GetHMenu()];

    menu->SetInvokingWindow(NULL);
    return true;
}

// ============================================================================
// wxTaskBarIconWindow
// ============================================================================

wxBEGIN_EVENT_TABLE(wxTaskBarIconWindow, wxWindow)
    EVT_MENU(-1, wxTaskBarIconWindow::OnMenuEvent)
    EVT_UPDATE_UI(-1, wxTaskBarIconWindow::OnUpdateUIEvent)
wxEND_EVENT_TABLE()

wxTaskBarIconWindow::wxTaskBarIconWindow(wxTaskBarIconImpl *impl) 
: m_impl(impl)
{
}

void wxTaskBarIconWindow::OnMenuEvent(wxCommandEvent& event)
{
    m_impl->GetTaskBarIcon()->ProcessEvent(event);
}

void wxTaskBarIconWindow::OnUpdateUIEvent(wxUpdateUIEvent& event)
{
    m_impl->GetTaskBarIcon()->ProcessEvent(event);
}

#endif //def wxHAS_TASK_BAR_ICON
