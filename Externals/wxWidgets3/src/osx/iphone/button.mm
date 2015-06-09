/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/iphone/button.mm
// Purpose:     wxButton
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/button.h"

#ifndef WX_PRECOMP
    #include "wx/panel.h"
    #include "wx/toplevel.h"
    #include "wx/dcclient.h"
#endif

#include "wx/stockitem.h"

#include "wx/osx/private.h"

@implementation wxUIButton

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXIPhoneClassAddWXMethods( self );
    }
}

- (int) intValue
{
    return 0;
}

- (void) setIntValue: (int) v
{
}

@end


wxWidgetImplType* wxWidgetImpl::CreateButton( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID id,
                                    const wxString& WXUNUSED(label),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    CGRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    UIButtonType buttonType = UIButtonTypeRoundedRect;

    if ( id == wxID_HELP )
    {
        buttonType = UIButtonTypeInfoDark;
    }

    UIButton* v = [[UIButton buttonWithType:buttonType] retain];
    v.frame = r;
    wxWidgetIPhoneImpl* c = new wxWidgetIPhoneImpl( wxpeer, v );
    return c;
}

void wxWidgetIPhoneImpl::SetDefaultButton( bool isDefault )
{
}

void wxWidgetIPhoneImpl::PerformClick()
{
}

wxWidgetImplType* wxWidgetImpl::CreateDisclosureTriangle( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxString& label,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    CGRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxUIButton* v = [[wxUIButton alloc] initWithFrame:r];
    [v setTitle:wxCFStringRef( label).AsNSString() forState:UIControlStateNormal];
    wxWidgetIPhoneImpl* c = new wxWidgetIPhoneImpl( wxpeer, v );
    return c;
}
