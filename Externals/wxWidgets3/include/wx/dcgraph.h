/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dcgraph.h
// Purpose:     graphics context device bridge header
// Author:      Stefan Csomor
// Modified by:
// Created:
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GRAPHICS_DC_H_
#define _WX_GRAPHICS_DC_H_

#if wxUSE_GRAPHICS_CONTEXT

#include "wx/dc.h"
#include "wx/geometry.h"
#include "wx/graphics.h"

class WXDLLIMPEXP_FWD_CORE wxWindowDC;


class WXDLLIMPEXP_CORE wxGCDC: public wxDC
{
public:
    wxGCDC( const wxWindowDC& dc );
    wxGCDC( const wxMemoryDC& dc );
#if wxUSE_PRINTING_ARCHITECTURE
    wxGCDC( const wxPrinterDC& dc );
#endif
#if defined(__WXMSW__) && wxUSE_ENH_METAFILE
    wxGCDC( const wxEnhMetaFileDC& dc );
#endif
    wxGCDC(wxGraphicsContext* context);
    
    wxGCDC();
    virtual ~wxGCDC();

#ifdef __WXMSW__
    // override wxDC virtual functions to provide access to HDC associated with
    // this Graphics object (implemented in src/msw/graphics.cpp)
    virtual WXHDC AcquireHDC();
    virtual void ReleaseHDC(WXHDC hdc);
#endif // __WXMSW__

private:
    wxDECLARE_DYNAMIC_CLASS(wxGCDC);
    wxDECLARE_NO_COPY_CLASS(wxGCDC);
};


class WXDLLIMPEXP_CORE wxGCDCImpl: public wxDCImpl
{
public:
    wxGCDCImpl( wxDC *owner, const wxWindowDC& dc );
    wxGCDCImpl( wxDC *owner, const wxMemoryDC& dc );
#if wxUSE_PRINTING_ARCHITECTURE
    wxGCDCImpl( wxDC *owner, const wxPrinterDC& dc );
#endif
#if defined(__WXMSW__) && wxUSE_ENH_METAFILE
    wxGCDCImpl( wxDC *owner, const wxEnhMetaFileDC& dc );
#endif
    wxGCDCImpl( wxDC *owner );

    virtual ~wxGCDCImpl();

    // implement base class pure virtuals
    // ----------------------------------

    virtual void Clear() wxOVERRIDE;

    virtual bool StartDoc( const wxString& message ) wxOVERRIDE;
    virtual void EndDoc() wxOVERRIDE;

    virtual void StartPage() wxOVERRIDE;
    virtual void EndPage() wxOVERRIDE;

    // flushing the content of this dc immediately onto screen
    virtual void Flush() wxOVERRIDE;

    virtual void SetFont(const wxFont& font) wxOVERRIDE;
    virtual void SetPen(const wxPen& pen) wxOVERRIDE;
    virtual void SetBrush(const wxBrush& brush) wxOVERRIDE;
    virtual void SetBackground(const wxBrush& brush) wxOVERRIDE;
    virtual void SetBackgroundMode(int mode) wxOVERRIDE;
    virtual void SetPalette(const wxPalette& palette) wxOVERRIDE;

    virtual void DestroyClippingRegion() wxOVERRIDE;

    virtual wxCoord GetCharHeight() const wxOVERRIDE;
    virtual wxCoord GetCharWidth() const wxOVERRIDE;

    virtual bool CanDrawBitmap() const wxOVERRIDE;
    virtual bool CanGetTextExtent() const wxOVERRIDE;
    virtual int GetDepth() const wxOVERRIDE;
    virtual wxSize GetPPI() const wxOVERRIDE;

    virtual void SetLogicalFunction(wxRasterOperationMode function) wxOVERRIDE;

    virtual void SetTextForeground(const wxColour& colour) wxOVERRIDE;
    virtual void SetTextBackground(const wxColour& colour) wxOVERRIDE;

    virtual void ComputeScaleAndOrigin() wxOVERRIDE;

    wxGraphicsContext* GetGraphicsContext() const wxOVERRIDE { return m_graphicContext; }
    virtual void SetGraphicsContext( wxGraphicsContext* ctx ) wxOVERRIDE;

    virtual void* GetHandle() const wxOVERRIDE;

    // the true implementations
    virtual bool DoFloodFill(wxCoord x, wxCoord y, const wxColour& col,
                             wxFloodFillStyle style = wxFLOOD_SURFACE) wxOVERRIDE;

    virtual void DoGradientFillLinear(const wxRect& rect,
        const wxColour& initialColour,
        const wxColour& destColour,
        wxDirection nDirection = wxEAST) wxOVERRIDE;

    virtual void DoGradientFillConcentric(const wxRect& rect,
        const wxColour& initialColour,
        const wxColour& destColour,
        const wxPoint& circleCenter) wxOVERRIDE;

