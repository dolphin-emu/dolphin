/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/button.mm
// Purpose:     wxButton
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: button.mm 70402 2012-01-19 15:01:01Z SC $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/object.h"
#endif

#include "wx/button.h"

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

namespace
{

class wxButtonCocoaImpl : public wxWidgetCocoaImpl, public wxButtonImpl
{
public:
    wxButtonCocoaImpl(wxWindowMac *wxpeer, wxNSButton *v)
        : wxWidgetCocoaImpl(wxpeer, v)
    {
        SetNeedsFrame(false);
    }

    virtual void SetBitmap(const wxBitmap& bitmap)
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
    virtual void SetLabelMarkup(const wxString& markup)
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

    void SetPressedBitmap( const wxBitmap& bitmap )
    {
        NSButton* button = GetNSButton();
        [button setAlternateImage: bitmap.GetNSImage()];
        [button setButtonType:NSMomentaryChangeButton];
    }

    void GetLayoutInset(int &left , int &top , int &right, int &bottom) const
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
                    bottom = 8;
                    break;
                case NSSmallControlSize:
                    left = right = 5;
                    top = 4;
                    bottom = 7;
                    break;
                case NSMiniControlSize:
                    left = right = 1;
                    top = 0;
                    bottom = 2;
                    break;
            }
        }
    }
    
    
private:
    NSButton *GetNSButton() const
    {
        wxASSERT( [m_osxView isKindOfClass:[NSButton class]] );

        return static_cast<NSButton *>(m_osxView);
    }
};

} // anonymous namespace

extern "C" void SetBezelStyleFromBorderFlags(NSButton *v, long style);
    
// set bezel style depending on the wxBORDER_XXX flags specified by the style
void SetBezelStyleFromBorderFlags(NSButton *v, long style)
{
    if ( style & wxBORDER_NONE )
    {
        [v setBezelStyle:NSShadowlessSquareBezelStyle];
        [v setBordered:NO];
    }
    else // we do have a border
    {
        // see trac #11128 for a thorough discussion
        if ( (style & wxBORDER_MASK) == wxBORDER_RAISED )
            [v setBezelStyle:NSRegularSquareBezelStyle];
        else if ( (style & wxBORDER_MASK) == wxBORDER_SUNKEN )
            [v setBezelStyle:NSSmallSquareBezelStyle];
        else if ( (style & wxBORDER_MASK) == wxBORDER_SIMPLE )
            [v setBezelStyle:NSShadowlessSquareBezelStyle];
        else
            [v setBezelStyle:NSRegularSquareBezelStyle];
    }
}


wxWidgetImplType* wxWidgetImpl::CreateButton( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID id,
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSButton* v = [[wxNSButton alloc] initWithFrame:r];

    // We can't display a custom label inside a button with help bezel style so
    // we only use it if we are using the default label. wxButton itself checks
    // if the label is just "Help" in which case it discards it and passes us
    // an empty string.
    if ( id == wxID_HELP && label.empty() )
    {
        [v setBezelStyle:NSHelpButtonBezelStyle];
    }
    else
    {
        if ( style & wxBORDER_NONE )
        {
            [v setBezelStyle:NSShadowlessSquareBezelStyle];
            [v setBordered:NO];
        }
        else 
        {
            // the following styles only exist for certain sizes, so avoid them for
            // multi-line
            if ( label.Find('\n' ) == wxNOT_FOUND && label.Find('\r' ) == wxNOT_FOUND)
            {
                if ( (style & wxBORDER_MASK) == wxBORDER_RAISED )
                    [v setBezelStyle:NSRoundedBezelStyle];
                else if ( (style & wxBORDER_MASK) == wxBORDER_SUNKEN )
                    [v setBezelStyle:NSTexturedRoundedBezelStyle];
                else if ( (style & wxBORDER_MASK) == wxBORDER_SIMPLE )
                    [v setBezelStyle:NSShadowlessSquareBezelStyle];
                else
                    [v setBezelStyle:NSRoundedBezelStyle];
            }
            else 
            {
                if ( (style & wxBORDER_MASK) == wxBORDER_RAISED )
                    [v setBezelStyle:NSRegularSquareBezelStyle];
                else if ( (style & wxBORDER_MASK) == wxBORDER_SUNKEN )
                    [v setBezelStyle:NSSmallSquareBezelStyle];
                else if ( (style & wxBORDER_MASK) == wxBORDER_SIMPLE )
                    [v setBezelStyle:NSShadowlessSquareBezelStyle];
                else
                    [v setBezelStyle:NSRegularSquareBezelStyle];
            }

        }
    }
    
    [v setButtonType:NSMomentaryPushInButton];
    return new wxButtonCocoaImpl( wxpeer, v );
}

