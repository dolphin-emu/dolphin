/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/printmac.cpp
// Purpose:     wxMacPrinter framework
// Author:      Julian Smart, Stefan Csomor
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart, Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_PRINTING_ARCHITECTURE

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/dc.h"
    #include "wx/app.h"
    #include "wx/msgdlg.h"
    #include "wx/dcprint.h"
    #include "wx/math.h"
#endif

#include "wx/osx/private.h"

#include "wx/osx/printmac.h"
#include "wx/osx/private/print.h"

#include "wx/printdlg.h"
#include "wx/paper.h"
#include "wx/osx/printdlg.h"

#include <stdlib.h>

//
// move to print_osx.cpp
//

static int ResolutionSorter(const void *e1, const void *e2)
{
    const PMResolution *res1 = (const PMResolution *)e1;
    const PMResolution *res2 = (const PMResolution *)e2;
    const double area1 = res1->hRes * res1->vRes;
    const double area2 = res2->hRes * res2->vRes;

    if (area1 < area2)
        return -1;
    else if (area1 > area2)
        return 1;
    else
        return 0;
}

static PMResolution *GetSupportedResolutions(PMPrinter printer, UInt32 *count)
{
    PMResolution res, *resolutions = NULL;
    OSStatus status = PMPrinterGetPrinterResolutionCount(printer, count);
    if (status == noErr)
    {
        resolutions = (PMResolution *)malloc(sizeof(PMResolution) * (*count));
        UInt32 realCount = 0;
        for (UInt32 i = 0; i < *count; i++)
        {
            if (PMPrinterGetIndexedPrinterResolution(printer, i + 1, &res) == noErr)
                resolutions[realCount++] = res;
        }
        qsort(resolutions, realCount, sizeof(PMResolution), ResolutionSorter);

        *count = realCount;
    }
    if ((*count == 0) && (resolutions))
    {
        free(resolutions);
        resolutions = NULL;
    }
    return resolutions;
}



IMPLEMENT_DYNAMIC_CLASS(wxOSXPrintData, wxPrintNativeDataBase)

bool wxOSXPrintData::IsOk() const
{
    return (m_macPageFormat != kPMNoPageFormat) && (m_macPrintSettings != kPMNoPrintSettings) && (m_macPrintSession != kPMNoReference);
}

wxOSXPrintData::wxOSXPrintData()
{
    m_macPageFormat = kPMNoPageFormat;
    m_macPrintSettings = kPMNoPrintSettings;
    m_macPrintSession = kPMNoReference ;
    m_macPaper = kPMNoData;
}

wxOSXPrintData::~wxOSXPrintData()
{
}

void wxOSXPrintData::UpdateFromPMState()
{
}

void wxOSXPrintData::UpdateToPMState()
{
}

void wxOSXPrintData::TransferPrinterNameFrom( const wxPrintData &data )
{
    CFArrayRef printerList;
    CFIndex index, count;
    CFStringRef name;

    if (PMServerCreatePrinterList(kPMServerLocal, &printerList) == noErr)
    {
        PMPrinter printer = NULL;
        count = CFArrayGetCount(printerList);
        for (index = 0; index < count; index++)
        {
            printer = (PMPrinter)CFArrayGetValueAtIndex(printerList, index);
            if ((data.GetPrinterName().empty()) && (PMPrinterIsDefault(printer)))
                break;
            else
            {
                name = PMPrinterGetName(printer);
                CFRetain(name);
                if (data.GetPrinterName() == wxCFStringRef(name).AsString())
                    break;
            }
        }
        if (index < count)
            PMSessionSetCurrentPMPrinter(m_macPrintSession, printer);
        CFRelease(printerList);
    }
}

