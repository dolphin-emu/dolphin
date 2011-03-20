////////////////////////////////////////////////////////////////////////////////
// Name:        src/common/listctrlcmn.cpp
// Purpose:     Common defines for wxListCtrl and wxListCtrl-based classes.
// Author:      Kevin Ollivier
// Created:     09/15/06
// RCS-ID:      $Id: listctrlcmn.cpp 66555 2011-01-04 08:31:53Z SC $
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

#endif // wxUSE_LISTCTRL
