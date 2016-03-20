/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/combobox.mm
// Purpose:     wxChoice
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_COMBOBOX

#include "wx/combobox.h"
#include "wx/evtloop.h"

#ifndef WX_PRECOMP
    #include "wx/menu.h"
    #include "wx/dcclient.h"
#endif

#include "wx/osx/cocoa/private/textimpl.h"

// work in progress

@interface wxNSTableDataSource : NSObject <NSComboBoxDataSource>
{
    wxNSComboBoxControl* impl;
}

- (NSInteger)numberOfItemsInComboBox:(NSComboBox *)aComboBox;
- (id)comboBox:(NSComboBox *)aComboBox objectValueForItemAtIndex:(NSInteger)index;

@end


@implementation wxNSComboBox

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized) 
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (void) dealloc
{
    [fieldEditor release];
    [super dealloc];
}

// Over-riding NSComboBox onKeyDown method doesn't work for key events.
// Ensure that we can use our own wxNSTextFieldEditor to catch key events.
// See windowWillReturnFieldEditor in nonownedwnd.mm.
// Key events will be caught and handled via wxNSTextFieldEditor onkey...
// methods in textctrl.mm.

- (void) setFieldEditor:(wxNSTextFieldEditor*) editor
{
    if ( editor != fieldEditor )
    {
        [editor retain];
        [fieldEditor release];
        fieldEditor = editor;
    }
}

- (wxNSTextFieldEditor*) fieldEditor
{
    return fieldEditor;
}

- (void)controlTextDidChange:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl && impl->ShouldSendEvents() )
    {
        wxWindow* wxpeer = (wxWindow*) impl->GetWXPeer();
        if ( wxpeer ) {
            wxCommandEvent event(wxEVT_TEXT, wxpeer->GetId());
            event.SetEventObject( wxpeer );
            event.SetString( static_cast<wxComboBox*>(wxpeer)->GetValue() );
            wxpeer->HandleWindowEvent( event );
        }
    }
}

- (void)controlTextDidEndEditing:(NSNotification *) aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
    {
        wxNSTextFieldControl* timpl = dynamic_cast<wxNSTextFieldControl*>(impl);
        if ( timpl )
            timpl->UpdateInternalSelectionFromEditor(fieldEditor);
        impl->DoNotifyFocusLost();
    }
}

