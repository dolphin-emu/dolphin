/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/iphone/checkbox.mm
// Purpose:     wxCheckBox
// Author:      Stefan Csomor
// Modified by:
// Created:     2008-08-20
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_CHECKBOX

#include "wx/checkbox.h"
#include "wx/osx/private.h"

@interface wxUISwitch : UISwitch
{
}

@end

@implementation wxUISwitch

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXIPhoneClassAddWXMethods( self );
    }
}

@end

class wxCheckBoxIPhoneImpl : public wxWidgetIPhoneImpl
{
public:
    wxCheckBoxIPhoneImpl(wxWindowMac *wxpeer, UISwitch *v)
    : wxWidgetIPhoneImpl(wxpeer, v)
    {
        m_control = v;
    }
    
    wxInt32  GetValue() const
    {
        return [m_control isOn] ? 1 : 0;
    }
    
    void SetValue( wxInt32 v ) 
    {
        [m_control setOn:v != 0 animated:NO];
    }
private:
    UISwitch* m_control;
};

wxWidgetImplType* wxWidgetImpl::CreateCheckBox( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxString& WXUNUSED(label),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    CGRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxUISwitch* v = [[wxUISwitch alloc] initWithFrame:r];

//    if (style & wxCHK_3STATE)
//        [v setAllowsMixedState:YES];

    wxCheckBoxIPhoneImpl* c = new wxCheckBoxIPhoneImpl( wxpeer, v );
    return c;
}

#endif
