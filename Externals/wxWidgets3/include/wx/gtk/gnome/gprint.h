/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/gnome/gprint.h
// Author:      Robert Roebling
// Purpose:     GNOME printing support
// Created:     09/20/04
// RCS-ID:      $Id: gprint.h 67280 2011-03-22 14:17:38Z DS $
// Copyright:   Robert Roebling
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_GPRINT_H_
#define _WX_GTK_GPRINT_H_

#include "wx/defs.h"

#if wxUSE_LIBGNOMEPRINT

#include "wx/print.h"
#include "wx/printdlg.h"
#include "wx/dc.h"
#include "wx/module.h"

typedef struct _GnomePrintJob GnomePrintJob;
typedef struct _GnomePrintContext GnomePrintContext;
typedef struct _GnomePrintConfig GnomePrintConfig;

// ----------------------------------------------------------------------------
// wxGnomePrintModule
// ----------------------------------------------------------------------------

class wxGnomePrintModule: public wxModule
{
public:
    wxGnomePrintModule() {}
    bool OnInit();
    void OnExit();

private:
    DECLARE_DYNAMIC_CLASS(wxGnomePrintModule)
};

//----------------------------------------------------------------------------
// wxGnomePrintNativeData
//----------------------------------------------------------------------------

class wxGnomePrintNativeData: public wxPrintNativeDataBase
{
public:
    wxGnomePrintNativeData();
    virtual ~wxGnomePrintNativeData();

    virtual bool TransferTo( wxPrintData &data );
    virtual bool TransferFrom( const wxPrintData &data );

    virtual bool Ok() const { return IsOk(); }
    virtual bool IsOk() const { return true; }

    GnomePrintConfig* GetPrintConfig() { return m_config; }
    void SetPrintJob( GnomePrintJob *job ) { m_job = job; }
    GnomePrintJob* GetPrintJob() { return m_job; }


private:
    GnomePrintConfig  *m_config;
    GnomePrintJob     *m_job;

    DECLARE_DYNAMIC_CLASS(wxGnomePrintNativeData)
};

//----------------------------------------------------------------------------
// wxGnomePrintFactory
//----------------------------------------------------------------------------

class wxGnomePrintFactory: public wxPrintFactory
{
public:
    virtual wxPrinterBase *CreatePrinter( wxPrintDialogData *data );

    virtual wxPrintPreviewBase *CreatePrintPreview( wxPrintout *preview,
                                                    wxPrintout *printout = NULL,
                                                    wxPrintDialogData *data = NULL );
    virtual wxPrintPreviewBase *CreatePrintPreview( wxPrintout *preview,
                                                    wxPrintout *printout,
                                                    wxPrintData *data );

    virtual wxPrintDialogBase *CreatePrintDialog( wxWindow *parent,
                                                  wxPrintDialogData *data = NULL );
    virtual wxPrintDialogBase *CreatePrintDialog( wxWindow *parent,
                                                  wxPrintData *data );

    virtual wxPageSetupDialogBase *CreatePageSetupDialog( wxWindow *parent,
                                                          wxPageSetupDialogData * data = NULL );

#if wxUSE_NEW_DC
    virtual wxDCImpl* CreatePrinterDCImpl( wxPrinterDC *owner, const wxPrintData& data );
#else
    virtual wxDC* CreatePrinterDC( const wxPrintData& data );
#endif

    virtual bool HasPrintSetupDialog();
    virtual wxDialog *CreatePrintSetupDialog( wxWindow *parent, wxPrintData *data );
    virtual bool HasOwnPrintToFile();
    virtual bool HasPrinterLine();
    virtual wxString CreatePrinterLine();
    virtual bool HasStatusLine();
    virtual wxString CreateStatusLine();

    virtual wxPrintNativeDataBase *CreatePrintNativeData();
};

//----------------------------------------------------------------------------
// wxGnomePrintDialog
//----------------------------------------------------------------------------

class wxGnomePrintDialog: public wxPrintDialogBase
{
public:
    wxGnomePrintDialog( wxWindow *parent,
                         wxPrintDialogData* data = NULL );
    wxGnomePrintDialog( wxWindow *parent, wxPrintData* data);
    virtual ~wxGnomePrintDialog();