void wxOSXPrintData::TransferPaperInfoFrom( const wxPrintData &data )
{
    PMPrinter printer;
    PMSessionGetCurrentPrinter(m_macPrintSession, &printer);

    wxSize papersize = wxDefaultSize;
    const wxPaperSize paperId = data.GetPaperId();
    if ( paperId != wxPAPER_NONE && wxThePrintPaperDatabase )
    {
        papersize = wxThePrintPaperDatabase->GetSize(paperId);
        if ( papersize != wxDefaultSize )
        {
            papersize.x /= 10;
            papersize.y /= 10;
        }
    }
    else
    {
        papersize = data.GetPaperSize();
    }

    if ( papersize != wxDefaultSize )
    {
        papersize.x = (wxInt32) (papersize.x * mm2pt);
        papersize.y = (wxInt32) (papersize.y * mm2pt);

        double height, width;
        PMPaperGetHeight(m_macPaper, &height);
        PMPaperGetWidth(m_macPaper, &width);

        if ( fabs( width - papersize.x ) >= 5 ||
            fabs( height - papersize.y ) >= 5 )
        {
            // we have to change the current paper
            CFArrayRef paperlist = 0 ;
            if ( PMPrinterGetPaperList( printer, &paperlist ) == noErr )
            {
                PMPaper bestPaper = kPMNoData ;
                CFIndex top = CFArrayGetCount(paperlist);
                for ( CFIndex i = 0 ; i < top ; ++ i )
                {
                    PMPaper paper = (PMPaper) CFArrayGetValueAtIndex( paperlist, i );
                    PMPaperGetHeight(paper, &height);
                    PMPaperGetWidth(paper, &width);
                    if ( fabs( width - papersize.x ) < 5 &&
                        fabs( height - papersize.y ) < 5 )
                    {
                        // TODO test for duplicate hits and use additional
                        // criteria for best match
                        bestPaper = paper;
                    }
                }
                PMPaper paper = kPMNoData;
                if ( bestPaper == kPMNoData )
                {
                    const PMPaperMargins margins = { 0.0, 0.0, 0.0, 0.0 };
                    wxString id, name(wxT("Custom paper"));
                    id.Printf(wxT("wxPaperCustom%dx%d"), papersize.x, papersize.y);

                    PMPaperCreateCustom(printer, wxCFStringRef( id, wxFont::GetDefaultEncoding() ), wxCFStringRef( name, wxFont::GetDefaultEncoding() ),
                                            papersize.x, papersize.y, &margins, &paper);
                }
                if ( bestPaper != kPMNoData )
                {
                    PMPageFormat pageFormat;
                    PMCreatePageFormatWithPMPaper(&pageFormat, bestPaper);
                    PMCopyPageFormat( pageFormat, m_macPageFormat );
                    PMRelease(pageFormat);
                    PMGetPageFormatPaper(m_macPageFormat, &m_macPaper);
                }
                PMRelease(paper);
            }
        }
    }

    PMSetCopies( m_macPrintSettings , data.GetNoCopies() , false ) ;
    PMSetCollate(m_macPrintSettings, data.GetCollate());
    if ( data.IsOrientationReversed() )
        PMSetOrientation( m_macPageFormat , ( data.GetOrientation() == wxLANDSCAPE ) ?
                         kPMReverseLandscape : kPMReversePortrait , false ) ;
    else
        PMSetOrientation( m_macPageFormat , ( data.GetOrientation() == wxLANDSCAPE ) ?
                         kPMLandscape : kPMPortrait , false ) ;

    PMDuplexMode mode = 0 ;
    switch( data.GetDuplex() )
    {
        case wxDUPLEX_HORIZONTAL :
            mode = kPMDuplexNoTumble ;
            break ;
        case wxDUPLEX_VERTICAL :
            mode = kPMDuplexTumble ;
            break ;
        case wxDUPLEX_SIMPLEX :
        default :
            mode = kPMDuplexNone ;
            break ;
    }
    PMSetDuplex(  m_macPrintSettings, mode ) ;


    if ( data.IsOrientationReversed() )
        PMSetOrientation(  m_macPageFormat , ( data.GetOrientation() == wxLANDSCAPE ) ?
                         kPMReverseLandscape : kPMReversePortrait , false ) ;
    else
        PMSetOrientation(  m_macPageFormat , ( data.GetOrientation() == wxLANDSCAPE ) ?
                         kPMLandscape : kPMPortrait , false ) ;
}

