/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/iphone/gauge.mm
// Purpose:     wxGauge class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_GAUGE

#include "wx/gauge.h"

#include "wx/osx/private.h"

@interface wxUIProgressView : UIProgressView
{
}

@end

@implementation wxUIProgressView

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

class wxOSXGaugeIPhoneImpl : public wxWidgetIPhoneImpl
{
public :
    wxOSXGaugeIPhoneImpl( wxWindowMac* peer, WXWidget w) : wxWidgetIPhoneImpl( peer, w )
    {
    }

    void SetMaximum(wxInt32 m)
    {
        wxUIProgressView* v =  (wxUIProgressView*)GetWXWidget();
        wxGauge* wxpeer = (wxGauge*) GetWXPeer();
        SetDeterminateMode();
        [v setProgress:(float) wxpeer->GetValue() / m];
    }

    void SetValue(wxInt32 n)
    {
        wxUIProgressView* v =  (wxUIProgressView*)GetWXWidget();
        wxGauge* wxpeer = (wxGauge*) GetWXPeer();
        SetDeterminateMode();
        [v setProgress:(float) n / wxpeer->GetRange()];
    }

    void PulseGauge()
    {
    }
protected:
    void SetDeterminateMode()
    {
       // switch back to determinate mode if necessary
    }
};


wxWidgetImplType* wxWidgetImpl::CreateGauge( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    wxInt32 value,
                                    wxInt32 minimum,
                                    wxInt32 maximum,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    CGRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxUIProgressView* v = [[wxUIProgressView alloc] initWithFrame:r];
    [v setProgress:(float) value/maximum];

    wxWidgetIPhoneImpl* c = new wxOSXGaugeIPhoneImpl( wxpeer, v );
    return c;
}

#endif // wxUSE_GAUGE

