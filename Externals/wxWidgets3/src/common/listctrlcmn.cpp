////////////////////////////////////////////////////////////////////////////////
// Name:        src/common/listctrlcmn.cpp
// Purpose:     Common defines for wxListCtrl and wxListCtrl-based classes.
// Author:      Kevin Ollivier
// Created:     09/15/06
// RCS-ID:      $Id: listctrlcmn.cpp 70284 2012-01-07 15:09:51Z VZ $
// Copyright:   (c) Kevin Ollivier
// Licence:     wxWindows licence
////////////////////////////////////////////////////////////////////////////////

// =============================================================================
// declarations
// =============================================================================

// -----------------------------------------------------------------------------
// headers
// -----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_LISTCTRL

#include "wx/listctrl.h"

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
#endif

const char wxListCtrlNameStr[] = "listCtrl";

// ListCtrl events
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_BEGIN_DRAG, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_BEGIN_RDRAG, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_BEGIN_LABEL_EDIT, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_END_LABEL_EDIT, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_DELETE_ITEM, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_DELETE_ALL_ITEMS, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_ITEM_DESELECTED, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_KEY_DOWN, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_INSERT_ITEM, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_COL_CLICK, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_COL_RIGHT_CLICK, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_COL_BEGIN_DRAG, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_COL_DRAGGING, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_COL_END_DRAG, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_ITEM_RIGHT_CLICK, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_ITEM_MIDDLE_CLICK, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_ITEM_FOCUSED, wxListEvent );
wxDEFINE_EVENT( wxEVT_COMMAND_LIST_CACHE_HINT, wxListEvent );

// -----------------------------------------------------------------------------
// XTI
// -----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxListCtrlStyle )
wxBEGIN_FLAGS( wxListCtrlStyle )
// new style border flags, we put them first to
// use them for streaming out
wxFLAGS_MEMBER(wxBORDER_SIMPLE)
wxFLAGS_MEMBER(wxBORDER_SUNKEN)
wxFLAGS_MEMBER(wxBORDER_DOUBLE)
wxFLAGS_MEMBER(wxBORDER_RAISED)
wxFLAGS_MEMBER(wxBORDER_STATIC)
wxFLAGS_MEMBER(wxBORDER_NONE)

// old style border flags
wxFLAGS_MEMBER(wxSIMPLE_BORDER)
wxFLAGS_MEMBER(wxSUNKEN_BORDER)
wxFLAGS_MEMBER(wxDOUBLE_BORDER)
wxFLAGS_MEMBER(wxRAISED_BORDER)
wxFLAGS_MEMBER(wxSTATIC_BORDER)
wxFLAGS_MEMBER(wxBORDER)

// standard window styles
wxFLAGS_MEMBER(wxTAB_TRAVERSAL)
wxFLAGS_MEMBER(wxCLIP_CHILDREN)
wxFLAGS_MEMBER(wxTRANSPARENT_WINDOW)
wxFLAGS_MEMBER(wxWANTS_CHARS)
wxFLAGS_MEMBER(wxFULL_REPAINT_ON_RESIZE)
wxFLAGS_MEMBER(wxALWAYS_SHOW_SB )
wxFLAGS_MEMBER(wxVSCROLL)
wxFLAGS_MEMBER(wxHSCROLL)

wxFLAGS_MEMBER(wxLC_LIST)
wxFLAGS_MEMBER(wxLC_REPORT)
wxFLAGS_MEMBER(wxLC_ICON)
wxFLAGS_MEMBER(wxLC_SMALL_ICON)
wxFLAGS_MEMBER(wxLC_ALIGN_TOP)
wxFLAGS_MEMBER(wxLC_ALIGN_LEFT)
wxFLAGS_MEMBER(wxLC_AUTOARRANGE)
wxFLAGS_MEMBER(wxLC_USER_TEXT)
wxFLAGS_MEMBER(wxLC_EDIT_LABELS)
wxFLAGS_MEMBER(wxLC_NO_HEADER)
wxFLAGS_MEMBER(wxLC_SINGLE_SEL)
wxFLAGS_MEMBER(wxLC_SORT_ASCENDING)
wxFLAGS_MEMBER(wxLC_SORT_DESCENDING)
wxFLAGS_MEMBER(wxLC_VIRTUAL)
wxEND_FLAGS( wxListCtrlStyle )

