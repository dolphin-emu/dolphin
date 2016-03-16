/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/print.h
// Author:      Anthony Bretaudeau
// Purpose:     GTK printing support
// Created:     2007-08-25
// Copyright:   (c) Anthony Bretaudeau
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PRINT_H_
#define _WX_GTK_PRINT_H_

#include "wx/defs.h"

#if wxUSE_GTKPRINT

#include "wx/print.h"
#include "wx/printdlg.h"
#include "wx/prntbase.h"
#include "wx/dc.h"

typedef struct _GtkPrintOperation GtkPrintOperation;
typedef struct _GtkPrintContext GtkPrintContext;
typedef struct _GtkPrintSettings GtkPrintSettings;
typedef struct _GtkPageSetup GtkPageSetup;

typedef struct _cairo cairo_t;

//----------------------------------------------------------------------------
// wxGtkPrintFactory
//----------------------------------------------------------------------------

class wxGtkPrintFactory: public wxPrintFactory
{
public:
    virtual wxPrinterBase *CreatePrinter( wxPrintDialogData *data ) wxOVERRIDE;

    virtual wxPrintPreviewBase *CreatePrintPreview( wxPrintout *preview,
                                                    wxPrintout *printout = NULL,
                                                    wxPrintDialogData *data = NULL ) wxOVERRIDE;
    virtual wxPrintPreviewBase *CreatePrintPreview( wxPrintout *preview,
                                                    wxPrintout *printout,
                                                    wxPrintData *data ) wxOVERRIDE;

    virtual wxPrintDialogBase *CreatePrintDialog( wxWindow *parent,
                                                  wxPrintDialogData *data = NULL ) wxOVERRIDE;
    virtual wxPrintDialogBase *CreatePrintDialog( wxWindow *parent,
                                                  wxPrintData *data ) wxOVERRIDE;

    virtual wxPageSetupDialogBase *CreatePageSetupDialog( wxWindow *parent,
                                                          wxPageSetupDialogData * data = NULL ) wxOVERRIDE;

    virtual wxDCImpl* CreatePrinterDCImpl( wxPrinterDC *owner, const wxPrintData& data ) wxOVERRIDE;

    virtual bool HasPrintSetupDialog() wxOVERRIDE;
    virtual wxDialog *CreatePrintSetupDialog( wxWindow *parent, wxPrintData *data ) wxOVERRIDE;
    virtual bool HasOwnPrintToFile() wxOVERRIDE;
    virtual bool HasPrinterLine() wxOVERRIDE;
    virtual wxString CreatePrinterLine() wxOVERRIDE;
    virtual bool HasStatusLine() wxOVERRIDE;
    virtual wxString CreateStatusLine() wxOVERRIDE;

    virtual wxPrintNativeDataBase *CreatePrintNativeData() wxOVERRIDE;
};

//----------------------------------------------------------------------------
// wxGtkPrintDialog
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGtkPrintDialog: public wxPrintDialogBase
{
public:
    wxGtkPrintDialog( wxWindow *parent,
                         wxPrintDialogData* data = NULL );
    wxGtkPrintDialog( wxWindow *parent, wxPrintData* data);
    virtual ~wxGtkPrintDialog();

    wxPrintData& GetPrintData() wxOVERRIDE
        { return m_printDialogData.GetPrintData(); }
    wxPrintDialogData& GetPrintDialogData() wxOVERRIDE
        { return m_printDialogData; }

    wxDC *GetPrintDC() wxOVERRIDE { return m_dc; }
    void SetPrintDC(wxDC * printDC) { m_dc = printDC; }

    virtual int ShowModal() wxOVERRIDE;

    virtual bool Validate() wxOVERRIDE { return true; }
    virtual bool TransferDataToWindow() wxOVERRIDE { return true; }
    virtual bool TransferDataFromWindow() wxOVERRIDE { return true; }

    void SetShowDialog(bool show) { m_showDialog = show; }
    bool GetShowDialog() { return m_showDialog; }

protected:
    // Implement some base class methods to do nothing to avoid asserts and
    // GTK warnings, since this is not a real wxDialog.
    virtual void DoSetSize(int WXUNUSED(x), int WXUNUSED(y),
                           int WXUNUSED(width), int WXUNUSED(height),
                           int WXUNUSED(sizeFlags) = wxSIZE_AUTO) wxOVERRIDE {}
    virtual void DoMoveWindow(int WXUNUSED(x), int WXUNUSED(y),
                              int WXUNUSED(width), int WXUNUSED(height)) wxOVERRIDE {}

private:
    wxPrintDialogData    m_printDialogData;
    wxWindow            *m_parent;
    bool                 m_showDialog;
    wxDC                *m_dc;

    wxDECLARE_DYNAMIC_CLASS(wxGtkPrintDialog);
};