    wxPrintData& GetPrintData()
        { return m_printDialogData.GetPrintData(); }
    wxPrintDialogData& GetPrintDialogData()
        { return m_printDialogData; }

    wxDC *GetPrintDC();

    virtual int ShowModal();

    virtual bool Validate();
    virtual bool TransferDataToWindow();
    virtual bool TransferDataFromWindow();

protected:
    // Implement some base class methods to do nothing to avoid asserts and
    // GTK warnings, since this is not a real wxDialog.
    virtual void DoSetSize(int WXUNUSED(x), int WXUNUSED(y),
                           int WXUNUSED(width), int WXUNUSED(height),
                           int WXUNUSED(sizeFlags) = wxSIZE_AUTO) {}
    virtual void DoMoveWindow(int WXUNUSED(x), int WXUNUSED(y),
                              int WXUNUSED(width), int WXUNUSED(height)) {}

private:
    void Init();
    wxPrintDialogData   m_printDialogData;

    DECLARE_DYNAMIC_CLASS(wxGnomePrintDialog)
};

//----------------------------------------------------------------------------
// wxGnomePageSetupDialog
//----------------------------------------------------------------------------

class wxGnomePageSetupDialog: public wxPageSetupDialogBase
{
public:
    wxGnomePageSetupDialog( wxWindow *parent,
                            wxPageSetupDialogData* data = NULL );
    virtual ~wxGnomePageSetupDialog();

    virtual wxPageSetupDialogData& GetPageSetupDialogData();

    virtual int ShowModal();

    virtual bool Validate();
    virtual bool TransferDataToWindow();
    virtual bool TransferDataFromWindow();

protected:
    // Implement some base class methods to do nothing to avoid asserts and
    // GTK warnings, since this is not a real wxDialog.
    virtual void DoSetSize(int WXUNUSED(x), int WXUNUSED(y),
                           int WXUNUSED(width), int WXUNUSED(height),
                           int WXUNUSED(sizeFlags) = wxSIZE_AUTO) {}
    virtual void DoMoveWindow(int WXUNUSED(x), int WXUNUSED(y),
                              int WXUNUSED(width), int WXUNUSED(height)) {}

private:
    wxPageSetupDialogData   m_pageDialogData;

    DECLARE_DYNAMIC_CLASS(wxGnomePageSetupDialog)
};

//----------------------------------------------------------------------------
// wxGnomePrinter
//----------------------------------------------------------------------------

class wxGnomePrinter: public wxPrinterBase
{
public:
    wxGnomePrinter(wxPrintDialogData *data = NULL);
    virtual ~wxGnomePrinter();

    virtual bool Print(wxWindow *parent,
                       wxPrintout *printout,
                       bool prompt = true);
    virtual wxDC* PrintDialog(wxWindow *parent);
    virtual bool Setup(wxWindow *parent);

private:
    bool               m_native_preview;

private:
    DECLARE_DYNAMIC_CLASS(wxGnomePrinter)
    wxDECLARE_NO_COPY_CLASS(wxGnomePrinter);
};

//-----------------------------------------------------------------------------
// wxGnomePrinterDC
//-----------------------------------------------------------------------------

