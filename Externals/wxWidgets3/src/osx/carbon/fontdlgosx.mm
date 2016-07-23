/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/fontdlgosx.mm
// Purpose:     wxFontDialog class.
// Author:      Ryan Norton
// Modified by:
// Created:     2004-10-03
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

#include "wx/fontdlg.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/cmndata.h"
#endif

#include "wx/fontutil.h"
#include "wx/modalhook.h"

// ============================================================================
// implementation
// ============================================================================


#if wxOSX_USE_EXPERIMENTAL_FONTDIALOG

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#include "wx/osx/private.h"


@interface wxFontPanelDelegate : NSObject<NSWindowDelegate>
{
    @public
    bool m_isUnderline;
    bool m_isStrikethrough;
}

// Delegate methods
- (id)init;
- (void)changeAttributes:(id)sender;
- (void)changeFont:(id)sender;
@end // interface wxNSFontPanelDelegate : NSObject



@implementation wxFontPanelDelegate : NSObject

- (id)init
{
    [super init];
    m_isUnderline = false;
    m_isStrikethrough = false;
    return self;
}

- (void)changeAttributes:(id)sender
{
    NSDictionary *dummyAttribs = [NSDictionary dictionaryWithObjectsAndKeys:
                                   [NSNumber numberWithInt:m_isUnderline?NSUnderlineStyleSingle:NSUnderlineStyleNone], NSUnderlineStyleAttributeName,
                                   [NSNumber numberWithInt:m_isStrikethrough?NSUnderlineStyleSingle:NSUnderlineStyleNone], NSStrikethroughStyleAttributeName,
                                   nil];
    NSDictionary *attribs = [sender convertAttributes:dummyAttribs];

    m_isUnderline = m_isStrikethrough = false;
    for (id key in attribs) {
        NSNumber *number = static_cast<NSNumber *>([attribs objectForKey:key]);
        int i = [number intValue];
        if ([key isEqual:NSUnderlineStyleAttributeName]) {
            m_isUnderline = [number intValue] != NSUnderlineStyleNone;
        } else if ([key isEqual:NSStrikethroughStyleAttributeName]) {
            m_isStrikethrough = [number intValue] != NSUnderlineStyleNone;
        }
    }

    NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:
                                [NSNumber numberWithInt:m_isUnderline?NSUnderlineStyleSingle:NSUnderlineStyleNone], NSUnderlineStyleAttributeName,
                                [NSNumber numberWithInt:m_isStrikethrough?NSUnderlineStyleSingle:NSUnderlineStyleNone], NSStrikethroughStyleAttributeName,
                                nil];
    [[NSFontManager sharedFontManager] setSelectedAttributes:attributes isMultiple:false];
}
- (void)changeFont:(id)sender
{
    NSFont *dummyFont = [NSFont userFontOfSize:12.0];
    [[NSFontPanel sharedFontPanel] setPanelFont:[sender convertFont:dummyFont] isMultiple:NO];
    [[NSFontManager sharedFontManager] setSelectedFont:[sender convertFont:dummyFont] isMultiple:false];
}
@end

@interface wxMacFontPanelAccView : NSView
{
    BOOL m_okPressed ;
    BOOL m_shouldClose ;
    NSButton* m_cancelButton ;
    NSButton* m_okButton ;
}

- (IBAction)cancelPressed:(id)sender;
- (IBAction)okPressed:(id)sender;
- (void)resetFlags;
- (BOOL)closedWithOk;
- (BOOL)shouldCloseCarbon;
- (NSButton*)okButton;
@end

