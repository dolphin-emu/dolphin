/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/dirdlg.cpp
// Purpose:     wxDirDialog
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_DIRDLG && !defined(__WXUNIVERSAL__)

#include "wx/dirdlg.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/dialog.h"
#endif // WX_PRECOMP

#include "wx/filename.h"
#include "wx/modalhook.h"

#include "wx/osx/private.h"

IMPLEMENT_CLASS(wxDirDialog, wxDialog)

static pascal void NavEventProc(
                                NavEventCallbackMessage inSelector,
                                NavCBRecPtr ioParams,
                                NavCallBackUserData ioUserData );

static NavEventUPP sStandardNavEventFilter = NewNavEventUPP(NavEventProc);

static pascal void NavEventProc(
                                NavEventCallbackMessage inSelector,
                                NavCBRecPtr ioParams,
                                NavCallBackUserData ioUserData )
{
    wxDirDialog * data = ( wxDirDialog *) ioUserData ;
    if ( inSelector == kNavCBStart )
    {
        if (data && !data->GetPath().empty() )
        {
            // Set default location for the modern Navigation APIs
            // Apple Technical Q&A 1151
            FSRef theFile;
            wxMacPathToFSRef(data->GetPath(), &theFile);
            AEDesc theLocation = { typeNull, NULL };
            if (noErr == ::AECreateDesc(typeFSRef, &theFile, sizeof(FSRef), &theLocation))
                 ::NavCustomControl(ioParams->context, kNavCtlSetLocation, (void *) &theLocation);
        }
    }
}

void wxDirDialog::Init()
{
}

void wxDirDialog::Create(wxWindow *parent,
                         const wxString& message,
                         const wxString& defaultPath,
                         long style,
                         const wxPoint& WXUNUSED(pos),
                         const wxSize& WXUNUSED(size),
                         const wxString& WXUNUSED(name))
{
    wxASSERT_MSG( NavServicesAvailable() , wxT("Navigation Services are not running") ) ;
    m_parent = parent;

    SetMessage( message );
    SetWindowStyle(style);
    SetPath(defaultPath);
}

int wxDirDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    NavDialogRef dialog = NULL;
    NavDialogCreationOptions options;
    NavReplyRecord reply ;
    bool disposeReply = false ;
    OSStatus err = noErr;

    err = NavGetDefaultDialogCreationOptions(&options);
    options.optionFlags &= ~kNavAllowMultipleFiles;
    if (err == noErr)
    {
        wxCFStringRef message(m_message, GetFont().GetEncoding());
        options.message = message;
        err = NavCreateChooseFolderDialog(&options, sStandardNavEventFilter , NULL,  this , &dialog);
        if (err == noErr)
        {
            wxDialog::OSXBeginModalDialog();
            err = NavDialogRun(dialog);
            wxDialog::OSXEndModalDialog();
            if ( err == noErr )
            {
                err = NavDialogGetReply(dialog, &reply);
                disposeReply = true ;
            }
        }
    }

    if ( err == noErr )
    {
        if ( reply.validRecord )
        {
            FSRef folderInfo;
            AEDesc specDesc ;

            OSErr err = ::AECoerceDesc( &reply.selection , typeFSRef, &specDesc);
            if ( err != noErr )
            {
                m_path = wxEmptyString ;
            }
            else
            {
                folderInfo = **(FSRef**) specDesc.dataHandle;
                m_path = wxMacFSRefToPath( &folderInfo ) ;
                if (specDesc.dataHandle != nil)
                {
                    ::AEDisposeDesc(&specDesc);
                }
            }
        }
        else
        {
            err = paramErr ; // could be any error, only used for giving back wxID_CANCEL
        }
    }

    if ( disposeReply )
        ::NavDisposeReply(&reply);

    // apparently cancelling shouldn't change m_path
    if ( err != noErr && err != userCanceledErr )
        m_path = wxEmptyString ;

    if ( dialog )
        ::NavDialogDispose(dialog);

    return (err == noErr) ? wxID_OK : wxID_CANCEL ;
}

#endif