#if ((!defined(__WXMSW__) && !(defined(__WXMAC__) && wxOSX_USE_CARBON)) || defined(__WXUNIVERSAL__))
wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxListCtrl, wxGenericListCtrl, "wx/listctrl.h")
#else
wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxListCtrl, wxControl, "wx/listctrl.h")
#endif

wxBEGIN_PROPERTIES_TABLE(wxListCtrl)
wxEVENT_PROPERTY( TextUpdated, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEvent )

wxPROPERTY_FLAGS( WindowStyle, wxListCtrlStyle, long, SetWindowStyleFlag, \
                 GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                 wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxListCtrl)

wxCONSTRUCTOR_5( wxListCtrl, wxWindow*, Parent, wxWindowID, Id, \
                wxPoint, Position, wxSize, Size, long, WindowStyle )

/*
 TODO : Expose more information of a list's layout etc. via appropriate objects 
 (see NotebookPageInfo)
 */

IMPLEMENT_DYNAMIC_CLASS(wxListView, wxListCtrl)
IMPLEMENT_DYNAMIC_CLASS(wxListItem, wxObject)
IMPLEMENT_DYNAMIC_CLASS(wxListEvent, wxNotifyEvent)

// ----------------------------------------------------------------------------
// wxListCtrlBase implementation
// ----------------------------------------------------------------------------

long
wxListCtrlBase::AppendColumn(const wxString& heading,
                             int format,
                             int width)
{
    return InsertColumn(GetColumnCount(), heading, format, width);
}

long
wxListCtrlBase::InsertColumn(long col,
                             const wxString& heading,
                             int format,
                             int width)
{
    wxListItem item;
    item.m_mask = wxLIST_MASK_TEXT | wxLIST_MASK_FORMAT;
    item.m_text = heading;
    if ( width >= 0
            || width == wxLIST_AUTOSIZE
                || width == wxLIST_AUTOSIZE_USEHEADER )
    {
        item.m_mask |= wxLIST_MASK_WIDTH;
        item.m_width = width;
    }
    item.m_format = format;

    return InsertColumn(col, item);
}

long wxListCtrlBase::InsertColumn(long col, const wxListItem& info)
{
    long rc = DoInsertColumn(col, info);
    if ( rc != -1 )
    {
        // As our best size calculation depends on the column headers,
        // invalidate the previously cached best size when a column is added.
        InvalidateBestSize();
    }

    return rc;
}

wxSize wxListCtrlBase::DoGetBestClientSize() const
{
    // There is no obvious way to determine the best size in icon and list
    // modes so just don't do it for now.
    if ( !InReportView() )
        return wxControl::DoGetBestClientSize();

    // In report mode, we use only the column headers, not items, to determine
    // the best width. The reason for this is that it's easier (we can't just
    // iterate over all items, especially not in a virtual control, so we'd
    // have to do something relatively complicated such as checking the size of
    // some items in the beginning and the end only) and also because the
    // columns are usually static while the list contents is dynamic so it
    // usually doesn't make much sense to adjust the control size to it anyhow.
    // And finally, scrollbars can always be used with the items while the
    // headers are just truncated if there is not enough place for them.
    const int columns = GetColumnCount();
    if ( HasFlag(wxLC_NO_HEADER) || !columns )
        return wxControl::DoGetBestClientSize();

    wxClientDC dc(const_cast<wxListCtrlBase*>(this));

    // Total width of all headers determines the best control width.
    int totalWidth = 0;
    for ( int col = 0; col < columns; col++ )
    {
        totalWidth += GetColumnWidth(col);
    }

    // Use some arbitrary height, there is no good way to determine it.
    return wxSize(totalWidth, 10*dc.GetCharHeight());
}

#endif // wxUSE_LISTCTRL
