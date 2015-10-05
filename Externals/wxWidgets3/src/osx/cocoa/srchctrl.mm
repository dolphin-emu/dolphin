///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/srchctrl.mm
// Purpose:     implements mac carbon wxSearchCtrl
// Author:      Vince Harron
// Created:     2006-02-19
// Copyright:   Vince Harron
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_SEARCHCTRL

#include "wx/srchctrl.h"

#ifndef WX_PRECOMP
    #include "wx/menu.h"
#endif //WX_PRECOMP

#if wxUSE_NATIVE_SEARCH_CONTROL

#include "wx/osx/private.h"
#include "wx/osx/cocoa/private/textimpl.h"


@interface wxNSSearchField : NSSearchField
{
}

@end

@implementation wxNSSearchField

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    return self;
}
 
- (void)controlTextDidChange:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
        impl->controlTextDidChange();
}

- (NSArray *)control:(NSControl *)control textView:(NSTextView *)textView completions:(NSArray *)words
 forPartialWordRange:(NSRange)charRange indexOfSelectedItem:(int*)index
{
    NSMutableArray* matches = NULL;
    NSString*       partialString;
    
    partialString = [[textView string] substringWithRange:charRange];
    matches       = [NSMutableArray array];
    
    // wxTextWidgetImpl* impl = (wxTextWidgetImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    wxArrayString completions;
    
    // adapt to whatever strategy we have for getting the strings
    // impl->GetTextEntry()->GetCompletions(wxCFStringRef::AsString(partialString), completions);
    
    for (size_t i = 0; i < completions.GetCount(); ++i )
        [matches addObject: wxCFStringRef(completions[i]).AsNSString()];
    
    // [matches sortUsingSelector:@selector(compare:)];
    
    
    return matches;
}

@end

// ============================================================================
// wxMacSearchFieldControl
// ============================================================================

class wxNSSearchFieldControl : public wxNSTextFieldControl, public wxSearchWidgetImpl
{
public :
    wxNSSearchFieldControl( wxTextCtrl *wxPeer, wxNSSearchField* w  ) : wxNSTextFieldControl(wxPeer, w)
    {
        m_searchFieldCell = [w cell];
        m_searchField = w;
    }
    ~wxNSSearchFieldControl();

    // search field options
    virtual void ShowSearchButton( bool show ) wxOVERRIDE
    {
        if ( show )
            [m_searchFieldCell resetSearchButtonCell];
        else
            [m_searchFieldCell setSearchButtonCell:nil];
        [m_searchField setNeedsDisplay:YES];
    }

    virtual bool IsSearchButtonVisible() const wxOVERRIDE
    {
        return [m_searchFieldCell searchButtonCell] != nil;
    }

    virtual void ShowCancelButton( bool show ) wxOVERRIDE
    {
        if ( show )
            [m_searchFieldCell resetCancelButtonCell];
        else
            [m_searchFieldCell setCancelButtonCell:nil];
        [m_searchField setNeedsDisplay:YES];
    }

    virtual bool IsCancelButtonVisible() const wxOVERRIDE
    {
        return [m_searchFieldCell cancelButtonCell] != nil;
    }

    virtual void SetSearchMenu( wxMenu* menu ) wxOVERRIDE
    {
        if ( menu )
            [m_searchFieldCell setSearchMenuTemplate:menu->GetHMenu()];
        else
            [m_searchFieldCell setSearchMenuTemplate:nil];
        [m_searchField setNeedsDisplay:YES];
    }

    virtual void SetDescriptiveText(const wxString& text) wxOVERRIDE
    {
        [m_searchFieldCell setPlaceholderString:
            wxCFStringRef( text , m_wxPeer->GetFont().GetEncoding() ).AsNSString()];
    }

    virtual bool SetFocus() wxOVERRIDE
    {
       return  wxNSTextFieldControl::SetFocus();
    }

    void controlAction( WXWidget WXUNUSED(slf), void *WXUNUSED(_cmd), void *WXUNUSED(sender)) wxOVERRIDE
    {
        wxSearchCtrl* wxpeer = (wxSearchCtrl*) GetWXPeer();
        if ( wxpeer )
        {
            NSString *searchString = [m_searchField stringValue];
            if ( searchString == nil || !searchString.length )
            {
                wxpeer->HandleSearchFieldCancelHit();
            }
            else
            {
                wxpeer->HandleSearchFieldSearchHit();
            }
        }
    }

    virtual void SetCentredLook( bool centre )
    {
        SEL sel = @selector(setCenteredLook:);
        if ( [m_searchFieldCell respondsToSelector: sel] )
        {
            // all this avoids xcode parsing warnings when using
            // [m_searchFieldCell setCenteredLook:NO];
            NSMethodSignature* signature =
            [NSSearchFieldCell instanceMethodSignatureForSelector:sel];
            NSInvocation* invocation =
            [NSInvocation invocationWithMethodSignature: signature];
            [invocation setTarget: m_searchFieldCell];
            [invocation setSelector:sel];
            [invocation setArgument:&centre atIndex:2];
            [invocation invoke];
        }
    }

private:
    wxNSSearchField* m_searchField;
    NSSearchFieldCell* m_searchFieldCell;
} ;

wxNSSearchFieldControl::~wxNSSearchFieldControl()
{
}

wxWidgetImplType* wxWidgetImpl::CreateSearchControl( wxSearchCtrl* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxString& str,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSSearchField* v = [[wxNSSearchField alloc] initWithFrame:r];

    // Make it behave consistently with the single line wxTextCtrl
    [[v cell] setScrollable:YES];

    [[v cell] setSendsWholeSearchString:YES];
    // per wx default cancel is not shown
    [[v cell] setCancelButtonCell:nil];

    wxNSSearchFieldControl* c = new wxNSSearchFieldControl( wxpeer, v );
    c->SetNeedsFrame( false );
    c->SetCentredLook( false );
    c->SetStringValue( str );
    return c;
}

#endif // wxUSE_NATIVE_SEARCH_CONTROL

#endif // wxUSE_SEARCHCTRL