    virtual bool DoGetPixel(wxCoord x, wxCoord y, wxColour *col) const wxOVERRIDE;

    virtual void DoDrawPoint(wxCoord x, wxCoord y) wxOVERRIDE;

#if wxUSE_SPLINES
    virtual void DoDrawSpline(const wxPointList *points) wxOVERRIDE;
#endif

    virtual void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2) wxOVERRIDE;

    virtual void DoDrawArc(wxCoord x1, wxCoord y1,
        wxCoord x2, wxCoord y2,
        wxCoord xc, wxCoord yc) wxOVERRIDE;

    virtual void DoDrawCheckMark(wxCoord x, wxCoord y,
        wxCoord width, wxCoord height) wxOVERRIDE;

    virtual void DoDrawEllipticArc(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
        double sa, double ea) wxOVERRIDE;

    virtual void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height) wxOVERRIDE;
    virtual void DoDrawRoundedRectangle(wxCoord x, wxCoord y,
        wxCoord width, wxCoord height,
        double radius) wxOVERRIDE;
    virtual void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height) wxOVERRIDE;

    virtual void DoCrossHair(wxCoord x, wxCoord y) wxOVERRIDE;

    virtual void DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y) wxOVERRIDE;
    virtual void DoDrawBitmap(const wxBitmap &bmp, wxCoord x, wxCoord y,
        bool useMask = false) wxOVERRIDE;

    virtual void DoDrawText(const wxString& text, wxCoord x, wxCoord y) wxOVERRIDE;
    virtual void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y,
        double angle) wxOVERRIDE;

    virtual bool DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
                        wxDC *source, wxCoord xsrc, wxCoord ysrc,
                        wxRasterOperationMode rop = wxCOPY, bool useMask = false,
                        wxCoord xsrcMask = -1, wxCoord ysrcMask = -1) wxOVERRIDE;

    virtual bool DoStretchBlit(wxCoord xdest, wxCoord ydest,
                               wxCoord dstWidth, wxCoord dstHeight,
                               wxDC *source,
                               wxCoord xsrc, wxCoord ysrc,
                               wxCoord srcWidth, wxCoord srcHeight,
                               wxRasterOperationMode = wxCOPY, bool useMask = false,
                               wxCoord xsrcMask = wxDefaultCoord, wxCoord ysrcMask = wxDefaultCoord) wxOVERRIDE;

    virtual void DoGetSize(int *,int *) const wxOVERRIDE;
    virtual void DoGetSizeMM(int* width, int* height) const wxOVERRIDE;

    virtual void DoDrawLines(int n, const wxPoint points[],
        wxCoord xoffset, wxCoord yoffset) wxOVERRIDE;
    virtual void DoDrawPolygon(int n, const wxPoint points[],
                               wxCoord xoffset, wxCoord yoffset,
                               wxPolygonFillMode fillStyle = wxODDEVEN_RULE) wxOVERRIDE;
    virtual void DoDrawPolyPolygon(int n, const int count[], const wxPoint points[],
                                   wxCoord xoffset, wxCoord yoffset,
                                   wxPolygonFillMode fillStyle) wxOVERRIDE;

    virtual void DoSetDeviceClippingRegion(const wxRegion& region) wxOVERRIDE;
    virtual void DoSetClippingRegion(wxCoord x, wxCoord y,
        wxCoord width, wxCoord height) wxOVERRIDE;

    virtual void DoGetTextExtent(const wxString& string,
        wxCoord *x, wxCoord *y,
        wxCoord *descent = NULL,
        wxCoord *externalLeading = NULL,
        const wxFont *theFont = NULL) const wxOVERRIDE;

    virtual bool DoGetPartialTextExtents(const wxString& text, wxArrayInt& widths) const wxOVERRIDE;

#ifdef __WXMSW__
    virtual wxRect MSWApplyGDIPlusTransform(const wxRect& r) const;
#endif // __WXMSW__

protected:
    // unused int parameter distinguishes this version, which does not create a
    // wxGraphicsContext, in the expectation that the derived class will do it
    wxGCDCImpl(wxDC* owner, int);

    // scaling variables
    bool m_logicalFunctionSupported;
    wxGraphicsMatrix m_matrixOriginal;
    wxGraphicsMatrix m_matrixCurrent;

    double m_formerScaleX, m_formerScaleY;

    wxGraphicsContext* m_graphicContext;

private:
    void Init(wxGraphicsContext*);

    wxDECLARE_CLASS(wxGCDCImpl);
    wxDECLARE_NO_COPY_CLASS(wxGCDCImpl);
};

#endif // wxUSE_GRAPHICS_CONTEXT
#endif // _WX_GRAPHICS_DC_H_
