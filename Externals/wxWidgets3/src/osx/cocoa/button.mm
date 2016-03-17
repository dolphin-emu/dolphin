/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/button.mm
// Purpose:     wxButton
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/object.h"
#endif

#include "wx/button.h"
#include "wx/toplevel.h"
#include "wx/tglbtn.h"

#include "wx/osx/private.h"

#if wxUSE_MARKUP
    #include "wx/osx/cocoa/private/markuptoattr.h"
#endif // wxUSE_MARKUP


@implementation wxNSButton

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (id) initWithFrame:(NSRect) frame
{
    self = [super initWithFrame:frame];

    // NSButton uses "Button" as its title by default which is inconvenient as
    // wxWidgets expects the control to not have any default label, so reset it
    // here to resolve this mismatch.
    [self setTitle:@""];

    return self;
}

- (int) intValue
{
    switch ( [self state] )
    {
        case NSOnState:
            return 1;
        case NSMixedState:
            return 2;
        default:
            return 0;
    }
}

- (void) setIntValue: (int) v
{
    switch( v )
    {
        case 2:
            [self setState:NSMixedState];
            break;
        case 1:
            [self setState:NSOnState];
            break;
        default :
            [self setState:NSOffState];
            break;
    }
}

- (void) setTrackingTag: (NSTrackingRectTag)tag
{
    rectTag = tag;
}

- (NSTrackingRectTag) trackingTag
{
    return rectTag;
}

@end

@interface NSView(PossibleSizeMethods)
- (NSControlSize)controlSize;
@end

wxButtonCocoaImpl::wxButtonCocoaImpl(wxWindowMac *wxpeer, wxNSButton *v)
: wxWidgetCocoaImpl(wxpeer, v)
{
    SetNeedsFrame(false);
}

void wxButtonCocoaImpl::SetBitmap(const wxBitmap& bitmap)
{
    // switch bezel style for plain pushbuttons
    if ( bitmap.IsOk() )
    {
        if ([GetNSButton() bezelStyle] == NSRoundedBezelStyle)
            [GetNSButton() setBezelStyle:NSRegularSquareBezelStyle];
    }
    else
    {
        [GetNSButton() setBezelStyle:NSRoundedBezelStyle];
    }
    
    wxWidgetCocoaImpl::SetBitmap(bitmap);
}

#if wxUSE_MARKUP
void wxButtonCocoaImpl::SetLabelMarkup(const wxString& markup)
{
    wxMarkupToAttrString toAttr(GetWXPeer(), markup);
    NSMutableAttributedString *attrString = toAttr.GetNSAttributedString();
    
    // Button text is always centered.
    NSMutableParagraphStyle *
    paragraphStyle = [[NSMutableParagraphStyle alloc] init];
    [paragraphStyle setAlignment: NSCenterTextAlignment];
    [attrString addAttribute:NSParagraphStyleAttributeName
                       value:paragraphStyle
                       range:NSMakeRange(0, [attrString length])];
    [paragraphStyle release];
    
    [GetNSButton() setAttributedTitle:attrString];
}
#endif // wxUSE_MARKUP

void wxButtonCocoaImpl::SetPressedBitmap( const wxBitmap& bitmap )
{
    NSButton* button = GetNSButton();
    [button setAlternateImage: bitmap.GetNSImage()];
#if wxUSE_TOGGLEBTN
    if ( GetWXPeer()->IsKindOf(wxCLASSINFO(wxToggleButton)) )
    {
        [button setButtonType:NSToggleButton];
    }
    else
#endif
    {
        [button setButtonType:NSMomentaryChangeButton];
    }
}

void wxButtonCocoaImpl::GetLayoutInset(int &left , int &top , int &right, int &bottom) const
{
    left = top = right = bottom = 0;
    NSControlSize size = NSRegularControlSize;
    if ( [m_osxView respondsToSelector:@selector(controlSize)] )
        size = [m_osxView controlSize];
    else if ([m_osxView respondsToSelector:@selector(cell)])
    {
        id cell = [(id)m_osxView cell];
        if ([cell respondsToSelector:@selector(controlSize)])
            size = [cell controlSize];
    }
    
    if ( [GetNSButton() bezelStyle] == NSRoundedBezelStyle )
    {
        switch( size )
        {
            case NSRegularControlSize:
                left = right = 6;
                top = 4;
                bottom = 7;
                break;
            case NSSmallControlSize:
                left = right = 5;
                top = 4;
                bottom = 6;
                break;
            case NSMiniControlSize:
                left = right = 1;
                top = 0;
                bottom = 1;
                break;
        }
    }
    else if ( [GetNSButton() bezelStyle] == NSSmallSquareBezelStyle )
    {
        left = right = 0;
        top = bottom = 1;
    }
}

void wxButtonCocoaImpl::SetAcceleratorFromLabel(const wxString& label)
{
    const int accelPos = wxControl::FindAccelIndex(label);
    if ( accelPos != wxNOT_FOUND )
    {
        wxString accelstring(label[accelPos + 1]); // Skip '&' itself
        accelstring.MakeLower();
        wxCFStringRef cfText(accelstring);
        [GetNSButton() setKeyEquivalent:cfText.AsNSString()];
        [GetNSButton() setKeyEquivalentModifierMask:NSCommandKeyMask];
    }
    else
    {
        [GetNSButton() setKeyEquivalent:@""];
    }
}