@implementation wxMacFontPanelAccView : NSView
- (id)initWithFrame:(NSRect)rectBox
{
    [super initWithFrame:rectBox];

    wxCFStringRef cfOkString( wxT("OK"), wxLocale::GetSystemEncoding() );
    wxCFStringRef cfCancelString( wxT("Cancel"), wxLocale::GetSystemEncoding() );

    NSRect rectCancel = NSMakeRect( (CGFloat) 10.0 , (CGFloat)10.0 , (CGFloat)82  , (CGFloat)24 );
    NSRect rectOK = NSMakeRect( (CGFloat)100.0 , (CGFloat)10.0 , (CGFloat)82  , (CGFloat)24 );

    NSButton* cancelButton = [[NSButton alloc] initWithFrame:rectCancel];
    [cancelButton setTitle:(NSString*)wxCFRetain((CFStringRef)cfCancelString)];
    [cancelButton setBezelStyle:NSRoundedBezelStyle];
    [cancelButton setButtonType:NSMomentaryPushInButton];
    [cancelButton setAction:@selector(cancelPressed:)];
    [cancelButton setTarget:self];
    m_cancelButton = cancelButton ;

    NSButton* okButton = [[NSButton alloc] initWithFrame:rectOK];
    [okButton setTitle:(NSString*)wxCFRetain((CFStringRef)cfOkString)];
    [okButton setBezelStyle:NSRoundedBezelStyle];
    [okButton setButtonType:NSMomentaryPushInButton];
    [okButton setAction:@selector(okPressed:)];
    [okButton setTarget:self];
    // doesn't help either, the button is not highlighted after a color dialog has been used
    // [okButton setKeyEquivalent:@"\r"];
    m_okButton = okButton ;


    [self addSubview:cancelButton];
    [self addSubview:okButton];

    [self resetFlags];
    return self;
}

- (void)resetFlags
{
    m_okPressed = NO ;
    m_shouldClose = NO ;
}

- (IBAction)cancelPressed:(id)sender
{
    wxUnusedVar(sender);
    m_shouldClose = YES ;
    [NSApp stopModal];
}

- (IBAction)okPressed:(id)sender
{
    wxUnusedVar(sender);
    m_okPressed = YES ;
    m_shouldClose = YES ;
    [NSApp stopModal];
}

-(BOOL)closedWithOk
{
    return m_okPressed ;
}

-(BOOL)shouldCloseCarbon
{
    return m_shouldClose ;
}

-(NSButton*)okButton
{
    return m_okButton ;
}
@end


extern "C" int RunMixedFontDialog(wxFontDialog* dialog) ;

int RunMixedFontDialog(wxFontDialog* dialog)
{
#if wxOSX_USE_COCOA
    wxFontData& fontdata= ((wxFontDialog*)dialog)->GetFontData() ;
#else
    wxUnusedVar(dialog);
#endif
    int retval = wxID_CANCEL ;

    wxMacAutoreleasePool pool;

    // setting up the ok/cancel buttons
    NSFontPanel* fontPanel = [NSFontPanel sharedFontPanel] ;

    wxFontPanelDelegate* theFPDelegate = [[wxFontPanelDelegate alloc] init];
    [fontPanel setDelegate:theFPDelegate];


    [fontPanel setFloatingPanel:NO] ;
    [[fontPanel standardWindowButton:NSWindowCloseButton] setEnabled:NO] ;

    wxMacFontPanelAccView* accessoryView = nil;
    if ( [fontPanel accessoryView] == nil || [[fontPanel accessoryView] class] != [wxMacFontPanelAccView class] )
    {
        NSRect rectBox = NSMakeRect( 0 , 0 , 192 , 40 );
        accessoryView = [[wxMacFontPanelAccView alloc] initWithFrame:rectBox];
        [fontPanel setAccessoryView:accessoryView];
        [accessoryView release];

        [fontPanel setDefaultButtonCell:[[accessoryView okButton] cell]] ;
    }
    else
    {
        accessoryView = (wxMacFontPanelAccView*)[fontPanel accessoryView];
    }

    [accessoryView resetFlags];
#if wxOSX_USE_COCOA
    wxFont font = *wxNORMAL_FONT ;
    if ( fontdata.m_initialFont.IsOk() )
    {
        font = fontdata.m_initialFont ;
    }
    theFPDelegate->m_isStrikethrough = font.GetStrikethrough();
    theFPDelegate->m_isUnderline = font.GetUnderlined();

    [[NSFontPanel sharedFontPanel] setPanelFont: font.OSXGetNSFont() isMultiple:NO];
    [[NSFontManager sharedFontManager] setSelectedFont:font.OSXGetNSFont() isMultiple:false];

    NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:
                                [NSNumber numberWithInt:font.GetUnderlined()
                                    ? NSUnderlineStyleSingle
                                    : NSUnderlineStyleNone],
                                NSUnderlineStyleAttributeName,
                                [NSNumber numberWithInt:font.GetStrikethrough()
                                    ? NSUnderlineStyleSingle
                                    : NSUnderlineStyleNone],
                                NSStrikethroughStyleAttributeName,
                                nil];

    [[NSFontManager sharedFontManager] setSelectedAttributes:attributes isMultiple:false];

    if(fontdata.m_fontColour.IsOk())
        [[NSColorPanel sharedColorPanel] setColor: fontdata.m_fontColour.OSXGetNSColor()];
    else
        [[NSColorPanel sharedColorPanel] setColor:[NSColor blackColor]];