void wxOSXPrintData::TransferResolutionFrom( const wxPrintData &data )
{
    PMPrinter printer;
    PMSessionGetCurrentPrinter(m_macPrintSession, &printer);

    UInt32 resCount;
    PMResolution *resolutions = GetSupportedResolutions(printer, &resCount);
    if (resolutions)
    {
        wxPrintQuality quality = data.GetQuality();
        if (quality >= 0)
            quality = wxPRINT_QUALITY_HIGH;

        PMResolution res = resolutions[((quality - wxPRINT_QUALITY_DRAFT) * (resCount - 1)) / 3];
        PMPrinterSetOutputResolution(printer, m_macPrintSettings, &res);

        free(resolutions);
    }
}

bool wxOSXPrintData::TransferFrom( const wxPrintData &data )
{
    TransferPrinterNameFrom(data);
    TransferPaperInfoFrom(data);
    TransferResolutionFrom(data);

    // after setting the new resolution the format has to be updated, otherwise the page rect remains
    // at the 'old' scaling

    PMSessionValidatePageFormat(m_macPrintSession,
                                m_macPageFormat, kPMDontWantBoolean);
    PMSessionValidatePrintSettings(m_macPrintSession,
                                   m_macPrintSettings, kPMDontWantBoolean);

#if wxOSX_USE_COCOA
    UpdateFromPMState();
#endif

    return true ;
}

void wxOSXPrintData::TransferPrinterNameTo( wxPrintData &data )
{
    CFStringRef name;
    PMPrinter printer ;
    PMSessionGetCurrentPrinter( m_macPrintSession, &printer );
    if (PMPrinterIsDefault(printer))
        data.SetPrinterName(wxEmptyString);
    else
    {
        name = PMPrinterGetName(printer);
        CFRetain(name);
        data.SetPrinterName(wxCFStringRef(name).AsString());
    }
}

void wxOSXPrintData::TransferPaperInfoTo( wxPrintData &data )
{
    PMGetPageFormatPaper(m_macPageFormat, &m_macPaper);

    PMPrinter printer ;
    PMSessionGetCurrentPrinter( m_macPrintSession, &printer );
    OSStatus err = noErr ;
    UInt32 copies ;
    err = PMGetCopies( m_macPrintSettings , &copies ) ;
    if ( err == noErr )
        data.SetNoCopies( copies ) ;

    PMOrientation orientation ;
    err = PMGetOrientation(  m_macPageFormat , &orientation ) ;
    if ( err == noErr )
    {
        if ( orientation == kPMPortrait || orientation == kPMReversePortrait )
        {
            data.SetOrientation( wxPORTRAIT  );
            data.SetOrientationReversed( orientation == kPMReversePortrait );
        }
        else
        {
            data.SetOrientation( wxLANDSCAPE );
            data.SetOrientationReversed( orientation == kPMReverseLandscape );
        }
    }

    Boolean collate;
    if (PMGetCollate(m_macPrintSettings, &collate) == noErr)
        data.SetCollate(collate);


    PMDuplexMode mode = 0 ;
    PMGetDuplex(  m_macPrintSettings, &mode ) ;
    switch( mode )
    {
        case kPMDuplexNoTumble :
            data.SetDuplex(wxDUPLEX_HORIZONTAL);
            break ;
        case kPMDuplexTumble :
            data.SetDuplex(wxDUPLEX_VERTICAL);
            break ;
        case kPMDuplexNone :
        default :
            data.SetDuplex(wxDUPLEX_SIMPLEX);
            break ;
    }

    double height, width;
    PMPaperGetHeight(m_macPaper, &height);
    PMPaperGetWidth(m_macPaper, &width);

    wxSize sz((int)(width * pt2mm + 0.5 ) ,
              (int)(height * pt2mm + 0.5 ));
    data.SetPaperSize(sz);
    wxPaperSize id = wxThePrintPaperDatabase->GetSize(wxSize(sz.x* 10, sz.y * 10));
    if (id != wxPAPER_NONE)
    {
        data.SetPaperId(id);
    }
}