//----------------------------------------------------------------------------
// wxGtkPageSetupDialog
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGtkPageSetupDialog: public wxPageSetupDialogBase
{
public:
    wxGtkPageSetupDialog( wxWindow *parent,
                            wxPageSetupDialogData* data = NULL );
    virtual ~wxGtkPageSetupDialog();

    virtual wxPageSetupDialogData& GetPageSetupDialogData() wxOVERRIDE { return m_pageDialogData; }

    virtual int ShowModal() wxOVERRIDE;

    virtual bool Validate() wxOVERRIDE { return true; }
    virtual bool TransferDataToWindow() wxOVERRIDE { return true; }
    virtual bool TransferDataFromWindow() wxOVERRIDE { return true; }

protected:
    // Implement some base class methods to do nothing to avoid asserts and
    // GTK warnings, since this is not a real wxDialog.
    virtual void DoSetSize(int WXUNUSED(x), int WXUNUSED(y),
                           int WXUNUSED(width), int WXUNUSED(height),
                           int WXUNUSED(sizeFlags) = wxSIZE_AUTO) wxOVERRIDE {}
    virtual void DoMoveWindow(int WXUNUSED(x), int WXUNUSED(y),
                              int WXUNUSED(width), int WXUNUSED(height)) wxOVERRIDE {}

private:
    wxPageSetupDialogData    m_pageDialogData;
    wxWindow                *m_parent;

    wxDECLARE_DYNAMIC_CLASS(wxGtkPageSetupDialog);
};

//----------------------------------------------------------------------------
// wxGtkPrinter
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGtkPrinter : public wxPrinterBase
{
public:
    wxGtkPrinter(wxPrintDialogData *data = NULL);
    virtual ~wxGtkPrinter();

    virtual bool Print(wxWindow *parent,
                       wxPrintout *printout,
                       bool prompt = true) wxOVERRIDE;
    virtual wxDC* PrintDialog(wxWindow *parent) wxOVERRIDE;
    virtual bool Setup(wxWindow *parent) wxOVERRIDE;

    GtkPrintContext *GetPrintContext() { return m_gpc; }
    void SetPrintContext(GtkPrintContext *context) {m_gpc = context;}
    void BeginPrint(wxPrintout *printout, GtkPrintOperation *operation, GtkPrintContext *context);
    void DrawPage(wxPrintout *printout, GtkPrintOperation *operation, GtkPrintContext *context, int page_nr);

private:
    GtkPrintContext *m_gpc;
    wxDC            *m_dc;

    wxDECLARE_DYNAMIC_CLASS(wxGtkPrinter);
    wxDECLARE_NO_COPY_CLASS(wxGtkPrinter);
};

