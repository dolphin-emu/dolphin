/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dcmemory.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_DCMEMORY_H_
#define _WX_GTK_DCMEMORY_H_

#include "wx/dcmemory.h"
#include "wx/gtk/dcclient.h"

//-----------------------------------------------------------------------------
// wxMemoryDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMemoryDCImpl : public wxWindowDCImpl
{
public:
    wxMemoryDCImpl( wxMemoryDC *owner );
    wxMemoryDCImpl( wxMemoryDC *owner, wxBitmap& bitmap );
    wxMemoryDCImpl( wxMemoryDC *owner, wxDC *dc );

    virtual ~wxMemoryDCImpl();

    // these get reimplemented for mono-bitmaps to behave
    // more like their Win32 couterparts. They now interpret
    // wxWHITE, wxWHITE_BRUSH and wxWHITE_PEN as drawing 0
    // and everything else as drawing 1.
    virtual void SetPen( const wxPen &pen ) wxOVERRIDE;
    virtual void SetBrush( const wxBrush &brush ) wxOVERRIDE;
    virtual void SetBackground( const wxBrush &brush ) wxOVERRIDE;
    virtual void SetTextForeground( const wxColour &col ) wxOVERRIDE;
    virtual void SetTextBackground( const wxColour &col ) wxOVERRIDE;

    // overridden from wxDCImpl
    virtual void DoGetSize( int *width, int *height ) const wxOVERRIDE;
    virtual wxBitmap DoGetAsBitmap(const wxRect *subrect) const wxOVERRIDE;
    virtual void* GetHandle() const wxOVERRIDE;
    
    // overridden for wxMemoryDC Impl
    virtual void DoSelect(const wxBitmap& bitmap) wxOVERRIDE;

    virtual const wxBitmap& GetSelectedBitmap() const wxOVERRIDE;
    virtual wxBitmap& GetSelectedBitmap() wxOVERRIDE;

private:
    wxBitmap  m_selected;

    void Init();

    wxDECLARE_ABSTRACT_CLASS(wxMemoryDCImpl);
};

#endif
    // _WX_GTK_DCMEMORY_H_

