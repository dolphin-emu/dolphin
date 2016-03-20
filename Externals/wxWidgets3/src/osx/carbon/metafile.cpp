/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/metafile.cpp
// Purpose:     wxMetaFile, wxMetaFileDC etc. These classes are optional.
// Author:      Stefan Csomor
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////
//
// Currently, the only purpose for making a metafile
// is to put it on the clipboard.


#include "wx/wxprec.h"

#if wxUSE_METAFILE

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/app.h"
#endif

#include "wx/metafile.h"
#include "wx/clipbrd.h"
#include "wx/osx/private.h"
#include "wx/graphics.h"
#include "wx/osx/metafile.h"

#include <stdio.h>
#include <string.h>

wxIMPLEMENT_DYNAMIC_CLASS(wxMetafile, wxObject);
wxIMPLEMENT_ABSTRACT_CLASS(wxMetafileDC, wxDC);
wxIMPLEMENT_ABSTRACT_CLASS(wxMetafileDCImpl, wxGCDCImpl);

#define M_METAFILEREFDATA( a ) ((wxMetafileRefData*)(a).GetRefData())

class wxMetafileRefData : public wxGDIRefData
{
public:
    // default ctor needed for CreateGDIRefData(), must be initialized later
    wxMetafileRefData() { Init(); }

    // creates a metafile from memory, assumes ownership
    wxMetafileRefData(CFDataRef data);

    // prepares a recording metafile
    wxMetafileRefData( int width, int height);

    // prepares a metafile to be read from a file (if filename is not empty)
    wxMetafileRefData( const wxString& filename);

    virtual ~wxMetafileRefData();

    virtual bool IsOk() const wxOVERRIDE { return m_data != NULL; }

    void Init();

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    CGPDFDocumentRef GetPDFDocument() const { return m_pdfDoc; }
    void UpdateDocumentFromData() ;

    const wxCFDataRef& GetData() const { return m_data; }
    CGContextRef GetContext() const { return m_context; }

    // ends the recording
    void Close();
private:
    wxCFDataRef m_data;
    wxCFRef<CGPDFDocumentRef> m_pdfDoc;
    CGContextRef m_context;

    int m_width ;
    int m_height ;


    // Our m_pdfDoc field can't be easily (deep) copied and so we don't define a
    // copy ctor.
    wxDECLARE_NO_COPY_CLASS(wxMetafileRefData);
};

wxMetafileRefData::wxMetafileRefData(CFDataRef data) :
    m_data(data)
{
    Init();
    UpdateDocumentFromData();
}

wxMetafileRefData::wxMetafileRefData( const wxString& filename )
{
    Init();

    if ( !filename.empty() )
    {
        wxCFRef<CFMutableStringRef> cfMutableString(CFStringCreateMutableCopy(NULL, 0, wxCFStringRef(filename)));
        CFStringNormalize(cfMutableString,kCFStringNormalizationFormD);
        wxCFRef<CFURLRef> url(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfMutableString , kCFURLPOSIXPathStyle, false));
        m_pdfDoc.reset(CGPDFDocumentCreateWithURL(url));
    }
}


wxMetafileRefData::wxMetafileRefData( int width, int height)
{
    Init();

    m_width = width;
    m_height = height;

    CGRect r = CGRectMake( 0 , 0 , width  , height );

    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    m_data.reset(data);
    CGDataConsumerRef dataConsumer = wxMacCGDataConsumerCreateWithCFData(data);
    m_context = CGPDFContextCreate( dataConsumer, (width != 0 && height != 0) ? &r : NULL , NULL );
    CGDataConsumerRelease( dataConsumer );
    if ( m_context )
    {
        CGPDFContextBeginPage(m_context, NULL);

        CGColorSpaceRef genericColorSpace  = wxMacGetGenericRGBColorSpace();

        CGContextSetFillColorSpace( m_context, genericColorSpace );
        CGContextSetStrokeColorSpace( m_context, genericColorSpace );

        CGContextTranslateCTM( m_context , 0 ,  height ) ;
        CGContextScaleCTM( m_context , 1 , -1 ) ;
    }
}

wxMetafileRefData::~wxMetafileRefData()
{
}

void wxMetafileRefData::Init()
{
    m_context = NULL;
    m_width = -1;
    m_height = -1;
}

void wxMetafileRefData::Close()
{
    CGPDFContextEndPage(m_context);

    CGContextRelease(m_context);
    m_context = NULL;

    UpdateDocumentFromData();
}

void wxMetafileRefData::UpdateDocumentFromData()
{
    wxCFRef<CGDataProviderRef> provider(wxMacCGDataProviderCreateWithCFData(m_data));
    m_pdfDoc.reset(CGPDFDocumentCreateWithProvider(provider));
    if ( m_pdfDoc != NULL )
    {
        CGPDFPageRef page = CGPDFDocumentGetPage( m_pdfDoc, 1 );
        CGRect rect = CGPDFPageGetBoxRect ( page, kCGPDFMediaBox);
        m_width = static_cast<int>(rect.size.width);
        m_height = static_cast<int>(rect.size.height);
    }
}

wxMetaFile::wxMetaFile(const wxString& file)
{
    m_refData = new wxMetafileRefData(file);
}

wxMetaFile::~wxMetaFile()
{
}

