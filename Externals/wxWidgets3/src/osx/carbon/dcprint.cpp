/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/dcprint.cpp
// Purpose:     wxPrinterDC class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_PRINTING_ARCHITECTURE

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/dcprint.h"

#ifndef WX_PRECOMP
    #include "wx/msgdlg.h"
    #include "wx/math.h"
#endif

#include "wx/osx/private.h"
#include "wx/osx/private/print.h"
#include "wx/osx/dcprint.h"
#include "wx/graphics.h"

wxIMPLEMENT_ABSTRACT_CLASS(wxPrinterDCImpl, wxGCDCImpl);

class wxNativePrinterDC
{
public :
    wxNativePrinterDC() {}
    virtual ~wxNativePrinterDC() {}
    virtual bool StartDoc(  wxPrinterDC* dc , const wxString& message ) = 0;
    virtual void EndDoc( wxPrinterDC* dc ) = 0;
    virtual void StartPage( wxPrinterDC* dc ) = 0;
    virtual void EndPage( wxPrinterDC* dc ) = 0;
    virtual void GetSize( int *w , int *h) const = 0 ;
    virtual wxSize GetPPI() const = 0 ;

    // returns 0 in case of no Error, otherwise platform specific error codes
    virtual wxUint32 GetStatus() const = 0 ;
    bool IsOk() const { return GetStatus() == 0 ; }

    static wxNativePrinterDC* Create(wxPrintData* data) ;
} ;

class wxMacCarbonPrinterDC : public wxNativePrinterDC
{
public :
    wxMacCarbonPrinterDC( wxPrintData* data ) ;
    virtual ~wxMacCarbonPrinterDC() ;
    virtual bool StartDoc(  wxPrinterDC* dc , const wxString& message ) wxOVERRIDE ;
    virtual void EndDoc( wxPrinterDC* dc ) wxOVERRIDE ;
    virtual void StartPage( wxPrinterDC* dc ) wxOVERRIDE ;
    virtual void EndPage( wxPrinterDC* dc ) wxOVERRIDE ;
    virtual wxUint32 GetStatus() const wxOVERRIDE { return m_err ; }
    virtual void GetSize( int *w , int *h) const wxOVERRIDE ;
    virtual wxSize GetPPI() const wxOVERRIDE ;
private :
    wxCoord m_maxX ;
    wxCoord m_maxY ;
    wxSize  m_ppi ;
    OSStatus m_err ;
} ;

wxMacCarbonPrinterDC::wxMacCarbonPrinterDC( wxPrintData* data )
{
    m_err = noErr ;
    wxOSXPrintData *native = (wxOSXPrintData*) data->GetNativeData() ;

    PMRect rPage;
    m_err = PMGetAdjustedPageRect(native->GetPageFormat(), &rPage);
    if ( m_err != noErr )
        return;

    m_maxX = wxCoord(rPage.right - rPage.left) ;
    m_maxY = wxCoord(rPage.bottom - rPage.top);

    PMResolution res;
    PMPrinter printer;
    m_err = PMSessionGetCurrentPrinter(native->GetPrintSession(), &printer);
    if ( m_err == noErr )
    {
        m_err = PMPrinterGetOutputResolution( printer, native->GetPrintSettings(), &res) ;
        if ( m_err == -9589 /* kPMKeyNotFound */ )
        {
            m_err = noErr ;
            res.hRes = res.vRes = 300;
        }
    }
    else
    {
        res.hRes = res.vRes = 300;
    }

    m_maxX = wxCoord((double)m_maxX * res.hRes / 72.0);
    m_maxY = wxCoord((double)m_maxY * res.vRes / 72.0);

    m_ppi = wxSize(int(res.hRes), int(res.vRes));
}

wxMacCarbonPrinterDC::~wxMacCarbonPrinterDC()
{
}

wxNativePrinterDC* wxNativePrinterDC::Create(wxPrintData* data)
{
    return new wxMacCarbonPrinterDC(data) ;
}

