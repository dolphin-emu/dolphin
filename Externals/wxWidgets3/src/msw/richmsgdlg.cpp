/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/richmsgdlg.cpp
// Purpose:     wxRichMessageDialog
// Author:      Rickard Westerlund
// Created:     2010-07-04
// RCS-ID:      $Id: richmsgdlg.cpp 66613 2011-01-07 04:50:53Z PC $
// Copyright:   (c) 2010 wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#if wxUSE_RICHMSGDLG

#include "wx/richmsgdlg.h"

#ifndef WX_PRECOMP
    #include "wx/msw/private.h"
#endif

// This will define wxHAS_MSW_TASKDIALOG if we have support for it in the
// headers we use.
#include "wx/msw/private/msgdlg.h"

// ----------------------------------------------------------------------------
// wxRichMessageDialog
// ----------------------------------------------------------------------------

int wxRichMessageDialog::ShowModal()
{
#ifdef wxHAS_MSW_TASKDIALOG
    using namespace wxMSWMessageDialog;

    if ( HasNativeTaskDialog() )
    {
        // create a task dialog
        WinStruct<TASKDIALOGCONFIG> tdc;
        wxMSWTaskDialogConfig wxTdc(*this);

        wxTdc.MSWCommonTaskDialogInit( tdc );

        // add a checkbox
        if ( !m_checkBoxText.empty() )
        {
            tdc.pszVerificationText = m_checkBoxText.wx_str();
            if ( m_checkBoxValue )
                tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
        }

        // add collapsible footer
        if ( !m_detailedText.empty() )
            tdc.pszExpandedInformation = m_detailedText.wx_str();

        TaskDialogIndirect_t taskDialogIndirect = GetTaskDialogIndirectFunc();
        if ( !taskDialogIndirect )
            return wxID_CANCEL;

        // create the task dialog, process the answer and return it.
        BOOL checkBoxChecked;
        int msAns;
        HRESULT hr = taskDialogIndirect( &tdc, &msAns, NULL, &checkBoxChecked );
        if ( FAILED(hr) )
        {
            wxLogApiError( "TaskDialogIndirect", hr );
            return wxID_CANCEL;
        }
        m_checkBoxValue = checkBoxChecked != FALSE;

        return MSWTranslateReturnCode( msAns );
    }
#endif // wxHAS_MSW_TASKDIALOG

    // use the generic version when task dialog is't available at either
    // compile or run-time.
    return wxGenericRichMessageDialog::ShowModal();
}

#endif // wxUSE_RICHMSGDLG
