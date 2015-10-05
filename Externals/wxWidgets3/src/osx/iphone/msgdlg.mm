/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/iphone/msgdlg.mm
// Purpose:     wxMessageDialog
// Author:      Stefan Csomor
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/msgdlg.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/app.h"
#endif

#include "wx/thread.h"
#include "wx/osx/private.h"
#include "wx/modalhook.h"


wxIMPLEMENT_CLASS(wxMessageDialog, wxDialog);


wxMessageDialog::wxMessageDialog(wxWindow *parent,
                                 const wxString& message,
                                 const wxString& caption,
                                 long style,
                                 const wxPoint& WXUNUSED(pos))
               : wxMessageDialogBase(parent, message, caption, style)
{
}

int wxMessageDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    int resultbutton = wxID_CANCEL;

    const long style = GetMessageDialogStyle();

    // work out what to display
    // if the extended text is empty then we use the caption as the title
    // and the message as the text (for backwards compatibility)
    // but if the extended message is not empty then we use the message as the title
    // and the extended message as the text because that makes more sense

    wxString msgtitle,msgtext;
    if(m_extendedMessage.IsEmpty())
    {
        msgtitle = m_caption;
        msgtext  = m_message;
    }
    else
    {
        msgtitle = m_message;
        msgtext  = m_extendedMessage;
    }

    wxCFStringRef cfNoString( GetNoLabel(), GetFont().GetEncoding() );
    wxCFStringRef cfYesString( GetYesLabel(), GetFont().GetEncoding() );
    wxCFStringRef cfOKString( GetOKLabel(), GetFont().GetEncoding() );
    wxCFStringRef cfCancelString( GetCancelLabel(), GetFont().GetEncoding() );

    wxCFStringRef cfTitle( msgtitle, GetFont().GetEncoding() );
    wxCFStringRef cfText( msgtext, GetFont().GetEncoding() );

    UIAlertView* alert = [[UIAlertView alloc] initWithTitle:cfTitle.AsNSString() message:cfText.AsNSString() delegate:nil cancelButtonTitle:nil otherButtonTitles:nil];

    int buttonId[3] = { 0, 0, 0 };
    int buttonCount = 0;

    if (style & wxYES_NO)
    {
        if ( style & wxNO_DEFAULT )
        {
            [alert addButtonWithTitle:cfNoString.AsNSString()];
            buttonId[ buttonCount++ ] = wxID_NO;
            [alert addButtonWithTitle:cfYesString.AsNSString()];
            buttonId[ buttonCount++ ] = wxID_YES;
        }
        else
        {
            [alert addButtonWithTitle:cfYesString.AsNSString()];
            buttonId[ buttonCount++ ] = wxID_YES;
            [alert addButtonWithTitle:cfNoString.AsNSString()];
            buttonId[ buttonCount++ ] = wxID_NO;
        }

        if (style & wxCANCEL)
        {
            [alert addButtonWithTitle:cfCancelString.AsNSString()];
            buttonId[ buttonCount++ ] = wxID_CANCEL;
        }
    }
    // the MSW implementation even shows an OK button if it is not specified, we'll do the same
    else
    {
        [alert addButtonWithTitle:cfOKString.AsNSString()];
        buttonId[ buttonCount++ ] = wxID_OK;
        if (style & wxCANCEL)
        {
            [alert addButtonWithTitle:cfCancelString.AsNSString()];
            buttonId[ buttonCount++ ] = wxID_CANCEL;
        }
    }


    wxNonOwnedWindow* parentWindow = NULL;
    int button = -1;

    if (GetParent())
    {
        parentWindow = dynamic_cast<wxNonOwnedWindow*>(wxGetTopLevelParent(GetParent()));
    }

/*
    if (parentWindow)
    {
        NSWindow* nativeParent = parentWindow->GetWXWindow();
        ModalDialogDelegate* sheetDelegate = [[ModalDialogDelegate alloc] init];
        [alert beginSheetModalForWindow: nativeParent modalDelegate: sheetDelegate
            didEndSelector: @selector(sheetDidEnd:returnCode:contextInfo:)
            contextInfo: nil];
        [sheetDelegate waitForSheetToFinish];
        button = [sheetDelegate code];
        [sheetDelegate release];
    }
    else
*/
    {
        [alert show];
    }
    // [alert release];

    return wxID_CANCEL;
}
