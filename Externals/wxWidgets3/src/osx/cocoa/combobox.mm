/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/combobox.mm
// Purpose:     wxChoice
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: combobox.mm 67232 2011-03-18 15:10:15Z DS $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_COMBOBOX

#include "wx/combobox.h"

#ifndef WX_PRECOMP
    #include "wx/menu.h"
    #include "wx/dcclient.h"
#endif

#include "wx/osx/cocoa/private/textimpl.h"

// work in progress

@interface wxNSTableDataSource : NSObject wxOSX_10_6_AND_LATER(<NSComboBoxDataSource>)
{
    wxNSComboBoxControl* impl;
}

- (NSInteger)numberOfItemsInComboBox:(NSComboBox *)aComboBox;
- (id)comboBox:(NSComboBox *)aComboBox objectValueForItemAtIndex:(NSInteger)index;

@end


@interface wxNSComboBox : NSComboBox
{
}

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

- (void)controlTextDidChange:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl && impl->ShouldSendEvents() )
    {
        wxWindow* wxpeer = (wxWindow*) impl->GetWXPeer();
        if ( wxpeer ) {
            wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, wxpeer->GetId());
            event.SetEventObject( wxpeer );
            event.SetString( static_cast<wxComboBox*>(wxpeer)->GetValue() );
            wxpeer->HandleWindowEvent( event );
        }
    }
}

- (void)comboBoxSelectionDidChange:(NSNotification *)notification
{
    wxUnusedVar(notification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl && impl->ShouldSendEvents())
    {
        wxWindow* wxpeer = (wxWindow*) impl->GetWXPeer();
        if ( wxpeer ) {
            wxCommandEvent event(wxEVT_COMMAND_COMBOBOX_SELECTED, wxpeer->GetId());
            event.SetEventObject( wxpeer );
            event.SetInt( static_cast<wxComboBox*>(wxpeer)->GetSelection() );
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