bool wxMacCarbonPrinterDC::StartDoc(  wxPrinterDC* dc , const wxString& message  )
{
    if ( m_err )
        return false ;

    wxPrinterDCImpl *impl = (wxPrinterDCImpl*) dc->GetImpl();
    wxOSXPrintData *native = (wxOSXPrintData*) impl->GetPrintData().GetNativeData() ;

    PMPrintSettingsSetJobName(native->GetPrintSettings(), wxCFStringRef(message));

    m_err = PMSessionBeginCGDocumentNoDialog(native->GetPrintSession(),
              native->GetPrintSettings(),
              native->GetPageFormat());
    if ( m_err != noErr )
        return false;

    PMRect rPage;
    m_err = PMGetAdjustedPageRect(native->GetPageFormat(), &rPage);
    if ( m_err != noErr )
        return false ;

    m_maxX = wxCoord(rPage.right - rPage.left) ;
    m_maxY = wxCoord(rPage.bottom - rPage.top);

    PMResolution res;
    PMPrinter printer;

    bool useDefaultResolution = true;
    m_err = PMSessionGetCurrentPrinter(native->GetPrintSession(), &printer);
    if (m_err == noErr)
    {
        m_err = PMPrinterGetOutputResolution( printer, native->GetPrintSettings(), &res) ;
        if (m_err == noErr)
            useDefaultResolution = true;
    }
    
    // Ignore errors which may occur while retrieving the resolution and just
    // use the default one.
    if ( useDefaultResolution )
    {
        res.hRes =
        res.vRes = 300;
        m_err = noErr ;
    }

    m_maxX = wxCoord((double)m_maxX * res.hRes / 72.0);
    m_maxY = wxCoord((double)m_maxY * res.vRes / 72.0);

    m_ppi = wxSize(int(res.hRes), int(res.vRes));
    return true ;
}

void wxMacCarbonPrinterDC::EndDoc( wxPrinterDC* dc )
{
    if ( m_err )
        return ;

    wxPrinterDCImpl *impl = (wxPrinterDCImpl*) dc->GetImpl();
    wxOSXPrintData *native = (wxOSXPrintData*) impl->GetPrintData().GetNativeData() ;

    m_err = PMSessionEndDocumentNoDialog(native->GetPrintSession());
}

void wxMacCarbonPrinterDC::StartPage( wxPrinterDC* dc )
{
    if ( m_err )
        return ;

    wxPrinterDCImpl *impl = (wxPrinterDCImpl*) dc->GetImpl();
    wxOSXPrintData *native = (wxOSXPrintData*) impl->GetPrintData().GetNativeData() ;

    m_err = PMSessionBeginPageNoDialog(native->GetPrintSession(),
                 native->GetPageFormat(),
                 NULL);

    CGContextRef pageContext = NULL ;

    if ( m_err == noErr )
    {
        m_err = PMSessionGetCGGraphicsContext(native->GetPrintSession(),
                                            &pageContext );
    }

    if ( m_err != noErr )
    {
        PMSessionEndPageNoDialog(native->GetPrintSession());
        PMSessionEndDocumentNoDialog(native->GetPrintSession());
    }
    else
    {
        PMRect paperRect ;
        m_err = PMGetAdjustedPaperRect( native->GetPageFormat() , &paperRect ) ;
        // make sure (0,0) is at the upper left of the printable area (wx conventions)
        // Core Graphics initially has the lower left of the paper as 0,0
        if ( !m_err )
            CGContextTranslateCTM( pageContext , (CGFloat) -paperRect.left , (CGFloat) paperRect.bottom ) ;

        // since this is a non-critical error, we set the flag back
        m_err = noErr ;

        // Leopard deprecated PMSetResolution() which will not be available in 64 bit mode, so we avoid using it.
        // To set the proper drawing resolution, the docs suggest the use of CGContextScaleCTM(), so here we go; as a
        // consequence though, PMGetAdjustedPaperRect() and PMGetAdjustedPageRect() return unscaled rects, so we
        // have to manually scale them later.
        CGContextScaleCTM( pageContext, 72.0 / (double)m_ppi.x, -72.0 / (double)m_ppi.y);

        impl->SetGraphicsContext( wxGraphicsContext::CreateFromNative( pageContext ) );
    }
}

void wxMacCarbonPrinterDC::EndPage( wxPrinterDC* dc )
{
    if ( m_err )
        return ;

    wxPrinterDCImpl *impl = (wxPrinterDCImpl*) dc->GetImpl();
    wxOSXPrintData *native = (wxOSXPrintData*) impl->GetPrintData().GetNativeData() ;

    m_err = PMSessionEndPageNoDialog(native->GetPrintSession());
    if ( m_err != noErr )
    {
        PMSessionEndDocumentNoDialog(native->GetPrintSession());
    }
    // the cg context we got when starting the page isn't valid anymore, so replace it
    impl->SetGraphicsContext( wxGraphicsContext::Create() );
}

void wxMacCarbonPrinterDC::GetSize( int *w , int *h) const
{
    if ( w )
        *w = m_maxX ;
    if ( h )
        *h = m_maxY ;
}

wxSize wxMacCarbonPrinterDC::GetPPI() const
{
     return m_ppi ;
};

//
//
//

