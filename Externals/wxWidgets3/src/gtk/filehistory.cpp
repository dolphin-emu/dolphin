/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/filehistory.cpp
// Purpose:     GTK+ bits for wxFileHistory class
// Author:      Vaclav Slavik
// Created:     2010-05-06
// RCS-ID:      $Id: filehistory.cpp 64240 2010-05-07 06:45:48Z VS $
// Copyright:   (c) 2010 Vaclav Slavik
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

#include "wx/filehistory.h"

#if wxUSE_FILE_HISTORY

#include "wx/filename.h"

#include <glib.h>
#include <gtk/gtk.h>
#include "wx/gtk/private/string.h"

// ============================================================================
// implementation
// ============================================================================

void wxFileHistory::AddFileToHistory(const wxString& file)
{
    wxFileHistoryBase::AddFileToHistory(file);

#ifdef __WXGTK210__
    const wxString fullPath = wxFileName(file).GetFullPath();
    if ( !gtk_check_version(2,10,0) )
    {
        wxGtkString uri(g_filename_to_uri(fullPath.fn_str(), NULL, NULL));

        if ( uri )
            gtk_recent_manager_add_item(gtk_recent_manager_get_default(), uri);
    }
#endif
}

#endif // wxUSE_FILE_HISTORY
