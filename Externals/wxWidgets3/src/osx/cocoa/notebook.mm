///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/notebook.mm
// Purpose:     implementation of wxNotebook
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: notebook.mm 67887 2011-06-08 22:48:29Z SC $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_NOTEBOOK

#include "wx/notebook.h"

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/image.h"
#endif

#include "wx/string.h"
#include "wx/imaglist.h"
#include "wx/osx/private.h"

//
// controller
//

@interface wxTabViewController : NSObject wxOSX_10_6_AND_LATER(<NSTabViewDelegate>)
{
}

- (BOOL)tabView:(NSTabView *)tabView shouldSelectTabViewItem:(NSTabViewItem *)tabViewItem;
- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem;

@end

@interface wxNSTabView : NSTabView
{
}

@end

@implementation wxTabViewController

- (id) init
{
    self = [super init];
    return self;
}

- (BOOL)tabView:(NSTabView *)tabView shouldSelectTabViewItem:(NSTabViewItem *)tabViewItem
{
    wxUnusedVar(tabViewItem);
    wxNSTabView* view = (wxNSTabView*) tabView;
    wxWidgetCocoaImpl* viewimpl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( view );

    if ( viewimpl )
    {
        // wxNotebook* wxpeer = (wxNotebook*) viewimpl->GetWXPeer();
    }
    return YES;
}

- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem
{
    wxUnusedVar(tabViewItem);
    wxNSTabView* view = (wxNSTabView*) tabView;
    wxWidgetCocoaImpl* viewimpl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( view );
    if ( viewimpl )
    {
        wxNotebook* wxpeer = (wxNotebook*) viewimpl->GetWXPeer();
        wxpeer->OSXHandleClicked(0);
    }
}

@end

@implementation wxNSTabView

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

@end

class wxCocoaTabView : public wxWidgetCocoaImpl
{
public:
    wxCocoaTabView( wxWindowMac* peer , WXWidget w ) : wxWidgetCocoaImpl(peer, w)
    {
    }

    void GetContentArea( int &left , int &top , int &width , int &height ) const
    {
        wxNSTabView* slf = (wxNSTabView*) m_osxView;
        NSRect r = [slf contentRect];
        left = (int)r.origin.x;
        top = (int)r.origin.y;
        width = (int)r.size.width;
        height = (int)r.size.height;
    }

    void SetValue( wxInt32 value )
    {
        wxNSTabView* slf = (wxNSTabView*) m_osxView;
        // avoid 'changed' events when setting the tab programmatically
        wxTabViewController* controller = [slf delegate];
        [slf setDelegate:nil];
        [slf selectTabViewItemAtIndex:(value-1)];
        [slf setDelegate:controller];
    }

    wxInt32 GetValue() const
    {
        wxNSTabView* slf = (wxNSTabView*) m_osxView;
        NSTabViewItem* selectedItem = [slf selectedTabViewItem];
        if ( selectedItem == nil )
            return 0;
        else
            return [slf indexOfTabViewItem:selectedItem]+1;
    }

    void SetMaximum( wxInt32 maximum )
    {
        wxNSTabView* slf = (wxNSTabView*) m_osxView;
        int cocoacount = [slf numberOfTabViewItems ];
        // avoid 'changed' events when setting the tab programmatically
        wxTabViewController* controller = [slf delegate];
        [slf setDelegate:nil];

        if ( maximum > cocoacount )
        {
            for ( int i = cocoacount ; i < maximum ; ++i )
            {
                NSTabViewItem* item = [[NSTabViewItem alloc] init];
                [slf addTabViewItem:item];
                [item release];
            }
        }
        else if ( maximum < cocoacount )
        {
            for ( int i = cocoacount -1 ; i >= maximum ; --i )
            {
                NSTabViewItem* item = [(wxNSTabView*) m_osxView tabViewItemAtIndex:i];
                [slf removeTabViewItem:item];
            }
        }
        [slf setDelegate:controller];
    }

    void SetupTabs( const wxNotebook& notebook)
    {
        int pcount = notebook.GetPageCount();

        SetMaximum( pcount );

        for ( int i = 0 ; i < pcount ; ++i )
        {
            wxNotebookPage* page = notebook.GetPage(i);
            NSTabViewItem* item = [(wxNSTabView*) m_osxView tabViewItemAtIndex:i];
            [item setView:page->GetHandle() ];
            wxCFStringRef cf( page->GetLabel() , notebook.GetFont().GetEncoding() );
            [item setLabel:cf.AsNSString()];
            if ( notebook.GetImageList() && notebook.GetPageImage(i) >= 0 )
            {
                const wxBitmap bmap = notebook.GetImageList()->GetBitmap( notebook.GetPageImage( i ) ) ;
                if ( bmap.IsOk() )
                {
                    // TODO how to set an image on a tab
                }
            }
        }
    }
};


/*
#if 0
    Rect bounds = wxMacGetBoundsForControl( this, pos, size );

    if ( bounds.right <= bounds.left )
        bounds.right = bounds.left + 100;
    if ( bounds.bottom <= bounds.top )
        bounds.bottom = bounds.top + 100;

    UInt16 tabstyle = kControlTabDirectionNorth;
    if ( HasFlag(wxBK_LEFT) )
        tabstyle = kControlTabDirectionWest;
    else if ( HasFlag( wxBK_RIGHT ) )
        tabstyle = kControlTabDirectionEast;
    else if ( HasFlag( wxBK_BOTTOM ) )
        tabstyle = kControlTabDirectionSouth;

    ControlTabSize tabsize;
    switch (GetWindowVariant())
    {
        case wxWINDOW_VARIANT_MINI:
            tabsize = 3 ;
            break;

        case wxWINDOW_VARIANT_SMALL:
            tabsize = kControlTabSizeSmall;
            break;

        default:
            tabsize = kControlTabSizeLarge;
            break;
    }

    m_peer = new wxMacControl( this );
    OSStatus err = CreateTabsControl(
        MAC_WXHWND(parent->MacGetTopLevelWindowRef()), &bounds,
        tabsize, tabstyle, 0, NULL, GetPeer()->GetControlRefAddr() );
    verify_noerr( err );
#endif
*/
wxWidgetImplType* wxWidgetImpl::CreateTabView( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    static wxTabViewController* controller = NULL;

    if ( !controller )
        controller =[[wxTabViewController alloc] init];

    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;

    NSTabViewType tabstyle = NSTopTabsBezelBorder;
    if ( style & wxBK_LEFT )
        tabstyle = NSLeftTabsBezelBorder;
    else if ( style & wxBK_RIGHT )
        tabstyle = NSRightTabsBezelBorder;
    else if ( style & wxBK_BOTTOM )
        tabstyle = NSBottomTabsBezelBorder;

    wxNSTabView* v = [[wxNSTabView alloc] initWithFrame:r];
    [v setTabViewType:tabstyle];
    wxWidgetCocoaImpl* c = new wxCocoaTabView( wxpeer, v );
    [v setDelegate: controller];
    return c;
}

#endif