wxPrinterDCImpl::wxPrinterDCImpl( wxPrinterDC *owner, const wxPrintData& printdata )
   : wxGCDCImpl( owner )
{
    m_ok = false ;
    m_printData = printdata ;
    m_printData.ConvertToNative() ;
    m_nativePrinterDC = wxNativePrinterDC::Create( &m_printData ) ;
    if ( m_nativePrinterDC )
    {
        m_ok = m_nativePrinterDC->IsOk() ;
        if ( !m_ok )
        {
            wxString message ;
            message.Printf( wxT("Print Error %u"), m_nativePrinterDC->GetStatus() ) ;
            wxMessageDialog dialog( NULL , message , wxEmptyString, wxICON_HAND | wxOK) ;
            dialog.ShowModal();
        }
        else
        {
            wxSize sz = GetPPI();
            m_mm_to_pix_x = mm2inches * sz.x;
            m_mm_to_pix_y = mm2inches * sz.y;
        }
        // we need at least a measuring context because people start measuring before a page
        // gets printed at all
        SetGraphicsContext( wxGraphicsContext::Create() );
    }
}

wxSize wxPrinterDCImpl::GetPPI() const
{
    return m_nativePrinterDC->GetPPI() ;
}

wxPrinterDCImpl::~wxPrinterDCImpl()
{
    delete m_nativePrinterDC ;
}

bool wxPrinterDCImpl::StartDoc( const wxString& message )
{
    wxASSERT_MSG( IsOk() , wxT("Called wxPrinterDC::StartDoc from an invalid object") ) ;

    if ( !m_ok )
        return false ;

    if ( m_nativePrinterDC->StartDoc( (wxPrinterDC*) GetOwner(), message ) )
    {
        // in case we have to do additional things when successful
    }
    m_ok = m_nativePrinterDC->IsOk() ;
    if ( !m_ok )
    {
        wxString message ;
        message.Printf( wxT("Print Error %u"), m_nativePrinterDC->GetStatus() ) ;
        wxMessageDialog dialog( NULL , message , wxEmptyString, wxICON_HAND | wxOK) ;
        dialog.ShowModal();
    }

    return m_ok ;
}

void wxPrinterDCImpl::EndDoc(void)
{
    if ( !m_ok )
        return ;

    m_nativePrinterDC->EndDoc( (wxPrinterDC*) GetOwner() ) ;
    m_ok = m_nativePrinterDC->IsOk() ;

    if ( !m_ok )
    {
        wxString message ;
        message.Printf( wxT("Print Error %u"), m_nativePrinterDC->GetStatus() ) ;
        wxMessageDialog dialog( NULL , message , wxEmptyString, wxICON_HAND | wxOK) ;
        dialog.ShowModal();
    }
}

wxRect wxPrinterDCImpl::GetPaperRect() const
{
    wxCoord w, h;
    GetOwner()->GetSize(&w, &h);
    wxRect pageRect(0, 0, w, h);
    wxOSXPrintData *native = (wxOSXPrintData*) m_printData.GetNativeData() ;
    OSStatus err = noErr ;
    PMRect rPaper;
    err = PMGetAdjustedPaperRect(native->GetPageFormat(), &rPaper);
    if ( err != noErr )
        return pageRect;

    wxSize ppi = GetOwner()->GetPPI();
    rPaper.right *= (ppi.x / 72.0);
    rPaper.bottom *= (ppi.y / 72.0);
    rPaper.left *= (ppi.x / 72.0);
    rPaper.top *= (ppi.y / 72.0);

    return wxRect(wxCoord(rPaper.left), wxCoord(rPaper.top),
        wxCoord(rPaper.right - rPaper.left), wxCoord(rPaper.bottom - rPaper.top));
}

void wxPrinterDCImpl::StartPage()
{
    if ( !m_ok )
        return ;

    m_logicalFunction = wxCOPY;
    //  m_textAlignment = wxALIGN_TOP_LEFT;
    m_backgroundMode = wxTRANSPARENT;

    m_textForegroundColour = *wxBLACK;
    m_textBackgroundColour = *wxWHITE;
    m_pen = *wxBLACK_PEN;
    m_font = *wxNORMAL_FONT;
    m_brush = *wxTRANSPARENT_BRUSH;
    m_backgroundBrush = *wxWHITE_BRUSH;

    m_nativePrinterDC->StartPage( (wxPrinterDC*) GetOwner() ) ;
    m_ok = m_nativePrinterDC->IsOk() ;

}

void wxPrinterDCImpl::EndPage()
{
    if ( !m_ok )
        return ;

    m_nativePrinterDC->EndPage( (wxPrinterDC*) GetOwner() );
    m_ok = m_nativePrinterDC->IsOk() ;
}

void wxPrinterDCImpl::DoGetSize(int *width, int *height) const
{
    wxCHECK_RET( m_ok , wxT("GetSize() doesn't work without a valid wxPrinterDC") );
    m_nativePrinterDC->GetSize(width,  height ) ;
}

#endif