void wxOSXPrintData::TransferResolutionTo( wxPrintData &data )
{
    PMPrinter printer ;
    PMSessionGetCurrentPrinter( m_macPrintSession, &printer );

    /* assume high quality, will change below if we are able to */
    data.SetQuality(wxPRINT_QUALITY_HIGH);

    PMResolution *resolutions;
    UInt32 resCount;
    resolutions = GetSupportedResolutions(printer, &resCount);
    if (resolutions)
    {
        bool valid = false;
        PMResolution res;
        if ( PMPrinterGetOutputResolution(printer, m_macPrintSettings, &res) == noErr )
            valid = true;

        if ( valid )
        {
            UInt32 i;
            for (i = 0; i < resCount; i++)
            {
                if ((resolutions[i].hRes == res.hRes) && (resolutions[i].vRes = res.vRes))
                    break;
            }
            if (i < resCount)
                data.SetQuality((((i + 1) * 3) / resCount) + wxPRINT_QUALITY_DRAFT);
        }
        free(resolutions);
    }
}

bool wxOSXPrintData::TransferTo( wxPrintData &data )
{
#if wxOSX_USE_COCOA
    UpdateToPMState();
#endif

    TransferPrinterNameTo(data);
    TransferPaperInfoTo(data);
    TransferResolutionTo(data);
    return true ;
}

void wxOSXPrintData::TransferFrom( wxPageSetupDialogData *WXUNUSED(data) )
{
    // should we setup the page rect here ?
    // since MacOS sometimes has two same paper rects with different
    // page rects we could make it roundtrip safe perhaps
}

void wxOSXPrintData::TransferTo( wxPageSetupDialogData* data )
{
#if wxOSX_USE_COCOA
    UpdateToPMState();
#endif
    PMRect rPaper;
    OSStatus err = PMGetUnadjustedPaperRect(m_macPageFormat, &rPaper);
    if ( err == noErr )
    {
        wxSize sz((int)(( rPaper.right - rPaper.left ) * pt2mm + 0.5 ) ,
             (int)(( rPaper.bottom - rPaper.top ) * pt2mm + 0.5 ));
        data->SetPaperSize(sz);

        PMRect rPage ;
        err = PMGetUnadjustedPageRect(m_macPageFormat , &rPage ) ;
        if ( err == noErr )
        {
            data->SetMinMarginTopLeft( wxPoint (
                (int)(((double) rPage.left - rPaper.left ) * pt2mm) ,
                (int)(((double) rPage.top - rPaper.top ) * pt2mm) ) ) ;

            data->SetMinMarginBottomRight( wxPoint (
                (wxCoord)(((double) rPaper.right - rPage.right ) * pt2mm),
                (wxCoord)(((double) rPaper.bottom - rPage.bottom ) * pt2mm)) ) ;

            if ( data->GetMarginTopLeft().x < data->GetMinMarginTopLeft().x )
                data->SetMarginTopLeft( wxPoint( data->GetMinMarginTopLeft().x ,
                    data->GetMarginTopLeft().y ) ) ;

            if ( data->GetMarginBottomRight().x < data->GetMinMarginBottomRight().x )
                data->SetMarginBottomRight( wxPoint( data->GetMinMarginBottomRight().x ,
                    data->GetMarginBottomRight().y ) );

            if ( data->GetMarginTopLeft().y < data->GetMinMarginTopLeft().y )
                data->SetMarginTopLeft( wxPoint( data->GetMarginTopLeft().x , data->GetMinMarginTopLeft().y ) );

            if ( data->GetMarginBottomRight().y < data->GetMinMarginBottomRight().y )
                data->SetMarginBottomRight( wxPoint( data->GetMarginBottomRight().x ,
                    data->GetMinMarginBottomRight().y) );
        }
    }
}

