///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/dseldlg.cpp
// Purpose:     implementation of ::wxDirSelector()
// Author:      Paul Thiessen
// Modified by:
// Created:     20.02.01
// RCS-ID:      $Id: dseldlg.cpp 64940 2010-07-13 13:29:13Z VZ $
// Copyright:   (c) 2001 wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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

#if wxUSE_DIRDLG

#include "wx/dirdlg.h"

#ifndef WX_PRECOMP
#endif //WX_PRECOMP

// ============================================================================
// implementation
// ============================================================================

wxString wxDirSelector(const wxString& message,
                       const wxString& defaultPath,
                       long style,
                       const wxPoint& pos,
                       wxWindow *parent)
{
    wxString path;

    wxDirDialog dirDialog(parent, message, defaultPath, style, pos);
    if ( dirDialog.ShowModal() == wxID_OK )
    {
        path = dirDialog.GetPath();
    }

    return path;
}

#endif // wxUSE_DIRDLG
