/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/stattext.mm
// Purpose:     wxStaticText
// Author:      Stefan Csomor
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: stattext.mm 67232 2011-03-18 15:10:15Z DS $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_STATTEXT

#include "wx/stattext.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/dc.h"
    #include "wx/dcclient.h"
    #include "wx/settings.h"
#endif // WX_PRECOMP

#include "wx/osx/private.h"

#if wxUSE_MARKUP
    #include "wx/osx/cocoa/private/markuptoattr.h"
#endif // wxUSE_MARKUP

#include <stdio.h>

@interface wxNSStaticTextView : NSTextField
{
}
@end

@implementation wxNSStaticTextView

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (void) setEnabled:(BOOL) flag 
{ 
    [super setEnabled: flag]; 
    
    if (![self drawsBackground]) { 
        // Static text is drawn incorrectly when disabled. 
        // For an explanation, see 
        // http://www.cocoabuilder.com/archive/message/cocoa/2006/7/21/168028 
        if (flag)
        { 
            [self setTextColor: [NSColor controlTextColor]]; 
        }
        else 
        { 
            [self setTextColor: [NSColor secondarySelectedControlColor]]; 
        } 
    } 
} 

@end

class wxStaticTextCocoaImpl : public wxWidgetCocoaImpl
{
public:
    wxStaticTextCocoaImpl( wxWindowMac* peer , WXWidget w , NSLineBreakMode lineBreak) : wxWidgetCocoaImpl(peer, w)
    {
        m_lineBreak = lineBreak;
    }

    virtual void SetLabel(const wxString& title, wxFontEncoding encoding)
    {
        wxCFStringRef text( title , encoding );

        NSMutableAttributedString *
            attrstring = [[NSMutableAttributedString alloc] initWithString:text.AsNSString()];
        DoSetAttrString(attrstring);
        [attrstring release];
    }

#if wxUSE_MARKUP
    virtual void SetLabelMarkup( const wxString& markup)
    {
        wxMarkupToAttrString toAttr(GetWXPeer(), markup);

        DoSetAttrString(toAttr.GetNSAttributedString());
    }
#endif // wxUSE_MARKUP

private:
    void DoSetAttrString(NSMutableAttributedString *attrstring)
    {
        NSMutableParagraphStyle *paragraphStyle = [[NSMutableParagraphStyle alloc] init];
        [paragraphStyle setLineBreakMode:m_lineBreak];
        int style = GetWXPeer()->GetWindowStyleFlag();
        if (style & wxALIGN_CENTER)
            [paragraphStyle setAlignment: NSCenterTextAlignment];
        else if (style & wxALIGN_RIGHT)
            [paragraphStyle setAlignment: NSRightTextAlignment];

        [attrstring addAttribute:NSParagraphStyleAttributeName
                    value:paragraphStyle
                    range:NSMakeRange(0, [attrstring length])];
        NSCell* cell = [(wxNSStaticTextView *)GetWXWidget() cell];
        [cell setAttributedStringValue:attrstring];
        [paragraphStyle release];
    }

    NSLineBreakMode m_lineBreak;
};

wxSize wxStaticText::DoGetBestSize() const
{
    return wxWindowMac::DoGetBestSize() ;
}

wxWidgetImplType* wxWidgetImpl::CreateStaticText( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxString& WXUNUSED(label),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSStaticTextView* v = [[wxNSStaticTextView alloc] initWithFrame:r];

    [v setEditable:NO];
    [v setDrawsBackground:NO];
    [v setSelectable: NO];
    [v setBezeled:NO];
    [v setBordered:NO];

    NSLineBreakMode linebreak = NSLineBreakByWordWrapping;
    if ( ((wxStaticText*)wxpeer)->IsEllipsized() )
    {
        if ( style & wxST_ELLIPSIZE_MIDDLE )
            linebreak = NSLineBreakByTruncatingMiddle;
        else if (style & wxST_ELLIPSIZE_END )
            linebreak = NSLineBreakByTruncatingTail;
        else if (style & wxST_ELLIPSIZE_START )
            linebreak = NSLineBreakByTruncatingHead;
    }
    else
    {
        [[v cell] setWraps:YES];
    }

    wxWidgetCocoaImpl* c = new wxStaticTextCocoaImpl( wxpeer, v, linebreak );
    return c;
}

#endif //if wxUSE_STATTEXT