void wxOSXPrintData::TransferTo( wxPrintDialogData* data )
{
#if wxOSX_USE_COCOA
    UpdateToPMState();
#endif
    UInt32 minPage , maxPage ;
    PMGetPageRange( m_macPrintSettings , &minPage , &maxPage ) ;
    data->SetMinPage( minPage ) ;
    data->SetMaxPage( maxPage ) ;
    UInt32 copies ;
    PMGetCopies( m_macPrintSettings , &copies ) ;
    data->SetNoCopies( copies ) ;
    UInt32 from , to ;
    PMGetFirstPage( m_macPrintSettings , &from ) ;
    PMGetLastPage( m_macPrintSettings , &to ) ;
    if ( to >= 0x7FFFFFFF ) //  due to an OS Bug we don't get back kPMPrintAllPages
    {
        data->SetAllPages( true ) ;
        // This means all pages, more or less
        data->SetFromPage(1);
        data->SetToPage(9999);
    }
    else
    {
        data->SetFromPage( from ) ;
        data->SetToPage( to ) ;
        data->SetAllPages( false );
    }
}

void wxOSXPrintData::TransferFrom( wxPrintDialogData* data )
{
    // Respect the value of m_printAllPages
    if ( data->GetAllPages() )
        PMSetPageRange( m_macPrintSettings , data->GetMinPage() , (UInt32) kPMPrintAllPages ) ;
    else
        PMSetPageRange( m_macPrintSettings , data->GetMinPage() , data->GetMaxPage() ) ;
    PMSetCopies( m_macPrintSettings , data->GetNoCopies() , false ) ;
    PMSetFirstPage( m_macPrintSettings , data->GetFromPage() , false ) ;

    if (data->GetAllPages() || data->GetFromPage() == 0)
        PMSetLastPage( m_macPrintSettings , (UInt32) kPMPrintAllPages, true ) ;
    else
        PMSetLastPage( m_macPrintSettings , (UInt32) data->GetToPage() , false ) ;
#if wxOSX_USE_COCOA
    UpdateFromPMState();
#endif
}

wxPrintNativeDataBase* wxOSXCreatePrintData()
{
#if wxOSX_USE_COCOA
    return new wxOSXCocoaPrintData();
#elif wxOSX_USE_CARBON
    return new wxOSXCarbonPrintData();
#else
    return NULL;
#endif
}

/*
* Printer
*/

IMPLEMENT_DYNAMIC_CLASS(wxMacPrinter, wxPrinterBase)

wxMacPrinter::wxMacPrinter(wxPrintDialogData *data):
wxPrinterBase(data)
{
}

wxMacPrinter::~wxMacPrinter(void)
{
}