void wxWidgetCocoaImpl::SetDefaultButton( bool isDefault )
{
    if ( [m_osxView isKindOfClass:[NSButton class]] )
    {
        if ( isDefault )
            [(NSButton*)m_osxView setKeyEquivalent: @"\r" ];
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
                                                   wxWindowID WXUNUSED(id),
                                                   const wxBitmap& bitmap,
                                                   const wxPoint& pos,
                                                   const wxSize& size,
                                                   long style,
                                                   long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSButton* v = [[wxNSButton alloc] initWithFrame:r];

    SetBezelStyleFromBorderFlags(v, style);

    if (bitmap.IsOk())
        [v setImage:bitmap.GetNSImage() ];

    [v setButtonType:NSMomentaryPushInButton];
    wxWidgetCocoaImpl* c = new wxButtonCocoaImpl( wxpeer, v );
    return c;
}

#endif // wxUSE_BMPBUTTON

//
// wxDisclosureButton implementation
//

@interface wxDisclosureNSButton : NSButton
{

    BOOL isOpen;
}

- (void) updateImage;

- (void) toggle;

+ (NSImage *)rotateImage: (NSImage *)image;

@end

static const char * disc_triangle_xpm[] = {
"10 9 4 1",
"   c None",
".  c #737373",
"+  c #989898",
"-  c #c6c6c6",
" .-       ",
" ..+-     ",
" ....+    ",
" ......-  ",
" .......- ",
" ......-  ",
" ....+    ",
" ..+-     ",
" .-       ",
};

@implementation wxDisclosureNSButton

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
    isOpen = NO;
    [self setImagePosition:NSImageLeft];
    [self updateImage];
    return self;
}

- (int) intValue
{
    return isOpen ? 1 : 0;
}

- (void) setIntValue: (int) v
{
    isOpen = ( v != 0 );
    [self updateImage];
}

- (void) toggle
{
    isOpen = !isOpen;
    [self updateImage];
}

wxCFRef<NSImage*> downArray ;

- (void) updateImage
{
    static wxBitmap trianglebm(disc_triangle_xpm);
    if ( downArray.get() == NULL )
    {
        downArray.reset( [[wxDisclosureNSButton rotateImage:trianglebm.GetNSImage()] retain] );
    }

    if ( isOpen )
        [self setImage:(NSImage*)downArray.get()];
    else
        [self setImage:trianglebm.GetNSImage()];
}

+ (NSImage *)rotateImage: (NSImage *)image
{
    NSSize imageSize = [image size];
    NSSize newImageSize = NSMakeSize(imageSize.height, imageSize.width);
    NSImage* newImage = [[NSImage alloc] initWithSize: newImageSize];

    [newImage lockFocus];

    NSAffineTransform* tm = [NSAffineTransform transform];
    [tm translateXBy:newImageSize.width/2 yBy:newImageSize.height/2];
    [tm rotateByDegrees:-90];
    [tm translateXBy:-newImageSize.width/2 yBy:-newImageSize.height/2];
    [tm concat];


    [image drawInRect:NSMakeRect(0,0,newImageSize.width, newImageSize.height)
        fromRect:NSZeroRect operation:NSCompositeCopy fraction:1.0];

    [newImage unlockFocus];
    return [newImage autorelease];
}

@end

class wxDisclosureTriangleCocoaImpl : public wxWidgetCocoaImpl
{
public :
    wxDisclosureTriangleCocoaImpl(wxWindowMac* peer , WXWidget w) :
        wxWidgetCocoaImpl(peer, w)
    {
    }

    ~wxDisclosureTriangleCocoaImpl()
    {
    }

    virtual void controlAction(WXWidget slf, void* _cmd, void *sender)
    {
        wxDisclosureNSButton* db = (wxDisclosureNSButton*)m_osxView;
        [db toggle];
        wxWidgetCocoaImpl::controlAction(slf, _cmd, sender );
    }
};

wxWidgetImplType* wxWidgetImpl::CreateDisclosureTriangle( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(winid),
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxDisclosureNSButton* v = [[wxDisclosureNSButton alloc] initWithFrame:r];
    if ( !label.empty() )
        [v setTitle:wxCFStringRef(label).AsNSString()];

    SetBezelStyleFromBorderFlags(v, style);

    return new wxDisclosureTriangleCocoaImpl( wxpeer, v );
}
