/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/iphone/slider.mm
// Purpose:     wxSlider
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_SLIDER

#include "wx/slider.h"
#include "wx/osx/private.h"

@interface wxUISlider : UISlider
{
}
@end

@implementation wxUISlider

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXIPhoneClassAddWXMethods(self);
    }
}

@end

class wxSliderIPhoneImpl : public wxWidgetIPhoneImpl
{
public :
    wxSliderIPhoneImpl(wxWindowMac* peer , UISlider* w) :
        wxWidgetIPhoneImpl(peer, w)
    {
        m_control=w;
    }

    ~wxSliderIPhoneImpl()
    {
    }
    
    void controlAction(void* sender, wxUint32 controlEvent, WX_UIEvent rawEvent)
    {
        if ( controlEvent == UIControlEventValueChanged )
            GetWXPeer()->TriggerScrollEvent(wxEVT_SCROLL_THUMBTRACK);
        else 
            wxWidgetIPhoneImpl::controlAction(sender,controlEvent,rawEvent);
    }

    void SetMaximum(wxInt32 m)
    {
        [m_control setMaximumValue:m];
    }
    
    void SetMinimum(wxInt32 m)
    {
        [m_control setMinimumValue:m];
    }
    
    void SetValue(wxInt32 n)
    {
        [m_control setValue:n];
    }

    wxInt32  GetValue() const
    {
        return [m_control value];
    }
    
private:
    UISlider* m_control;
};

wxWidgetImplType* wxWidgetImpl::CreateSlider( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    wxInt32 value,
                                    wxInt32 minimum,
                                    wxInt32 maximum,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    CGRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    UISlider* v = [[UISlider alloc] initWithFrame:r];

    [v setMinimumValue: minimum];
    [v setMaximumValue: maximum];
    [v setValue: (double) value];

    wxWidgetIPhoneImpl* c = new wxSliderIPhoneImpl( wxpeer, v );
    return c;
}

#endif // wxUSE_SLIDER