- (void)comboBoxWillPopUp:(NSNotification *)notification
{
    wxUnusedVar(notification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if( impl && impl->ShouldSendEvents() )
    {
        wxComboBox* wxpeer = static_cast<wxComboBox*>(impl->GetWXPeer());
        if( wxpeer )
        {
            wxCommandEvent event(wxEVT_COMBOBOX_DROPDOWN, wxpeer->GetId());
            event.SetEventObject( wxpeer );
            wxpeer->GetEventHandler()->ProcessEvent( event );
        }
    }
}

- (void)comboBoxWillDismiss:(NSNotification *)notification
{
    wxUnusedVar(notification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if( impl && impl->ShouldSendEvents() )
    {
        wxComboBox* wxpeer = static_cast<wxComboBox*>(impl->GetWXPeer());
        if( wxpeer )
        {
            wxCommandEvent event(wxEVT_COMBOBOX_CLOSEUP, wxpeer->GetId());
            event.SetEventObject( wxpeer );
            wxpeer->GetEventHandler()->ProcessEvent( event );
        }
    }
}

- (void)comboBoxSelectionDidChange:(NSNotification *)notification
{
    wxUnusedVar(notification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl && impl->ShouldSendEvents())
    {
        wxComboBox* wxpeer = static_cast<wxComboBox*>(impl->GetWXPeer());
        if ( wxpeer ) {
            const int sel = wxpeer->GetSelection();

            wxCommandEvent event(wxEVT_COMBOBOX, wxpeer->GetId());
            event.SetEventObject( wxpeer );
            event.SetInt( sel );
            event.SetString( wxpeer->GetString(sel) );
            // For some reason, wxComboBox::GetValue will not return the newly selected item 
            // while we're inside this callback, so use AddPendingEvent to make sure
            // GetValue() returns the right value.

            wxpeer->GetEventHandler()->AddPendingEvent( event );

        }
    }
}
@end

wxNSComboBoxControl::wxNSComboBoxControl( wxComboBox *wxPeer, WXWidget w )
    : wxNSTextFieldControl(wxPeer, wxPeer, w)
{
    m_comboBox = (NSComboBox*)w;
}

wxNSComboBoxControl::~wxNSComboBoxControl()
{
}

void wxNSComboBoxControl::mouseEvent(WX_NSEvent event, WXWidget slf, void *_cmd)
{
    // NSComboBox has its own event loop, which reacts very badly to our synthetic
    // events used to signal when a wxEvent is posted, so during that time we switch
    // the wxEventLoop::WakeUp implementation to a lower-level version
    
    bool reset = false;
    wxEventLoop* const loop = (wxEventLoop*) wxEventLoopBase::GetActive();

    if ( loop != NULL && [event type] == NSLeftMouseDown )
    {
        reset = true;
        loop->OSXUseLowLevelWakeup(true);
    }
    
    wxOSX_EventHandlerPtr superimpl = (wxOSX_EventHandlerPtr) [[slf superclass] instanceMethodForSelector:(SEL)_cmd];
    superimpl(slf, (SEL)_cmd, event);
 
    if ( reset )
    {
        loop->OSXUseLowLevelWakeup(false);
    }
}

int wxNSComboBoxControl::GetSelectedItem() const
{
    return [m_comboBox indexOfSelectedItem];
}

void wxNSComboBoxControl::SetSelectedItem(int item)
{
    SendEvents(false);

    if ( item != wxNOT_FOUND )
    {
        wxASSERT_MSG( item >= 0 && item < [m_comboBox numberOfItems],
                      "Inavlid item index." );
        [m_comboBox selectItemAtIndex: item];
    }
    else // remove current selection (if we have any)
    {
        const int sel = GetSelectedItem();
        if ( sel != wxNOT_FOUND )
            [m_comboBox deselectItemAtIndex:sel];
    }

    SendEvents(true);
}

int wxNSComboBoxControl::GetNumberOfItems() const
{
    return [m_comboBox numberOfItems];
}

void wxNSComboBoxControl::InsertItem(int pos, const wxString& item)
{
    [m_comboBox insertItemWithObjectValue:wxCFStringRef( item , m_wxPeer->GetFont().GetEncoding() ).AsNSString() atIndex:pos];
}

void wxNSComboBoxControl::RemoveItem(int pos)
{
    SendEvents(false);
    [m_comboBox removeItemAtIndex:pos];
    SendEvents(true);
}

void wxNSComboBoxControl::Clear()
{
    SendEvents(false);
    [m_comboBox removeAllItems];
    [m_comboBox setStringValue:@""];
    SendEvents(true);
}

wxString wxNSComboBoxControl::GetStringAtIndex(int pos) const
{
    return wxCFStringRef::AsString([m_comboBox itemObjectValueAtIndex:pos], m_wxPeer->GetFont().GetEncoding());
}

int wxNSComboBoxControl::FindString(const wxString& text) const
{
    NSInteger nsresult = [m_comboBox indexOfItemWithObjectValue:wxCFStringRef( text , m_wxPeer->GetFont().GetEncoding() ).AsNSString()];

    int result;
    if (nsresult == NSNotFound)
        result = wxNOT_FOUND;
    else
        result = (int) nsresult;
    return result;
}

void wxNSComboBoxControl::Popup()
{
    id ax = NSAccessibilityUnignoredDescendant(m_comboBox);
    [ax accessibilitySetValue: [NSNumber numberWithBool: YES] forAttribute: NSAccessibilityExpandedAttribute];
}

void wxNSComboBoxControl::Dismiss()
{
    id ax = NSAccessibilityUnignoredDescendant(m_comboBox);
    [ax accessibilitySetValue: [NSNumber numberWithBool: NO] forAttribute: NSAccessibilityExpandedAttribute];
}

void wxNSComboBoxControl::SetEditable(bool editable)
{
    // TODO: unfortunately this does not work, setEditable just means the same as CB_READONLY
    // I don't see a way to access the text field directly
    
    // Behavior NONE <- SELECTECTABLE
    [m_comboBox setEditable:editable];
}

wxWidgetImplType* wxWidgetImpl::CreateComboBox( wxComboBox* wxpeer, 
                                    wxWindowMac* WXUNUSED(parent), 
                                    wxWindowID WXUNUSED(id), 
                                    wxMenu* WXUNUSED(menu),
                                    const wxPoint& pos, 
                                    const wxSize& size,
                                    long style, 
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSComboBox* v = [[wxNSComboBox alloc] initWithFrame:r];
    [v setNumberOfVisibleItems:13];
    if (style & wxCB_READONLY)
        [v setEditable:NO];
    wxNSComboBoxControl* c = new wxNSComboBoxControl( wxpeer, v );
    return c;
}

wxSize wxComboBox::DoGetBestSize() const
{
    int lbWidth = GetCount() > 0 ? 20 : 100;  // some defaults
    wxSize baseSize = wxWindow::DoGetBestSize();
    int lbHeight = baseSize.y;
    int wLine;
    
    {
        wxClientDC dc(const_cast<wxComboBox*>(this));
        
        // Find the widest line
        for(unsigned int i = 0; i < GetCount(); i++)
        {
            wxString str(GetString(i));
            
            wxCoord width, height ;
            dc.GetTextExtent( str , &width, &height);
            wLine = width ;
            
            lbWidth = wxMax( lbWidth, wLine ) ;
        }
        
        // Add room for the popup arrow
        lbWidth += 2 * lbHeight ;
    }
    
    return wxSize( lbWidth, lbHeight );
}

#endif // wxUSE_COMBOBOX
