///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/notebook_osx.cpp
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

wxBEGIN_EVENT_TABLE(wxNotebook, wxBookCtrlBase)
    EVT_SIZE(wxNotebook::OnSize)
    EVT_SET_FOCUS(wxNotebook::OnSetFocus)
    EVT_NAVIGATION_KEY(wxNotebook::OnNavigationKey)
wxEND_EVENT_TABLE()

bool wxNotebook::Create( wxWindow *parent,
    wxWindowID id,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name )
{    
    DontCreatePeer();
    
    if (! (style & wxBK_ALIGN_MASK))
        style |= wxBK_TOP;

    if ( !wxNotebookBase::Create( parent, id, pos, size, style, name ) )
        return false;

    SetPeer(wxWidgetImpl::CreateTabView(this,parent, id, pos, size, style, GetExtraStyle() ));

    MacPostControlCreate( pos, size );

    return true ;
}

// dtor
wxNotebook::~wxNotebook()
{
}

// ----------------------------------------------------------------------------
// wxNotebook accessors
// ----------------------------------------------------------------------------

void wxNotebook::SetPadding(const wxSize& WXUNUSED(padding))
{
    // unsupported by OS
}

void wxNotebook::SetTabSize(const wxSize& WXUNUSED(sz))
{
    // unsupported by OS
}

void wxNotebook::SetPageSize(const wxSize& size)
{
    SetSize( CalcSizeFromPage( size ) );
}

wxSize wxNotebook::CalcSizeFromPage(const wxSize& sizePage) const
{
    return DoGetSizeFromClientSize( sizePage );
}

int wxNotebook::DoSetSelection(size_t nPage, int flags)
{
    wxCHECK_MSG( IS_VALID_PAGE(nPage), wxNOT_FOUND, wxT("DoSetSelection: invalid notebook page") );

    if ( m_selection == wxNOT_FOUND || nPage != (size_t)m_selection )
    {
        if ( flags & SetSelection_SendEvent )
        {
            if ( !SendPageChangingEvent(nPage) )
            {
                // vetoed by program
                return m_selection;
            }
            //else: program allows the page change
        }

        // m_selection is set to newSel in ChangePage()
        // so store its value for event use.
        int oldSelection = m_selection;
        ChangePage(oldSelection, nPage);

        if ( flags & SetSelection_SendEvent )
            SendPageChangedEvent(oldSelection, nPage);
    }
    //else: no change

    return m_selection;
}

bool wxNotebook::SetPageText(size_t nPage, const wxString& strText)
{
    wxCHECK_MSG( IS_VALID_PAGE(nPage), false, wxT("SetPageText: invalid notebook page") );

    wxNotebookPage *page = m_pages[nPage];
    page->SetLabel(wxStripMenuCodes(strText));
    MacSetupTabs();

    return true;
}

wxString wxNotebook::GetPageText(size_t nPage) const
{
    wxCHECK_MSG( IS_VALID_PAGE(nPage), wxEmptyString, wxT("GetPageText: invalid notebook page") );

    wxNotebookPage *page = m_pages[nPage];

    return page->GetLabel();
}

int wxNotebook::GetPageImage(size_t nPage) const
{
    wxCHECK_MSG( IS_VALID_PAGE(nPage), wxNOT_FOUND, wxT("GetPageImage: invalid notebook page") );

    return m_images[nPage];
}

bool wxNotebook::SetPageImage(size_t nPage, int nImage)
{
    wxCHECK_MSG( IS_VALID_PAGE(nPage), false,
        wxT("SetPageImage: invalid notebook page") );
    wxCHECK_MSG( HasImageList() && nImage < GetImageList()->GetImageCount(), false,
        wxT("SetPageImage: invalid image index") );

    if ( nImage != m_images[nPage] )
    {
        // if the item didn't have an icon before or, on the contrary, did have
        // it but has lost it now, its size will change - but if the icon just
        // changes, it won't
        m_images[nPage] = nImage;

        MacSetupTabs() ;
    }

    return true;
}

// ----------------------------------------------------------------------------
// wxNotebook operations
// ----------------------------------------------------------------------------