//----------------------------------------------------------------------------
// wxGtkPrintNativeData
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGtkPrintNativeData : public wxPrintNativeDataBase
{
public:
    wxGtkPrintNativeData();
    virtual ~wxGtkPrintNativeData();

    virtual bool TransferTo( wxPrintData &data ) wxOVERRIDE;
    virtual bool TransferFrom( const wxPrintData &data ) wxOVERRIDE;

    virtual bool Ok() const wxOVERRIDE { return IsOk(); }
    virtual bool IsOk() const wxOVERRIDE { return true; }

    GtkPrintSettings* GetPrintConfig() { return m_config; }
    void SetPrintConfig( GtkPrintSettings * config );

    GtkPrintOperation* GetPrintJob() { return m_job; }
    void SetPrintJob(GtkPrintOperation *job) { m_job = job; }

    GtkPrintContext *GetPrintContext() { return m_context; }
    void SetPrintContext(GtkPrintContext *context) {m_context = context; }


    GtkPageSetup* GetPageSetupFromSettings(GtkPrintSettings* settings);
    void SetPageSetupToSettings(GtkPrintSettings* settings, GtkPageSetup* page_setup);

private:
    // NB: m_config is created and owned by us, but the other objects are not
    //     and their accessors don't change their ref count.
    GtkPrintSettings    *m_config;
    GtkPrintOperation   *m_job;
    GtkPrintContext     *m_context;

    wxDECLARE_DYNAMIC_CLASS(wxGtkPrintNativeData);
};

//-----------------------------------------------------------------------------
// wxGtkPrinterDC
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGtkPrinterDCImpl : public wxDCImpl
{
public:
    wxGtkPrinterDCImpl( wxPrinterDC *owner, const wxPrintData& data );
    virtual ~wxGtkPrinterDCImpl();

    bool Ok() const { return IsOk(); }
    bool IsOk() const wxOVERRIDE;

    virtual void* GetCairoContext() const wxOVERRIDE;
    virtual void* GetHandle() const wxOVERRIDE;
    
    bool CanDrawBitmap() const wxOVERRIDE { return true; }
    void Clear() wxOVERRIDE;
    void SetFont( const wxFont& font ) wxOVERRIDE;
    void SetPen( const wxPen& pen ) wxOVERRIDE;
    void SetBrush( const wxBrush& brush ) wxOVERRIDE;
    void SetLogicalFunction( wxRasterOperationMode function ) wxOVERRIDE;
    void SetBackground( const wxBrush& brush ) wxOVERRIDE;
    void DestroyClippingRegion() wxOVERRIDE;
    bool StartDoc(const wxString& message) wxOVERRIDE;
    void EndDoc() wxOVERRIDE;
    void StartPage() wxOVERRIDE;
    void EndPage() wxOVERRIDE;
    wxCoord GetCharHeight() const wxOVERRIDE;
    wxCoord GetCharWidth() const wxOVERRIDE;
    bool CanGetTextExtent() const wxOVERRIDE { return true; }
    wxSize GetPPI() const wxOVERRIDE;
    virtual int GetDepth() const wxOVERRIDE { return 24; }
    void SetBackgroundMode(int mode) wxOVERRIDE;
    void SetPalette(const wxPalette& WXUNUSED(palette)) wxOVERRIDE { }
    void SetResolution(int ppi);

    // overridden for wxPrinterDC Impl
    virtual int GetResolution() const wxOVERRIDE;
    virtual wxRect GetPaperRect() const wxOVERRIDE;

protected:
    bool DoFloodFill(wxCoord x1, wxCoord y1, const wxColour &col,
                     wxFloodFillStyle style=wxFLOOD_SURFACE ) wxOVERRIDE;
    void DoGradientFillConcentric(const wxRect& rect, const wxColour& initialColour, const wxColour& destColour, const wxPoint& circleCenter) wxOVERRIDE;
    void DoGradientFillLinear(const wxRect& rect, const wxColour& initialColour, const wxColour& destColour, wxDirection nDirection = wxEAST) wxOVERRIDE;
    bool DoGetPixel(wxCoord x1, wxCoord y1, wxColour *col) const wxOVERRIDE;
    void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2) wxOVERRIDE;
    void DoCrossHair(wxCoord x, wxCoord y) wxOVERRIDE;
    void DoDrawArc(wxCoord x1,wxCoord y1,wxCoord x2,wxCoord y2,wxCoord xc,wxCoord yc) wxOVERRIDE;
    void DoDrawEllipticArc(wxCoord x,wxCoord y,wxCoord w,wxCoord h,double sa,double ea) wxOVERRIDE;
    void DoDrawPoint(wxCoord x, wxCoord y) wxOVERRIDE;
    void DoDrawLines(int n, const wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0) wxOVERRIDE;
    void DoDrawPolygon(int n, const wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0, wxPolygonFillMode fillStyle=wxODDEVEN_RULE) wxOVERRIDE;
    void DoDrawPolyPolygon(int n, const int count[], const wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0, wxPolygonFillMode fillStyle=wxODDEVEN_RULE) wxOVERRIDE;
    void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height) wxOVERRIDE;
    void DoDrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius = 20.0) wxOVERRIDE;
    void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height) wxOVERRIDE;
