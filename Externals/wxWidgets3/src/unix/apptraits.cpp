///////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/apptraits.cpp
// Purpose:     implementation of wxGUIAppTraits for Unix systems
// Author:      Vadim Zeitlin
// Created:     2008-03-22
// RCS-ID:      $Id: apptraits.cpp 52742 2008-03-23 18:24:27Z PC $
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/apptrait.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
#endif // WX_PRECOMP

#include "wx/unix/execute.h"

// ============================================================================
// implementation
// ============================================================================

int wxGUIAppTraits::WaitForChild(wxExecuteData& execData)
{
    const int flags = execData.flags;
    if ( !(flags & wxEXEC_SYNC) || (flags & wxEXEC_NOEVENTS) )
    {
        // async or blocking sync cases are already handled by the base class
        // just fine, no need to duplicate its code here
        return wxAppTraits::WaitForChild(execData);
    }

    // here we're dealing with the case of synchronous execution when we want
    // to process the GUI events while waiting for the child termination

    wxEndProcessData endProcData;
    endProcData.pid = execData.pid;
    endProcData.tag = AddProcessCallback
                      (
                         &endProcData,
                         execData.GetEndProcReadFD()
                      );
    endProcData.async = false;


    // prepare to wait for the child termination: show to the user that we're
    // busy and refuse all input unless explicitly told otherwise
    wxBusyCursor bc;
    wxWindowDisabler wd(!(flags & wxEXEC_NODISABLE));

    // endProcData.pid will be set to 0 from wxHandleProcessTermination() when
    // the process terminates
    while ( endProcData.pid != 0 )
    {
        // don't consume 100% of the CPU while we're sitting in this
        // loop
        if ( !CheckForRedirectedIO(execData) )
            wxMilliSleep(1);

        // give the toolkit a chance to call wxHandleProcessTermination() here
        // and also repaint the GUI and handle other accumulated events
        wxYield();
    }

    return endProcData.exitcode;
}