// remove one page from the notebook, without deleting the window
wxNotebookPage* wxNotebook::DoRemovePage(size_t nPage)
{
    wxCHECK_MSG( IS_VALID_PAGE(nPage), NULL,
        wxT("DoRemovePage: invalid notebook page") );

    wxNotebookPage* page = m_pages[nPage] ;
    m_pages.RemoveAt(nPage);
    m_images.RemoveAt(nPage);

    MacSetupTabs();

    if ( m_selection >= (int)nPage )
    {
        if ( GetPageCount() == 0 )
            m_selection = wxNOT_FOUND;
        else
            m_selection = m_selection ? m_selection - 1 : 0;

        GetPeer()->SetValue( m_selection + 1 ) ;
    }

    if (m_selection >= 0)
        m_pages[m_selection]->Show(true);

    InvalidateBestSize();

    return page;
}

// remove all pages
bool wxNotebook::DeleteAllPages()
{
    WX_CLEAR_ARRAY(m_pages);
    m_images.clear();
    MacSetupTabs();
    m_selection = wxNOT_FOUND ;
    InvalidateBestSize();

    return true;
}

// same as AddPage() but does it at given position
bool wxNotebook::InsertPage(size_t nPage,
    wxNotebookPage *pPage,
    const wxString& strText,
    bool bSelect,
    int imageId )
{
    if ( !wxNotebookBase::InsertPage( nPage, pPage, strText, bSelect, imageId ) )
        return false;

    wxASSERT_MSG( pPage->GetParent() == this, wxT("notebook pages must have notebook as parent") );

    // don't show pages by default (we'll need to adjust their size first)
    pPage->Show( false ) ;

    pPage->SetLabel( wxStripMenuCodes(strText) );

    m_images.Insert( imageId, nPage );

    MacSetupTabs();

    wxRect rect = GetPageRect() ;
    pPage->SetSize( rect );
    if ( pPage->GetAutoLayout() )
        pPage->Layout();

    // now deal with the selection
    // ---------------------------

    // if the inserted page is before the selected one, we must update the
    // index of the selected page

    if ( int(nPage) <= m_selection )
    {
        m_selection++;

        // while this still is the same page showing, we need to update the tabs
        GetPeer()->SetValue( m_selection + 1 ) ;
    }

    DoSetSelectionAfterInsertion(nPage, bSelect);

    InvalidateBestSize();

    return true;
}

int wxNotebook::HitTest(const wxPoint& pt, long *flags) const
{
    return GetPeer()->TabHitTest(pt,flags);
}

// Added by Mark Newsam
// When a page is added or deleted to the notebook this function updates
// information held in the control so that it matches the order
// the user would expect.
//
void wxNotebook::MacSetupTabs()
{
    GetPeer()->SetupTabs(*this);
    Refresh();
}

wxRect wxNotebook::GetPageRect() const
{
    wxSize size = GetClientSize() ;

    return wxRect( 0 , 0 , size.x , size.y ) ;
}

// ----------------------------------------------------------------------------
// wxNotebook callbacks
// ----------------------------------------------------------------------------

// @@@ OnSize() is used for setting the font when it's called for the first
//     time because doing it in ::Create() doesn't work (for unknown reasons)
void wxNotebook::OnSize(wxSizeEvent& event)
{
    unsigned int nCount = m_pages.Count();
    wxRect rect = GetPageRect() ;

    for ( unsigned int nPage = 0; nPage < nCount; nPage++ )
    {
        wxNotebookPage *pPage = m_pages[nPage];
        pPage->SetSize(rect, wxSIZE_FORCE_EVENT);
    }

#if 0 // deactivate r65078 for the moment
    // If the selected page is hidden at this point, the notebook
    // has become visible for the first time after creation, and
    // we postponed showing the page in ChangePage().
    // So show the selected page now.
    if ( m_selection != wxNOT_FOUND )
    {
        wxNotebookPage *pPage = m_pages[m_selection];
        if ( !pPage->IsShown() )
        {
            pPage->Show( true );
            pPage->SetFocus();
        }
    }
#endif

    // Processing continues to next OnSize
    event.Skip();
}