#endif
    
    [NSApp runModalForWindow:fontPanel];
    
    // if we don't reenable it, FPShowHideFontPanel does not work
    [[fontPanel standardWindowButton:NSWindowCloseButton] setEnabled:YES] ;
    // we must pick the selection before closing, otherwise a native textcontrol interferes
    NSFont* theFont = [fontPanel panelConvertFont:[NSFont userFontOfSize:0]];
    [fontPanel close];

    if ( [accessoryView closedWithOk])
    {
#if wxOSX_USE_COCOA
        fontdata.m_chosenFont = wxFont( theFont );
        fontdata.m_chosenFont.SetUnderlined(theFPDelegate->m_isUnderline);
        fontdata.m_chosenFont.SetStrikethrough(theFPDelegate->m_isStrikethrough);

        //Get the shared color panel along with the chosen color and set the chosen color
        fontdata.m_fontColour = wxColour([[NSColorPanel sharedColorPanel] color]);
#endif
        retval = wxID_OK ;
    }
    [fontPanel setAccessoryView:nil];
    [theFPDelegate release];
    return retval ;
}

#else

#if USE_NATIVE_FONT_DIALOG_FOR_MACOSX

wxIMPLEMENT_DYNAMIC_CLASS(wxFontDialog, wxDialog);

// Cocoa headers

#import <AppKit/NSFont.h>
#import <AppKit/NSFontManager.h>
#import <AppKit/NSFontPanel.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSColorPanel.h>

// ---------------------------------------------------------------------------
// wxWCDelegate - Window Closed delegate
// ---------------------------------------------------------------------------

@interface wxWCDelegate : NSObject
{
    bool m_bIsClosed;
}

// Delegate methods
- (id)init;
- (BOOL)windowShouldClose:(id)sender;
- (BOOL)isClosed;
@end // interface wxNSFontPanelDelegate : NSObject

@implementation wxWCDelegate : NSObject

- (id)init
{
    [super init];
    m_bIsClosed = false;

    return self;
}

