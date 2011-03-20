/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dc.h
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: dc.h 50547 2007-12-06 16:22:00Z PC $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKDC_H_
#define _WX_GTKDC_H_

#include "wx/dc.h"

//-----------------------------------------------------------------------------
// wxDC
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGTKDCImpl : public wxDCImpl
{
public:
    wxGTKDCImpl( wxDC *owner );
    virtual ~wxGTKDCImpl();

#if wxUSE_PALETTE
    void SetColourMap( const wxPalette& palette ) { SetPalette(palette); };
#endif // wxUSE_PALETTE

    // Resolution in pixels per logical inch
    virtual wxSize GetPPI() const;

    virtual bool StartDoc( const wxString& WXUNUSED(message) ) { return true; }
    virtual void EndDoc() { }
    virtual void StartPage() { }
    virtual void EndPage() { }

    virtual GdkWindow* GetGDKWindow() const { return NULL; }

    // base class pure virtuals implemented here
    virtual void DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
    virtual void DoGetSizeMM(int* width, int* height) const;

    DECLARE_ABSTRACT_CLASS(wxGTKDCImpl)
};

// this must be defined when wxDC::Blit() honours the DC origin and needed to
// allow wxUniv code in univ/winuniv.cpp to work with versions of wxGTK
// 2.3.[23]
#ifndef wxHAS_WORKING_GTK_DC_BLIT
    #define wxHAS_WORKING_GTK_DC_BLIT
#endif

#endif // _WX_GTKDC_H_