#if wxUSE_NEW_DC
class wxGnomePrinterDCImpl : public wxDCImpl
#else
#define wxGnomePrinterDCImpl wxGnomePrinterDC
class wxGnomePrinterDC : public wxDC
#endif
{
public:
#if wxUSE_NEW_DC
    wxGnomePrinterDCImpl( wxPrinterDC *owner, const wxPrintData& data );
#else
    wxGnomePrinterDC( const wxPrintData& data );
#endif
    virtual ~wxGnomePrinterDCImpl();

    bool Ok() const { return IsOk(); }
    bool IsOk() const;

    bool CanDrawBitmap() const { return true; }
    void Clear();
    void SetFont( const wxFont& font );
    void SetPen( const wxPen& pen );
    void SetBrush( const wxBrush& brush );
    void SetLogicalFunction( wxRasterOperationMode function );
    void SetBackground( const wxBrush& brush );
    void DestroyClippingRegion();
    bool StartDoc(const wxString& message);
    void EndDoc();
    void StartPage();
    void EndPage();
    wxCoord GetCharHeight() const;
    wxCoord GetCharWidth() const;
    bool CanGetTextExtent() const { return true; }
    wxSize GetPPI() const;
    virtual int GetDepth() const { return 24; }
    void SetBackgroundMode(int WXUNUSED(mode)) { }
    void SetPalette(const wxPalette& WXUNUSED(palette)) { }

protected:
    bool DoFloodFill(wxCoord x1, wxCoord y1, const wxColour &col,
                    wxFloodFillStyle style=wxFLOOD_SURFACE );
    bool DoGetPixel(wxCoord x1, wxCoord y1, wxColour *col) const;
    void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2);
    void DoCrossHair(wxCoord x, wxCoord y);
    void DoDrawArc(wxCoord x1,wxCoord y1,wxCoord x2,wxCoord y2,wxCoord xc,wxCoord yc);
    void DoDrawEllipticArc(wxCoord x,wxCoord y,wxCoord w,wxCoord h,double sa,double ea);
    void DoDrawPoint(wxCoord x, wxCoord y);
    void DoDrawLines(int n, wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0);
    void DoDrawPolygon(int n, wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0, wxPolygonFillMode fillStyle=wxODDEVEN_RULE);
    void DoDrawPolyPolygon(int n, int count[], wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0, wxPolygonFillMode fillStyle=wxODDEVEN_RULE);
    void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
    void DoDrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius = 20.0);
    void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
#if wxUSE_SPLINES
    void DoDrawSpline(const wxPointList *points);
#endif
    bool DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
            wxDC *source, wxCoord xsrc, wxCoord ysrc,
            wxRasterOperationMode = wxCOPY, bool useMask = false,
            wxCoord xsrcMask = wxDefaultCoord, wxCoord ysrcMask = wxDefaultCoord);
    void DoDrawIcon( const wxIcon& icon, wxCoord x, wxCoord y );
    void DoDrawBitmap( const wxBitmap& bitmap, wxCoord x, wxCoord y, bool useMask = false  );
    void DoDrawText(const wxString& text, wxCoord x, wxCoord y );
    void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y, double angle);
    void DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
    void DoSetDeviceClippingRegion( const wxRegion &WXUNUSED(clip) )
    {
        wxFAIL_MSG( "not implemented" );
    }
    void DoGetTextExtent(const wxString& string, wxCoord *x, wxCoord *y,
                     wxCoord *descent = NULL,
                     wxCoord *externalLeading = NULL,
                     const wxFont *theFont = NULL ) const;
    void DoGetSize(int* width, int* height) const;
    void DoGetSizeMM(int *width, int *height) const;

    void SetPrintData(const wxPrintData& data);
    wxPrintData& GetPrintData() { return m_printData; }

    // overridden for wxPrinterDC Impl
    virtual wxRect GetPaperRect() const;
    virtual int GetResolution() const;

private:
    wxPrintData             m_printData;
    PangoContext           *m_context;
    PangoLayout            *m_layout;
    PangoFontDescription   *m_fontdesc;

    unsigned char           m_currentRed;
    unsigned char           m_currentGreen;
    unsigned char           m_currentBlue;

    double                  m_pageHeight;

    GnomePrintContext      *m_gpc;
    GnomePrintJob*          m_job;

    void makeEllipticalPath(wxCoord x, wxCoord y, wxCoord width, wxCoord height);

private:
    DECLARE_DYNAMIC_CLASS(wxGnomePrinterDCImpl)
    wxDECLARE_NO_COPY_CLASS(wxGnomePrinterDCImpl);
};

// ----------------------------------------------------------------------------
// wxGnomePrintPreview: programmer creates an object of this class to preview a
// wxPrintout.
// ----------------------------------------------------------------------------

class wxGnomePrintPreview : public wxPrintPreviewBase
{
public:
    wxGnomePrintPreview(wxPrintout *printout,
                             wxPrintout *printoutForPrinting = NULL,
                             wxPrintDialogData *data = NULL);
    wxGnomePrintPreview(wxPrintout *printout,
                             wxPrintout *printoutForPrinting,
                             wxPrintData *data);

    virtual ~wxGnomePrintPreview();

    virtual bool Print(bool interactive);
    virtual void DetermineScaling();

private:
    void Init(wxPrintout *printout, wxPrintout *printoutForPrinting);

private:
    DECLARE_CLASS(wxGnomePrintPreview)
};


#endif
    // wxUSE_LIBGNOMEPRINT

#endif
