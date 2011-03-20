/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/srchcmn.cpp
// Purpose:     common (to all ports) bits of wxSearchCtrl
// Author:      Robin Dunn
// Modified by:
// Created:     19-Dec-2006
// RCS-ID:      $Id: srchcmn.cpp 58039 2009-01-12 14:26:13Z VZ $
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_SEARCHCTRL

#include "wx/srchctrl.h"

#ifndef WX_PRECOMP
#endif

// ----------------------------------------------------------------------------

const char wxSearchCtrlNameStr[] = "searchCtrl";

wxDEFINE_EVENT(wxEVT_COMMAND_SEARCHCTRL_CANCEL_BTN, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_COMMAND_SEARCHCTRL_SEARCH_BTN, wxCommandEvent);


#endif // wxUSE_SEARCHCTRL
