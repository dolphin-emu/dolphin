/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/bmpbuttn.cpp
// Purpose:     wxBitmapButton
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: bmpbuttn.cpp 65680 2010-09-30 11:44:45Z VZ $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_BMPBUTTON

#include "wx/bmpbuttn.h"
#include "wx/image.h"

#ifndef WX_PRECOMP
    #include "wx/dcmemory.h"
#endif

#include "wx/osx/private.h"

namespace
{

// define a derived class to override SetBitmap() and also to provide
// InitButtonContentInfo() helper used by CreateBitmapButton()
class wxMacBitmapButton : public wxMacControl, public wxButtonImpl
{
public:
    wxMacBitmapButton(wxWindowMac* peer, const wxBitmap& bitmap, int style)
        : wxMacControl(peer)
    {
        // decide what kind of contents the button will have: we want to use an
        // icon for buttons with wxBORDER_NONE style as bevel buttons always do
        // have a border but icons are limited to a few standard sizes only and
        // are resized by the system with extremely ugly results if they don't
        // fit (in the past we also tried being smart and pasting a bitmap
        // instead of a larger square icon to avoid resizing but this resulted
        // in buttons having different size than specified by wx API and
        // breaking the layouts and still didn't look good so we don't even try
        // to do this any more)
        m_isIcon = (style & wxBORDER_NONE) &&
                        bitmap.IsOk() && IsOfStandardSize(bitmap);
    }

    virtual void SetBitmap(const wxBitmap& bitmap)
    {
        // unfortunately we can't avoid the ugly resizing problem mentioned
        // above if a bitmap of supported size was used initially but was
        // replaced with another one later as the control was already created
        // as an icon control (although maybe we ought to recreate it?)
        ControlButtonContentInfo info;
        InitButtonContentInfo(info, bitmap);

        if ( info.contentType == kControlContentIconRef )
            SetData(kControlIconPart, kControlIconContentTag, info);
        else if ( info.contentType != kControlNoContent )
            SetData(kControlButtonPart, kControlBevelButtonContentTag, info);

        wxMacReleaseBitmapButton(&info);
    }

    void InitButtonContentInfo(ControlButtonContentInfo& info,
                               const wxBitmap& bitmap)
    {
        wxMacCreateBitmapButton(&info, bitmap,
                                m_isIcon ? kControlContentIconRef : 0);
    }

    void SetPressedBitmap( const wxBitmap& WXUNUSED(bitmap) )
    {
        // not implemented under Carbon
    }

private:
    // helper function: returns true if the given bitmap is of one of standard
    // sizes supported by OS X icons
    static bool IsOfStandardSize(const wxBitmap& bmp)
    {
        const int w = bmp.GetWidth();

        return bmp.GetHeight() == w &&
                (w == 128 || w == 48 || w == 32 || w == 16);
    }


    // true if this is an icon control, false if it's a bevel button
    bool m_isIcon;

    wxDECLARE_NO_COPY_CLASS(wxMacBitmapButton);
};

} // anonymous namespace

wxWidgetImplType* wxWidgetImpl::CreateBitmapButton( wxWindowMac* wxpeer,
                                    wxWindowMac* parent,
                                    wxWindowID WXUNUSED(id),
                                    const wxBitmap& bitmap,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    wxMacBitmapButton* peer = new wxMacBitmapButton(wxpeer, bitmap, style);

    OSStatus err;
    WXWindow macParent = MAC_WXHWND(parent->MacGetTopLevelWindowRef());
    Rect bounds = wxMacGetBoundsForControl( wxpeer, pos, size );

    ControlButtonContentInfo info;
    peer->InitButtonContentInfo(info, bitmap);

    if ( info.contentType == kControlContentIconRef )
    {
        err = CreateIconControl
              (
                macParent,
                &bounds,
                &info,
                false,
                peer->GetControlRefAddr()
              );
    }
    else // normal bevel button
    {
        err = CreateBevelButtonControl
              (
                macParent,
                &bounds,
                CFSTR(""),
                style & wxBU_AUTODRAW ? kControlBevelButtonSmallBevel
                                      : kControlBevelButtonNormalBevel,
                kControlBehaviorOffsetContents,
                &info,
                0,  // menu id (no associated menu)
                0,  // menu behaviour (unused)
                0,  // menu placement (unused too)
                peer->GetControlRefAddr()
              );
    }

    verify_noerr( err );

    wxMacReleaseBitmapButton( &info );
    return peer;
}

#endif // wxUSE_BMPBUTTON