bool wxMacPrinter::Print(wxWindow *parent, wxPrintout *printout, bool prompt)
{
    sm_abortIt = false;
    sm_abortWindow = NULL;

    if (!printout)
    {
        sm_lastError = wxPRINTER_ERROR;
        return false;
    }

    if (m_printDialogData.GetMinPage() < 1)
        m_printDialogData.SetMinPage(1);
    if (m_printDialogData.GetMaxPage() < 1)
        m_printDialogData.SetMaxPage(9999);

    // Create a suitable device context
    wxPrinterDC *dc = NULL;
    if (prompt)
    {
        wxMacPrintDialog dialog(parent, & m_printDialogData);
        if (dialog.ShowModal() == wxID_OK)
        {
            dc = wxDynamicCast(dialog.GetPrintDC(), wxPrinterDC);
            wxASSERT(dc);
            m_printDialogData = dialog.GetPrintDialogData();
        }
    }
    else
    {
        dc = new wxPrinterDC( m_printDialogData.GetPrintData() ) ;
    }

    // May have pressed cancel.
    if (!dc || !dc->IsOk())
    {
        delete dc;
        return false;
    }

    // on the mac we have always pixels as addressing mode with 72 dpi
    printout->SetPPIScreen(72, 72);

    PMResolution res;
    PMPrinter printer;
    wxOSXPrintData* nativeData = (wxOSXPrintData*)
          (m_printDialogData.GetPrintData().GetNativeData());

    if (PMSessionGetCurrentPrinter(nativeData->GetPrintSession(), &printer) == noErr)
    {
        if (PMPrinterGetOutputResolution( printer, nativeData->GetPrintSettings(), &res) == -9589 /* kPMKeyNotFound */ )
        {
            res.hRes = res.vRes = 300;
        }
    }
    else
    {
        // fallback
        res.hRes = res.vRes = 300;
    }
    printout->SetPPIPrinter(int(res.hRes), int(res.vRes));

    // Set printout parameters
    printout->SetDC(dc);

    int w, h;
    dc->GetSize(&w, &h);
    printout->SetPageSizePixels((int)w, (int)h);
    printout->SetPaperRectPixels(dc->GetPaperRect());
    wxCoord mw, mh;
    dc->GetSizeMM(&mw, &mh);
    printout->SetPageSizeMM((int)mw, (int)mh);

    // Create an abort window
    wxBeginBusyCursor();

    printout->OnPreparePrinting();

    // Get some parameters from the printout, if defined
    int fromPage, toPage;
    int minPage, maxPage;
    printout->GetPageInfo(&minPage, &maxPage, &fromPage, &toPage);

    if (maxPage == 0)
    {
        sm_lastError = wxPRINTER_ERROR;
        return false;
    }

    // Only set min and max, because from and to will be
    // set by the user
    m_printDialogData.SetMinPage(minPage);
    m_printDialogData.SetMaxPage(maxPage);

    printout->OnBeginPrinting();

    bool keepGoing = true;

    if (!printout->OnBeginDocument(m_printDialogData.GetFromPage(), m_printDialogData.GetToPage()))
    {
            wxEndBusyCursor();
            wxMessageBox(wxT("Could not start printing."), wxT("Print Error"), wxOK, parent);
    }

    int pn;
    for (pn = m_printDialogData.GetFromPage();
        keepGoing && (pn <= m_printDialogData.GetToPage()) && printout->HasPage(pn);
        pn++)
    {
        if (sm_abortIt)
        {
                break;
        }
        else
        {
                dc->StartPage();
                keepGoing = printout->OnPrintPage(pn);
                dc->EndPage();
        }
    }
    printout->OnEndDocument();

    printout->OnEndPrinting();

    if (sm_abortWindow)
    {
        sm_abortWindow->Show(false);
        wxDELETE(sm_abortWindow);
    }

    wxEndBusyCursor();

    delete dc;

    return true;
}

wxDC* wxMacPrinter::PrintDialog(wxWindow *parent)
{
    wxDC* dc = NULL;

    wxPrintDialog dialog(parent, & m_printDialogData);
    int ret = dialog.ShowModal();

    if (ret == wxID_OK)
    {
        dc = dialog.GetPrintDC();
        m_printDialogData = dialog.GetPrintDialogData();
    }

    return dc;
}

bool wxMacPrinter::Setup(wxWindow *WXUNUSED(parent))
{
#if 0
    wxPrintDialog dialog(parent, & m_printDialogData);
    dialog.GetPrintDialogData().SetSetupDialog(true);

    int ret = dialog.ShowModal();

    if (ret == wxID_OK)
        m_printDialogData = dialog.GetPrintDialogData();

    return (ret == wxID_OK);
#endif

    return false;
}

/*
* Print preview
*/