- (BOOL)windowShouldClose:(id)sender
{
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

// ---------------------------------------------------------------------------
// wxWCODelegate - Window Closed or Open delegate
// ---------------------------------------------------------------------------

@interface wxWCODelegate : NSObject
{
    bool m_bIsClosed;
    bool m_bIsOpen;
}

// Delegate methods
- (id)init;
- (BOOL)windowShouldClose:(id)sender;
- (void)windowDidUpdate:(NSNotification *)aNotification;
- (BOOL)isClosed;
- (BOOL)isOpen;
@end // interface wxNSFontPanelDelegate : NSObject

@implementation wxWCODelegate : NSObject

- (id)init
{
    [super init];
    m_bIsClosed = false;
    m_bIsOpen = false;

    return self;
}

- (BOOL)windowShouldClose:(id)sender
{
    m_bIsClosed = true;
    m_bIsOpen = false;

    [NSApp abortModal];
    [NSApp stopModal];
    return YES;
}

- (void)windowDidUpdate:(NSNotification *)aNotification
{
    if (m_bIsOpen == NO)
    {
        m_bIsClosed = false;
        m_bIsOpen = true;

        [NSApp abortModal];
        [NSApp stopModal];
    }
}

- (BOOL)isClosed
{
    return m_bIsClosed;
}

- (BOOL)isOpen
{
    return m_bIsOpen;
}

@end // wxNSFontPanelDelegate

// ---------------------------------------------------------------------------
// wxFontDialog
// ---------------------------------------------------------------------------

wxFontDialog::wxFontDialog()
{
}

wxFontDialog::wxFontDialog(wxWindow *parent)
{
    Create(parent);
}

wxFontDialog::wxFontDialog(wxWindow *parent, const wxFontData&  data)
{
    Create(parent, data);
}

wxFontDialog::~wxFontDialog()
{
}

bool wxFontDialog::Create(wxWindow *parent)
{
    return Create(parent);
}

bool wxFontDialog::Create(wxWindow *parent, const wxFontData& data)
{
    m_fontData = data;

    return Create(parent);
}

bool wxFontDialog::Create(wxWindow *parent)
{
    //autorelease pool - req'd for carbon
    NSAutoreleasePool *thePool;
    thePool = [[NSAutoreleasePool alloc] init];

    //Get the initial wx font
    wxFont& thewxfont = m_fontData.m_initialFont;

    //if the font is valid set the default (selected) font of the
    //NSFontDialog to that font
    if (thewxfont.IsOk())
    {
        NSFontTraitMask theMask = 0;

        if(thewxfont.GetStyle() == wxFONTSTYLE_ITALIC)
            theMask |= NSItalicFontMask;

        if(thewxfont.IsFixedWidth())
            theMask |= NSFixedPitchFontMask;

        NSFont* theDefaultFont =
            [[NSFontManager sharedFontManager] fontWithFamily:
                                                    wxNSStringWithWxString(thewxfont.GetFaceName())
                                            traits:theMask
                                            weight:thewxfont.GetWeight() == wxFONTWEIGHT_BOLD ? 9 :
                                                    thewxfont.GetWeight() == wxFONTWEIGHT_LIGHT ? 0 : 5
                                            size: (float)(thewxfont.GetPointSize())
            ];

        wxASSERT_MSG(theDefaultFont, wxT("Invalid default font for wxCocoaFontDialog!"));

        //Apple docs say to call NSFontManager::setSelectedFont
        //However, 10.3 doesn't seem to create the font panel
        //is this is done, so create it ourselves
        [[NSFontPanel sharedFontPanel] setPanelFont:theDefaultFont isMultiple:NO];
        [[NSFontManager sharedFontManager] setSelectedFont:theDefaultFont isMultiple:false];

    }

    if(m_fontData.m_fontColour.IsOk())
        [[NSColorPanel sharedColorPanel] setColor: fontdata.m_fontColour.OSXGetNSColor()];
    else
        [[NSColorPanel sharedColorPanel] setColor:[NSColor blackColor]];

    //We're done - free up the pool
    [thePool release];

    return true;
}

int wxFontDialog::ShowModal()
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
    NSFontPanel* theFontPanel = [NSFontPanel sharedFontPanel];
    NSColorPanel* theColorPanel = [NSColorPanel sharedColorPanel];

    //Create and assign the delegates (cocoa event handlers) so
    //we can tell if a window has closed/open or not
    wxWCDelegate* theFPDelegate = [[wxWCDelegate alloc] init];
    [theFontPanel setDelegate:theFPDelegate];

    wxWCODelegate* theCPDelegate = [[wxWCODelegate alloc] init];
    [theColorPanel setDelegate:theCPDelegate];

    //
    //  Begin the modal loop for the font and color panels
    //
    //  The idea is that we first make the font panel modal,
    //  but if the color panel is opened, unless we stop the
    //  modal loop the color panel opens behind the font panel
    //  with no input acceptable to it - which makes it useless.
    //
    //  So we set up delegates for both the color and font panels,
    //  and the if the font panel opens the color panel, we
    //  stop the modal loop, and start a separate modal loop for
    //  the color panel until the color panel closes, switching
    //  back to the font panel modal loop once it does close.
    //
    wxDialog::OSXBeginModalDialog();
    do
    {
        //
        //  Start the font panel modal loop
        //
        NSModalSession session = [NSApp beginModalSessionForWindow:theFontPanel];
        for (;;)
        {
            [NSApp runModalSession:session];

            //If the font panel is closed or the font panel
            //opened the color panel, break
            if ([theFPDelegate isClosed] || [theCPDelegate isOpen])
                break;
        }
        [NSApp endModalSession:session];

        //is the color panel open?
        if ([theCPDelegate isOpen])
        {
            //
            //  Start the color panel modal loop
            //
            NSModalSession session = [NSApp beginModalSessionForWindow:theColorPanel];
            for (;;)
            {
                [NSApp runModalSession:session];

                //If the color panel is closed, return the font panel modal loop
                if ([theCPDelegate isClosed])
                    break;
            }
            [NSApp endModalSession:session];
        }
        //If the font panel is still alive (I.E. we broke
        //out of its modal loop because the color panel was
        //opened) return the font panel modal loop
    }while([theFPDelegate isClosed] == NO);
    wxDialog::OSXEndModalDialog();
    
    //free up the memory for the delegates - we don't need them anymore
    [theFPDelegate release];
    [theCPDelegate release];

    //Get the font the user selected
    NSFont* theFont = [theFontPanel panelConvertFont:[NSFont userFontOfSize:0]];

    //Get more information about the user's chosen font
    NSFontTraitMask theTraits = [[NSFontManager sharedFontManager] traitsOfFont:theFont];
    int theFontWeight = [[NSFontManager sharedFontManager] weightOfFont:theFont];
    int theFontSize = (int) [theFont pointSize];

    //Set the wx font to the appropriate data
    if(theTraits & NSFixedPitchFontMask)
        m_fontData.m_chosenFont.SetFamily(wxTELETYPE);

    m_fontData.m_chosenFont.SetFaceName(wxStringWithNSString([theFont familyName]));
    m_fontData.m_chosenFont.SetPointSize(theFontSize);
    m_fontData.m_chosenFont.SetStyle(theTraits & NSItalicFontMask ? wxFONTSTYLE_ITALIC : 0);
    m_fontData.m_chosenFont.SetWeight(theFontWeight < 5 ? wxFONTWEIGHT_LIGHT :
                                    theFontWeight >= 9 ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);

    //Get the shared color panel along with the chosen color and set the chosen color
    m_fontData.m_fontColour = wxColour([theColorPanel color]);

    //Friendly debug stuff
#ifdef FONTDLGDEBUG
    wxPrintf(wxT("---Font Panel---\n--NS--\nSize:%f\nWeight:%i\nTraits:%i\n--WX--\nFaceName:%s\nPointSize:%i\nStyle:%i\nWeight:%i\nColor:%i,%i,%i\n---END Font Panel---\n"),

                (float) theFontSize,
                theFontWeight,
                theTraits,

                m_fontData.m_chosenFont.GetFaceName().c_str(),
                m_fontData.m_chosenFont.GetPointSize(),
                m_fontData.m_chosenFont.GetStyle(),
                m_fontData.m_chosenFont.GetWeight(),
                    m_fontData.m_fontColour.Red(),
                    m_fontData.m_fontColour.Green(),
                    m_fontData.m_fontColour.Blue() );
#endif

    //Release the pool, we're done :)
    [thePool release];

    //Return ID_OK - there are no "apply" buttons or the like
    //on either the font or color panel
    return wxID_OK;
}

//old api stuff (REMOVE ME)
bool wxFontDialog::IsShown() const
{
    return false;
}

#endif // 10.2+

#endif