#if wxUSE_SPLINES
    void DoDrawSpline(const wxPointList *points) wxOVERRIDE;
#endif
    bool DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
            wxDC *source, wxCoord xsrc, wxCoord ysrc,
            wxRasterOperationMode rop = wxCOPY, bool useMask = false,
            wxCoord xsrcMask = wxDefaultCoord, wxCoord ysrcMask = wxDefaultCoord) wxOVERRIDE;
    void DoDrawIcon( const wxIcon& icon, wxCoord x, wxCoord y ) wxOVERRIDE;
    void DoDrawBitmap( const wxBitmap& bitmap, wxCoord x, wxCoord y, bool useMask = false  ) wxOVERRIDE;
    void DoDrawText(const wxString& text, wxCoord x, wxCoord y ) wxOVERRIDE;
    void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y, double angle) wxOVERRIDE;
    void DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height) wxOVERRIDE;
    void DoSetDeviceClippingRegion( const wxRegion &WXUNUSED(clip) ) wxOVERRIDE
    {
        wxFAIL_MSG( "not implemented" );
    }
    void DoGetTextExtent(const wxString& string, wxCoord *x, wxCoord *y,
                     wxCoord *descent = NULL,
                     wxCoord *externalLeading = NULL,
                     const wxFont *theFont = NULL ) const wxOVERRIDE;
    void DoGetSize(int* width, int* height) const wxOVERRIDE;
    void DoGetSizeMM(int *width, int *height) const wxOVERRIDE;

    wxPrintData& GetPrintData() { return m_printData; }
    void SetPrintData(const wxPrintData& data);

private:
    wxPrintData             m_printData;
    PangoContext           *m_context;
    PangoLayout            *m_layout;
    PangoFontDescription   *m_fontdesc;
    cairo_t                *m_cairo;

    unsigned char           m_currentRed;
    unsigned char           m_currentGreen;
    unsigned char           m_currentBlue;
    unsigned char           m_currentAlpha;

    GtkPrintContext        *m_gpc;
    int                     m_resolution;
    double                  m_PS2DEV;
    double                  m_DEV2PS;

    wxDECLARE_DYNAMIC_CLASS(wxGtkPrinterDCImpl);
    wxDECLARE_NO_COPY_CLASS(wxGtkPrinterDCImpl);
};

// ----------------------------------------------------------------------------
// wxGtkPrintPreview: programmer creates an object of this class to preview a
// wxPrintout.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGtkPrintPreview : public wxPrintPreviewBase
{
public:
    wxGtkPrintPreview(wxPrintout *printout,
                             wxPrintout *printoutForPrinting = NULL,
                             wxPrintDialogData *data = NULL);
    wxGtkPrintPreview(wxPrintout *printout,
                             wxPrintout *printoutForPrinting,
                             wxPrintData *data);

    virtual ~wxGtkPrintPreview();

    virtual bool Print(bool interactive) wxOVERRIDE;
    virtual void DetermineScaling() wxOVERRIDE;

private:
    void Init(wxPrintout *printout,
              wxPrintout *printoutForPrinting,
              wxPrintData *data);

    // resolution to use in DPI
    int m_resolution;

    wxDECLARE_CLASS(wxGtkPrintPreview);
};

#endif // wxUSE_GTKPRINT

#endif // _WX_GTK_PRINT_H_