NSButton *wxButtonCocoaImpl::GetNSButton() const
{
    wxASSERT( [m_osxView isKindOfClass:[NSButton class]] );
    
    return static_cast<NSButton *>(m_osxView);
}

// Set bezel style depending on the wxBORDER_XXX flags specified by the style
// and also accounting for the label (bezels are different for multiline
// buttons and normal ones) and the ID (special bezel is used for help button).
//
// This is extern because it's also used in src/osx/cocoa/tglbtn.mm.
extern "C"
void
SetBezelStyleFromBorderFlags(NSButton *v,
                             long style,
                             wxWindowID winid,
                             const wxString& label = wxString(),
                             const wxBitmap& bitmap = wxBitmap())
{
    // We can't display a custom label inside a button with help bezel style so
    // we only use it if we are using the default label. wxButton itself checks
    // if the label is just "Help" in which case it discards it and passes us
    // an empty string.
    if ( winid == wxID_HELP && label.empty() )
    {
        [v setBezelStyle:NSHelpButtonBezelStyle];
    }
    else
    {
        // We can't use rounded bezel styles neither for multiline buttons nor
        // for buttons containing (big) icons as they are only meant to be used
        // at certain sizes, so the style used depends on whether the label is
        // single or multi line.
        const bool
            isSimpleText = (label.find_first_of("\n\r") == wxString::npos)
                                && (!bitmap.IsOk() || bitmap.GetHeight() < 20);

        NSBezelStyle bezel;
        switch ( style & wxBORDER_MASK )
        {
            case wxBORDER_NONE:
                bezel = NSShadowlessSquareBezelStyle;
                [v setBordered:NO];
                break;

            case wxBORDER_SIMPLE:
                bezel = NSSmallSquareBezelStyle;
                break;

            case wxBORDER_SUNKEN:
                bezel = isSimpleText ? NSTexturedRoundedBezelStyle
                                     : NSSmallSquareBezelStyle;
                break;

            default:
                wxFAIL_MSG( "Unknown border style" );
                wxFALLTHROUGH;

            case 0:
            case wxBORDER_STATIC:
            case wxBORDER_RAISED:
            case wxBORDER_THEME:
                bezel = isSimpleText ? NSRoundedBezelStyle
                                     : NSSmallSquareBezelStyle;
                break;
        }

        [v setBezelStyle:bezel];
    }
}

// Set the keyboard accelerator key from the label (e.g. "Click &Me")
void wxButton::OSXUpdateAfterLabelChange(const wxString& label)
{
    wxButtonCocoaImpl *impl = static_cast<wxButtonCocoaImpl*>(GetPeer());

    // Update the bezel style as may be necessary if our new label is multi
    // line while the old one wasn't (or vice versa).
    SetBezelStyleFromBorderFlags(impl->GetNSButton(),
                                 GetWindowStyle(),
                                 GetId(),
                                 label);


    // Skip setting the accelerator for the default buttons as this would
    // overwrite the default "Enter" which should be preserved.
    wxTopLevelWindow * const
        tlw = wxDynamicCast(wxGetTopLevelParent(this), wxTopLevelWindow);
    if ( tlw )
    {
        if ( tlw->GetDefaultItem() == this )
            return;
    }

    impl->SetAcceleratorFromLabel(label);
}


wxWidgetImplType* wxWidgetImpl::CreateButton( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID winid,
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSButton* v = [[wxNSButton alloc] initWithFrame:r];

    SetBezelStyleFromBorderFlags(v, style, winid, label);

    [v setButtonType:NSMomentaryPushInButton];
    wxButtonCocoaImpl* const impl = new wxButtonCocoaImpl( wxpeer, v );
    impl->SetAcceleratorFromLabel(label);
    return impl;
}

void wxWidgetCocoaImpl::SetDefaultButton( bool isDefault )
{
    if ( [m_osxView isKindOfClass:[NSButton class]] )
    {
        if ( isDefault )
        {
            [(NSButton*)m_osxView setKeyEquivalent: @"\r" ];
            [(NSButton*)m_osxView setKeyEquivalentModifierMask: 0];
        }
        else
            [(NSButton*)m_osxView setKeyEquivalent: @"" ];
    }
}

void wxWidgetCocoaImpl::PerformClick()
{
    if ([m_osxView isKindOfClass:[NSControl class]])
        [(NSControl*)m_osxView performClick:nil];
}

#if wxUSE_BMPBUTTON

wxWidgetImplType* wxWidgetImpl::CreateBitmapButton( wxWindowMac* wxpeer,
                                                   wxWindowMac* WXUNUSED(parent),
                                                   wxWindowID winid,
                                                   const wxBitmap& bitmap,
                                                   const wxPoint& pos,
                                                   const wxSize& size,
                                                   long style,
                                                   long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSButton* v = [[wxNSButton alloc] initWithFrame:r];

    SetBezelStyleFromBorderFlags(v, style, winid, wxString(), bitmap);

    if (bitmap.IsOk())
        [v setImage:bitmap.GetNSImage() ];

    [v setButtonType:NSMomentaryPushInButton];
    wxWidgetCocoaImpl* c = new wxButtonCocoaImpl( wxpeer, v );
    return c;
}

#endif // wxUSE_BMPBUTTON
