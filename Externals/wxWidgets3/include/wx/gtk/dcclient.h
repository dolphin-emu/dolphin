/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dcclient.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKDCCLIENT_H_
#define _WX_GTKDCCLIENT_H_

#include "wx/gtk/dc.h"

//-----------------------------------------------------------------------------
// wxWindowDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxWindowDCImpl : public wxGTKDCImpl
{
public:
    wxWindowDCImpl( wxDC *owner );
    wxWindowDCImpl( wxDC *owner, wxWindow *win );

    virtual ~wxWindowDCImpl();

    virtual bool CanDrawBitmap() const wxOVERRIDE { return true; }
    virtual bool CanGetTextExtent() const wxOVERRIDE { return true; }

    virtual void DoGetSize(int *width, int *height) const wxOVERRIDE;
    virtual bool DoFloodFill( wxCoord x, wxCoord y, const wxColour& col,
                              wxFloodFillStyle style=wxFLOOD_SURFACE ) wxOVERRIDE;
    virtual bool DoGetPixel( wxCoord x1, wxCoord y1, wxColour *col ) const wxOVERRIDE;

    virtual void DoDrawLine( wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2 ) wxOVERRIDE;
    virtual void DoCrossHair( wxCoord x, wxCoord y ) wxOVERRIDE;
    virtual void DoDrawArc( wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2,
                            wxCoord xc, wxCoord yc ) wxOVERRIDE;
    virtual void DoDrawEllipticArc( wxCoord x, wxCoord y, wxCoord width, wxCoord height,
                                    double sa, double ea ) wxOVERRIDE;
    virtual void DoDrawPoint( wxCoord x, wxCoord y ) wxOVERRIDE;

    virtual void DoDrawLines(int n, const wxPoint points[],
                             wxCoord xoffset, wxCoord yoffset) wxOVERRIDE;
    virtual void DoDrawPolygon(int n, const wxPoint points[],
                               wxCoord xoffset, wxCoord yoffset,
                               wxPolygonFillMode fillStyle = wxODDEVEN_RULE) wxOVERRIDE;

    virtual void DoDrawRectangle( wxCoord x, wxCoord y, wxCoord width, wxCoord height ) wxOVERRIDE;
    virtual void DoDrawRoundedRectangle( wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius = 20.0 ) wxOVERRIDE;
    virtual void DoDrawEllipse( wxCoord x, wxCoord y, wxCoord width, wxCoord height ) wxOVERRIDE;

    virtual void DoDrawIcon( const wxIcon &icon, wxCoord x, wxCoord y ) wxOVERRIDE;
    virtual void DoDrawBitmap( const wxBitmap &bitmap, wxCoord x, wxCoord y,
                               bool useMask = false ) wxOVERRIDE;

    virtual bool DoBlit( wxCoord xdest, wxCoord ydest,
                         wxCoord width, wxCoord height,
                         wxDC *source, wxCoord xsrc, wxCoord ysrc,
                         wxRasterOperationMode logical_func = wxCOPY,
                         bool useMask = false,
                         wxCoord xsrcMask = -1, wxCoord ysrcMask = -1 ) wxOVERRIDE;

    virtual void DoDrawText( const wxString &text, wxCoord x, wxCoord y ) wxOVERRIDE;
    virtual void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y,
                                   double angle) wxOVERRIDE;
    virtual void DoGetTextExtent( const wxString &string,
                                wxCoord *width, wxCoord *height,
                                wxCoord *descent = NULL,
                                wxCoord *externalLeading = NULL,
                                const wxFont *theFont = NULL) const wxOVERRIDE;
    virtual bool DoGetPartialTextExtents(const wxString& text, wxArrayInt& widths) const wxOVERRIDE;
    virtual void DoSetClippingRegion( wxCoord x, wxCoord y, wxCoord width, wxCoord height ) wxOVERRIDE;
    virtual void DoSetDeviceClippingRegion( const wxRegion &region ) wxOVERRIDE;

    virtual wxCoord GetCharWidth() const wxOVERRIDE;
    virtual wxCoord GetCharHeight() const wxOVERRIDE;

    virtual void Clear() wxOVERRIDE;

    virtual void SetFont( const wxFont &font ) wxOVERRIDE;
    virtual void SetPen( const wxPen &pen ) wxOVERRIDE;
    virtual void SetBrush( const wxBrush &brush ) wxOVERRIDE;
    virtual void SetBackground( const wxBrush &brush ) wxOVERRIDE;
    virtual void SetLogicalFunction( wxRasterOperationMode function ) wxOVERRIDE;
    virtual void SetTextForeground( const wxColour &col ) wxOVERRIDE;
    virtual void SetTextBackground( const wxColour &col ) wxOVERRIDE;
    virtual void SetBackgroundMode( int mode ) wxOVERRIDE;
    virtual void SetPalette( const wxPalette& palette ) wxOVERRIDE;

    virtual void DestroyClippingRegion() wxOVERRIDE;

    // Resolution in pixels per logical inch
    virtual wxSize GetPPI() const wxOVERRIDE;
    virtual int GetDepth() const wxOVERRIDE;

    // overrriden here for RTL
    virtual void SetDeviceOrigin( wxCoord x, wxCoord y ) wxOVERRIDE;
    virtual void SetAxisOrientation( bool xLeftRight, bool yBottomUp ) wxOVERRIDE;

// protected:
    // implementation
    // --------------

    GdkWindow    *m_gdkwindow;
    GdkGC        *m_penGC;
    GdkGC        *m_brushGC;
    GdkGC        *m_textGC;
    GdkGC        *m_bgGC;
    GdkColormap  *m_cmap;
    bool          m_isScreenDC;
    wxRegion      m_currentClippingRegion;
    wxRegion      m_paintClippingRegion;

    // PangoContext stuff for GTK 2.0
    PangoContext *m_context;
    PangoLayout *m_layout;
    PangoFontDescription *m_fontdesc;

    void SetUpDC( bool ismem = false );
    void Destroy();

    virtual void ComputeScaleAndOrigin() wxOVERRIDE;

    virtual GdkWindow *GetGDKWindow() const wxOVERRIDE { return m_gdkwindow; }

private:
    void DrawingSetup(GdkGC*& gc, bool& originChanged);
    GdkPixmap* MonoToColor(GdkPixmap* monoPixmap, int x, int y, int w, int h) const;

    wxDECLARE_ABSTRACT_CLASS(wxWindowDCImpl);
};

//-----------------------------------------------------------------------------
// wxClientDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxClientDCImpl : public wxWindowDCImpl
{
public:
    wxClientDCImpl( wxDC *owner );
    wxClientDCImpl( wxDC *owner, wxWindow *win );

    virtual void DoGetSize(int *width, int *height) const wxOVERRIDE;

    wxDECLARE_ABSTRACT_CLASS(wxClientDCImpl);
};

//-----------------------------------------------------------------------------
// wxPaintDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPaintDCImpl : public wxClientDCImpl
{
public:
    wxPaintDCImpl( wxDC *owner );
    wxPaintDCImpl( wxDC *owner, wxWindow *win );

    wxDECLARE_ABSTRACT_CLASS(wxPaintDCImpl);
};

#endif // _WX_GTKDCCLIENT_H_