wxGDIRefData *wxMetaFile::CreateGDIRefData() const
{
    return new wxMetafileRefData;
}

wxGDIRefData *
wxMetaFile::CloneGDIRefData(const wxGDIRefData * WXUNUSED(data)) const
{
    wxFAIL_MSG( wxS("Cloning metafiles is not implemented in wxCarbon.") );

    return new wxMetafileRefData;
}

WXHMETAFILE wxMetaFile::GetHMETAFILE() const
{
    return (WXHMETAFILE) (CFDataRef) M_METAFILEDATA->GetData();
}

bool wxMetaFile::SetClipboard(int WXUNUSED(width), int WXUNUSED(height))
{
    bool success = true;

#if wxUSE_DRAG_AND_DROP
    if (m_refData == NULL)
        return false;

    bool alreadyOpen = wxTheClipboard->IsOpened();
    if (!alreadyOpen)
    {
        wxTheClipboard->Open();
        wxTheClipboard->Clear();
    }

    wxDataObject *data = new wxMetafileDataObject( *this );
    success = wxTheClipboard->SetData( data );
    if (!alreadyOpen)
        wxTheClipboard->Close();
#endif

    return success;
}

void wxMetafile::SetHMETAFILE(WXHMETAFILE mf)
{
    UnRef();

    m_refData = new wxMetafileRefData((CFDataRef)mf);
}

bool wxMetaFile::Play(wxDC *dc)
{
    if (!m_refData)
        return false;

    if (!dc->IsOk())
        return false;

    {
        wxDCImpl *impl = dc->GetImpl();
        wxGCDCImpl *gc_impl = wxDynamicCast(impl, wxGCDCImpl);
        if (gc_impl)
        {
            CGContextRef cg = (CGContextRef) (gc_impl->GetGraphicsContext()->GetNativeContext());
            CGPDFDocumentRef doc = M_METAFILEDATA->GetPDFDocument();
            CGPDFPageRef page = CGPDFDocumentGetPage( doc, 1 );
            wxMacCGContextStateSaver save(cg);
            CGContextDrawPDFPage( cg, page );
        }
//        CGContextTranslateCTM( cg, 0, bounds.size.width );
//        CGContextScaleCTM( cg, 1, -1 );
    }

    return true;
}

wxSize wxMetaFile::GetSize() const
{
    wxSize dataSize = wxDefaultSize;

    if (IsOk())
    {
        dataSize.x = M_METAFILEDATA->GetWidth();
        dataSize.y = M_METAFILEDATA->GetHeight();
    }

    return dataSize;
}

// Metafile device context

// New constructor that takes origin and extent. If you use this, don't
// give origin/extent arguments to wxMakeMetaFilePlaceable.

wxMetafileDCImpl::wxMetafileDCImpl(
    wxDC *owner,
    const wxString& filename,
    int width, int height,
    const wxString& WXUNUSED(description) ) :
    wxGCDCImpl( owner )
{
    wxASSERT_MSG( width != 0 || height != 0, wxT("no arbitration of metafile size supported") );
    wxASSERT_MSG( filename.empty(), wxT("no file based metafile support yet"));

    m_metaFile = new wxMetaFile( filename );
    wxMetafileRefData* metafiledata = new wxMetafileRefData(width, height);
    m_metaFile->UnRef();
    m_metaFile->SetRefData( metafiledata );

    SetGraphicsContext( wxGraphicsContext::CreateFromNative(metafiledata->GetContext()));
    m_ok = (m_graphicContext != NULL) ;

    SetMapMode( wxMM_TEXT );
}

wxMetafileDCImpl::~wxMetafileDCImpl()
{
}

void wxMetafileDCImpl::DoGetSize(int *width, int *height) const
{
    wxCHECK_RET( m_metaFile, wxT("GetSize() doesn't work without a metafile") );

    wxSize sz = m_metaFile->GetSize();
    if (width)
        (*width) = sz.x;
    if (height)
        (*height) = sz.y;
}

wxMetaFile *wxMetafileDCImpl::Close()
{
    wxDELETE(m_graphicContext);
    m_ok = false;

    M_METAFILEREFDATA(*m_metaFile)->Close();

    return m_metaFile;
}

#if wxUSE_DATAOBJ
size_t wxMetafileDataObject::GetDataSize() const
{
    CFIndex length = 0;
    wxMetafileRefData* refData = M_METAFILEREFDATA(m_metafile);
    if ( refData )
        length = refData->GetData().GetLength();
    return length;
}

bool wxMetafileDataObject::GetDataHere(void *buf) const
{
    bool result = false;

    wxMetafileRefData* refData = M_METAFILEREFDATA(m_metafile);
    if ( refData )
    {
        CFIndex length = refData->GetData().GetLength();
        if ( length > 0 )
        {
            result = true ;
            refData->GetData().GetBytes(CFRangeMake(0,length), (UInt8 *) buf);
        }
    }
    return result;
}

bool wxMetafileDataObject::SetData(size_t len, const void *buf)
{
    wxMetafileRefData* metafiledata = new wxMetafileRefData(wxCFRefFromGet(wxCFDataRef((UInt8*)buf, len).get()));
    m_metafile.UnRef();
    m_metafile.SetRefData( metafiledata );
    return true;
}
#endif

#endif
