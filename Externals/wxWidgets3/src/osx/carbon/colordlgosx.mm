/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/colordlgosx.mm
// Purpose:     wxColourDialog class. NOTE: you can use the generic class
//              if you wish, instead of implementing this.
// Author:      Ryan Norton
// Modified by:
// Created:     2004-11-16
// Copyright:   (c) Ryan Norton
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

#include "wx/wxprec.h"

#include "wx/colordlg.h"
#include "wx/fontdlg.h"
#include "wx/modalhook.h"

// ============================================================================
// implementation
// ============================================================================

//Mac OSX 10.2+ only
#if USE_NATIVE_FONT_DIALOG_FOR_MACOSX && wxUSE_COLOURDLG

wxIMPLEMENT_DYNAMIC_CLASS(wxColourDialog, wxDialog);

#include "wx/osx/private.h"

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

// ---------------------------------------------------------------------------
// wxCPWCDelegate - Window Closed delegate
// ---------------------------------------------------------------------------

@interface wxCPWCDelegate : NSObject <NSWindowDelegate>
{
    bool m_bIsClosed;
}

// Delegate methods
- (id)init;
- (BOOL)windowShouldClose:(id)sender;
- (BOOL)isClosed;
@end // interface wxNSFontPanelDelegate : NSObject

@implementation wxCPWCDelegate : NSObject

- (id)init
{
    self = [super init];
    m_bIsClosed = false;

    return self;
}

- (BOOL)windowShouldClose:(id)sender
{
    wxUnusedVar(sender);

    m_bIsClosed = true;

    [NSApp abortModal];
    [NSApp stopModal];
    return YES;
}

- (BOOL)isClosed
{
    return m_bIsClosed;
}

@end // wxNSFontPanelDelegate

/*
 * wxColourDialog
 */

wxColourDialog::wxColourDialog()
{
    m_dialogParent = NULL;
}

wxColourDialog::wxColourDialog(wxWindow *parent, wxColourData *data)
{
    Create(parent, data);
}

bool wxColourDialog::Create(wxWindow *parent, wxColourData *data)
{
    m_dialogParent = parent;

    if (data)
        m_colourData = *data;

    //autorelease pool - req'd for carbon
    NSAutoreleasePool *thePool;
    thePool = [[NSAutoreleasePool alloc] init];

    [[NSColorPanel sharedColorPanel] setShowsAlpha:YES];
    if(m_colourData.GetColour().IsOk())
        [[NSColorPanel sharedColorPanel] setColor:m_colourData.GetColour().OSXGetNSColor()];
    else
        [[NSColorPanel sharedColorPanel] setColor:[NSColor blackColor]];

    //We're done - free up the pool
    [thePool release];

    return true;
}
int wxColourDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    //Start the pool.  Required for carbon interaction
    //(For those curious, the only thing that happens
    //if you don't do this is a bunch of error
    //messages about leaks on the console,
    //with no windows shown or anything).
    NSAutoreleasePool *thePool;
    thePool = [[NSAutoreleasePool alloc] init];

    //Get the shared color and font panel
    NSColorPanel* theColorPanel = [NSColorPanel sharedColorPanel];

    //Create and assign the delegates (cocoa event handlers) so
    //we can tell if a window has closed/open or not
    wxCPWCDelegate* theCPDelegate = [[wxCPWCDelegate alloc] init];
    [theColorPanel setDelegate:theCPDelegate];

            //
            //	Start the color panel modal loop
            //
            wxDialog::OSXBeginModalDialog();
            NSModalSession session = [NSApp beginModalSessionForWindow:theColorPanel];
            for (;;)
            {
                [NSApp runModalSession:session];

                //If the color panel is closed, return the font panel modal loop
                if ([theCPDelegate isClosed])
                    break;
            }
            [NSApp endModalSession:session];
            wxDialog::OSXEndModalDialog();

    //free up the memory for the delegates - we don't need them anymore
    [theColorPanel setDelegate:nil];
    [theCPDelegate release];

    //Get the shared color panel along with the chosen color and set the chosen color
    m_colourData.GetColour() = wxColour([theColorPanel color]);

    //Release the pool, we're done :)
    [thePool release];

    //Return ID_OK - there are no "apply" buttons or the like
    //on either the font or color panel
    return wxID_OK;
}

#endif //use native font dialog

