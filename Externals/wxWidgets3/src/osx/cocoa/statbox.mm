/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/statbox.mm
// Purpose:     wxStaticBox
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: statbox.mm 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_STATBOX

#include "wx/statbox.h"
#include "wx/osx/private.h"

@implementation wxNSBox

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

@end

namespace
{
    class wxStaticBoxCocoaImpl : public wxWidgetCocoaImpl
    {
    public:
        wxStaticBoxCocoaImpl(wxWindowMac *wxpeer, wxNSBox *v)
        : wxWidgetCocoaImpl(wxpeer, v)
        {
        }

        virtual void SetLabel( const wxString& title, wxFontEncoding encoding )
        {
            if (title.empty())
                [GetNSBox() setTitlePosition:NSNoTitle];
            else
                [GetNSBox() setTitlePosition:NSAtTop];

            wxWidgetCocoaImpl::SetLabel(title, encoding);
        }

    private:
        NSBox *GetNSBox() const
        {
            wxASSERT( [m_osxView isKindOfClass:[NSBox class]] );

            return static_cast<NSBox*>(m_osxView);
        }
    };
} // anonymous namespace


wxWidgetImplType* wxWidgetImpl::CreateGroupBox( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxString& WXUNUSED(label),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSBox* v = [[wxNSBox alloc] initWithFrame:r];
    wxStaticBoxCocoaImpl* c = new wxStaticBoxCocoaImpl( wxpeer, v );
    c->SetFlipped(false);
    return c;
}

#endif // wxUSE_STATBOX