void wxNotebook::OnSetFocus(wxFocusEvent& event)
{
    // set focus to the currently selected page if any
    if ( m_selection != wxNOT_FOUND )
        m_pages[m_selection]->SetFocus();

    event.Skip();
}

void wxNotebook::OnNavigationKey(wxNavigationKeyEvent& event)
{
    if ( event.IsWindowChange() )
    {
        // change pages
        AdvanceSelection( event.GetDirection() );
    }
    else
    {
        // we get this event in 2 cases
        //
        // a) one of our pages might have generated it because the user TABbed
        // out from it in which case we should propagate the event upwards and
        // our parent will take care of setting the focus to prev/next sibling
        //
        // or
        //
        // b) the parent panel wants to give the focus to us so that we
        // forward it to our selected page. We can't deal with this in
        // OnSetFocus() because we don't know which direction the focus came
        // from in this case and so can't choose between setting the focus to
        // first or last panel child
        wxWindow *parent = GetParent();

        // the cast is here to fix a GCC ICE
        if ( ((wxWindow*)event.GetEventObject()) == parent )
        {
            // no, it doesn't come from child, case (b): forward to a page
            if ( m_selection != wxNOT_FOUND )
            {
                // so that the page knows that the event comes from it's parent
                // and is being propagated downwards
                event.SetEventObject( this );

                wxWindow *page = m_pages[m_selection];
                if ( !page->HandleWindowEvent( event ) )
                {
                    page->SetFocus();
                }
                //else: page manages focus inside it itself
            }
            else
            {
                // we have no pages - still have to give focus to _something_
                SetFocus();
            }
        }
        else
        {
            // it comes from our child, case (a), pass to the parent
            if ( parent )
            {
                event.SetCurrentFocus( this );
                parent->HandleWindowEvent( event );
            }
        }
    }
}

// ----------------------------------------------------------------------------
// wxNotebook base class virtuals
// ----------------------------------------------------------------------------

#if wxUSE_CONSTRAINTS

// override these 2 functions to do nothing: everything is done in OnSize

void wxNotebook::SetConstraintSizes(bool WXUNUSED(recurse))
{
    // don't set the sizes of the pages - their correct size is not yet known
    wxControl::SetConstraintSizes( false );
}

bool wxNotebook::DoPhase(int WXUNUSED(nPhase))
{
    return true;
}

#endif // wxUSE_CONSTRAINTS

void wxNotebook::Command(wxCommandEvent& WXUNUSED(event))
{
    wxFAIL_MSG(wxT("wxNotebook::Command not implemented"));
}

// ----------------------------------------------------------------------------
// wxNotebook helper functions
// ----------------------------------------------------------------------------

// hide the currently active panel and show the new one
void wxNotebook::ChangePage(int nOldSel, int nSel)
{
    if (nOldSel == nSel)
        return;

    if ( nOldSel != wxNOT_FOUND )
        m_pages[nOldSel]->Show( false );

    if ( nSel != wxNOT_FOUND )
    {
        wxNotebookPage *pPage = m_pages[nSel];
#if 0 // deactivate r65078 for the moment
        if ( IsShownOnScreen() )
        {
            pPage->Show( true );
            pPage->SetFocus();
        }
        else
        {
            // Postpone Show() until the control is actually shown.
            // Otherwise this forces the containing toplevel window
            // to show, even if it's just being created and called
            // AddPage() without intent to show the window yet.
            // We Show() the selected page in our OnSize handler,
            // unless it already is shown.
        }
#else
        pPage->Show( true );
        pPage->SetFocus();
#endif
    }

    m_selection = nSel;
    GetPeer()->SetValue( m_selection + 1 ) ;
}

bool wxNotebook::OSXHandleClicked( double WXUNUSED(timestampsec) )
{
    bool status = false ;

    SInt32 newSel = GetPeer()->GetValue() - 1 ;
    if ( newSel != m_selection )
    {
        if ( DoSetSelection(newSel, SetSelection_SendEvent ) != newSel )
            GetPeer()->SetValue( m_selection + 1 ) ;

        status = true ;
    }

    return status ;
}

#endif
