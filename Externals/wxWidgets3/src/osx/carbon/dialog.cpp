/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/dialog.cpp
// Purpose:     wxDialog class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: dialog.cpp 65680 2010-09-30 11:44:45Z VZ $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/dialog.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/frame.h"
    #include "wx/settings.h"
#endif // WX_PRECOMP

#include "wx/osx/private.h"
#include "wx/evtloop.h"

void wxDialog::EndWindowModal()
{
    // Nothing to do for now.
}

void wxDialog::DoShowWindowModal()
{
    // If someone wants to add support for this to wxOSX Carbon, here would
    // be the place to start: http://trac.wxwidgets.org/ticket/9459
    // Unfortunately, supporting sheets in Carbon isn't as straightforward
    // as with Cocoa, so it will probably take some tweaking.

    m_modality = wxDIALOG_MODALITY_APP_MODAL;
    ShowModal();
    SendWindowModalDialogEvent ( wxEVT_WINDOW_MODAL_DIALOG_CLOSED  );
}