IMPLEMENT_CLASS(wxMacPrintPreview, wxPrintPreviewBase)

wxMacPrintPreview::wxMacPrintPreview(wxPrintout *printout,
                                     wxPrintout *printoutForPrinting,
                                     wxPrintDialogData *data)
                                     : wxPrintPreviewBase(printout, printoutForPrinting, data)
{
    DetermineScaling();
}

wxMacPrintPreview::wxMacPrintPreview(wxPrintout *printout, wxPrintout *printoutForPrinting, wxPrintData *data):
wxPrintPreviewBase(printout, printoutForPrinting, data)
{
    DetermineScaling();
}

wxMacPrintPreview::~wxMacPrintPreview(void)
{
}

bool wxMacPrintPreview::Print(bool interactive)
{
    if (!m_printPrintout)
        return false;

    wxMacPrinter printer(&m_printDialogData);
    return printer.Print(m_previewFrame, m_printPrintout, interactive);
}

void wxMacPrintPreview::DetermineScaling(void)
{
    int screenWidth , screenHeight ;
    wxDisplaySize( &screenWidth , &screenHeight ) ;

    wxSize ppiScreen( 72 , 72 ) ;
    wxSize ppiPrinter( 72 , 72 ) ;

    // Note that with Leopard, screen dpi=72 is no longer a given
    m_previewPrintout->SetPPIScreen( ppiScreen.x , ppiScreen.y ) ;

    wxCoord w , h ;
    wxCoord ww, hh;
    wxRect paperRect;

    // Get a device context for the currently selected printer
    wxPrinterDC printerDC(m_printDialogData.GetPrintData());
    if (printerDC.IsOk())
    {
        printerDC.GetSizeMM(&ww, &hh);
        printerDC.GetSize( &w , &h ) ;
        ppiPrinter = printerDC.GetPPI() ;
        paperRect = printerDC.GetPaperRect();
        m_isOk = true ;
    }
    else
    {
        // use some defaults
        w = 8 * 72 ;
        h = 11 * 72 ;
        ww = (wxCoord) (w * 25.4 / ppiPrinter.x) ;
        hh = (wxCoord) (h * 25.4 / ppiPrinter.y) ;
        paperRect = wxRect(0, 0, w, h);
        m_isOk = false ;
    }
    m_pageWidth = w;
    m_pageHeight = h;

    m_previewPrintout->SetPageSizePixels(w , h) ;
    m_previewPrintout->SetPageSizeMM(ww, hh);
    m_previewPrintout->SetPaperRectPixels(paperRect);
    m_previewPrintout->SetPPIPrinter( ppiPrinter.x , ppiPrinter.y ) ;

    m_previewScaleX = float(ppiScreen.x) / ppiPrinter.x;
    m_previewScaleY = float(ppiScreen.y) / ppiPrinter.y;
}

//
// end of print_osx.cpp
//

#if wxOSX_USE_CARBON

IMPLEMENT_DYNAMIC_CLASS(wxOSXCarbonPrintData, wxOSXPrintData)

wxOSXCarbonPrintData::wxOSXCarbonPrintData()
{
    if ( PMCreateSession( &m_macPrintSession ) == noErr )
    {
        if ( PMCreatePageFormat(&m_macPageFormat) == noErr )
        {
            PMSessionDefaultPageFormat(m_macPrintSession,
                    m_macPageFormat);
            PMGetPageFormatPaper(m_macPageFormat, &m_macPaper);
        }

        if ( PMCreatePrintSettings(&m_macPrintSettings) == noErr )
        {
            PMSessionDefaultPrintSettings(m_macPrintSession,
                m_macPrintSettings);
        }
    }
}

wxOSXCarbonPrintData::~wxOSXCarbonPrintData()
{
    (void)PMRelease(m_macPageFormat);
    (void)PMRelease(m_macPrintSettings);
    (void)PMRelease(m_macPrintSession);
}
#endif

#endif
