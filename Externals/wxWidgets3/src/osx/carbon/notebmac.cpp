///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/notebmac.cpp
// Purpose:     implementation of wxNotebook
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
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


// check that the page index is valid
#define IS_VALID_PAGE(nPage) ((nPage) < GetPageCount())

class wxCarbonTabView : public wxMacControl
{
public:
    wxCarbonTabView( wxWindowMac* peer ) : wxMacControl(peer )
    {
    }
    
    // Added by Mark Newsam
    // When a page is added or deleted to the notebook this function updates
    // information held in the control so that it matches the order
    // the user would expect.
    //
    void SetupTabs( const wxNotebook& notebook)
    {
        const size_t countPages = notebook.GetPageCount();
        SetMaximum( countPages ) ;
        
        wxNotebookPage *page;
        ControlTabInfoRecV1 info;
        
        for (size_t ii = 0; ii < countPages; ii++)
        {
            page = (wxNotebookPage*) notebook.GetPage(ii);
            info.version = kControlTabInfoVersionOne;
            info.iconSuiteID = 0;
            wxCFStringRef cflabel( page->GetLabel(), page->GetFont().GetEncoding() ) ;
            info.name = cflabel ;
            SetData<ControlTabInfoRecV1>( ii + 1, kControlTabInfoTag, &info ) ;
            
            if ( notebook.GetImageList() && notebook.GetPageImage(ii) >= 0 )
            {
                const wxBitmap bmap = notebook.GetImageList()->GetBitmap( notebook.GetPageImage( ii ) ) ;
                if ( bmap.IsOk() )
                {
                    ControlButtonContentInfo info ;
                    
                    wxMacCreateBitmapButton( &info, bmap ) ;
                    
                    OSStatus err = SetData<ControlButtonContentInfo>( ii + 1, kControlTabImageContentTag, &info );
                    if ( err != noErr )
                    {
                        wxFAIL_MSG("Error when setting icon on tab");
                    }
                    
                    wxMacReleaseBitmapButton( &info ) ;
                }
            }
            SetTabEnabled( ii + 1, true ) ;
        }
    }
    
    int TabHitTest(const wxPoint & pt, long* flags)
    {
        int resultV = wxNOT_FOUND;
        
        wxNotebook *notebookpeer = wxDynamicCast( GetWXPeer() , wxNotebook );
        if ( NULL == notebookpeer )
            return wxNOT_FOUND;
        
        const int countPages = notebookpeer->GetPageCount();
        
        // we have to convert from Client to Window relative coordinates
        wxPoint adjustedPt = pt + GetWXPeer()->GetClientAreaOrigin();
        // and now to HIView native ones
        adjustedPt.x -= GetWXPeer()->MacGetLeftBorderSize() ;
        adjustedPt.y -= GetWXPeer()->MacGetTopBorderSize() ;
        
        HIPoint hipoint= { adjustedPt.x , adjustedPt.y } ;
        HIViewPartCode outPart = 0 ;
        OSStatus err = HIViewGetPartHit( GetControlRef(), &hipoint, &outPart );
        
        int max = GetMaximum() ;
        if ( outPart == 0 && max > 0 )
        {
            // this is a hack, as unfortunately a hit on an already selected tab returns 0,
            // so we have to go some extra miles to make sure we select something different
            // and try again ..
            int val = GetValue() ;
            int maxval = max ;
            if ( max == 1 )
            {
                SetMaximum( 2 ) ;
                maxval = 2 ;
            }
            
            if ( val == 1 )
                SetValue( maxval ) ;
            else
                SetValue( 1 ) ;
            
            err = HIViewGetPartHit( GetControlRef(), &hipoint, &outPart );
            
            SetValue( val ) ;
            if ( max == 1 )
                SetMaximum( 1 ) ;
        }
        
        if ( outPart >= 1 && outPart <= countPages )
            resultV = outPart - 1 ;
        
        if (flags != NULL)
        {
            *flags = 0;
            
            // we cannot differentiate better
            if (resultV >= 0)
                *flags |= wxBK_HITTEST_ONLABEL;
            else
                *flags |= wxBK_HITTEST_NOWHERE;
        }
        
        return resultV;
        
    }
    
};

wxWidgetImplType* wxWidgetImpl::CreateTabView( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID WXUNUSED(id),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    Rect bounds = wxMacGetBoundsForControl( wxpeer, pos, size );

    if ( bounds.right <= bounds.left )
        bounds.right = bounds.left + 100;
    if ( bounds.bottom <= bounds.top )
        bounds.bottom = bounds.top + 100;

    UInt16 tabstyle = kControlTabDirectionNorth;
    if ( style & wxBK_LEFT )
        tabstyle = kControlTabDirectionWest;
    else if ( style & wxBK_RIGHT )
        tabstyle = kControlTabDirectionEast;
    else if ( style & wxBK_BOTTOM )
        tabstyle = kControlTabDirectionSouth;

    ControlTabSize tabsize;
    switch (wxpeer->GetWindowVariant())
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

    wxMacControl* peer = new wxCarbonTabView( wxpeer );
    
    OSStatus err = CreateTabsControl(
        MAC_WXHWND(parent->MacGetTopLevelWindowRef()), &bounds,
        tabsize, tabstyle, 0, NULL, peer->GetControlRefAddr() );
    verify_noerr( err );

    return peer;
}

#endif
