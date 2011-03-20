///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/dataobj.cpp
// Purpose:     wxDataObject class
// Author:      Robert Roebling
// Id:          $Id: dataobj.cpp 54741 2008-07-21 03:35:15Z VZ $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_DATAOBJ

#include "wx/dataobj.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/image.h"
#endif

#include "wx/mstream.h"
#include "wx/uri.h"

#include "wx/gtk/private.h"

//-------------------------------------------------------------------------
// global data
//-------------------------------------------------------------------------

GdkAtom  g_textAtom        = 0;
GdkAtom  g_altTextAtom     = 0;
GdkAtom  g_pngAtom         = 0;
GdkAtom  g_fileAtom        = 0;

//-------------------------------------------------------------------------
// wxDataFormat
//-------------------------------------------------------------------------

wxDataFormat::wxDataFormat()
{
    // do *not* call PrepareFormats() from here for 2 reasons:
    //
    // 1. we will have time to do it later because some other Set function
    //    must be called before we really need them
    //
    // 2. doing so prevents us from declaring global wxDataFormats because
    //    calling PrepareFormats (and thus gdk_atom_intern) before GDK is
    //    initialised will result in a crash
    m_type = wxDF_INVALID;
    m_format = (GdkAtom) 0;
}

wxDataFormat::wxDataFormat( wxDataFormatId type )
{
    PrepareFormats();
    SetType( type );
}

void wxDataFormat::InitFromString( const wxString &id )
{
    PrepareFormats();
    SetId( id );
}

wxDataFormat::wxDataFormat( NativeFormat format )
{
    PrepareFormats();
    SetId( format );
}

void wxDataFormat::SetType( wxDataFormatId type )
{
    PrepareFormats();

    m_type = type;

#if wxUSE_UNICODE
    if (m_type == wxDF_UNICODETEXT)
        m_format = g_textAtom;
    else if (m_type == wxDF_TEXT)
        m_format = g_altTextAtom;
#else // !wxUSE_UNICODE
    // notice that we don't map wxDF_UNICODETEXT to g_textAtom here, this
    // would lead the code elsewhere to treat data objects with this format as
    // containing UTF-8 data which is not true
    if (m_type == wxDF_TEXT)
        m_format = g_textAtom;
#endif // wxUSE_UNICODE/!wxUSE_UNICODE
    else
    if (m_type == wxDF_BITMAP)
        m_format = g_pngAtom;
    else
    if (m_type == wxDF_FILENAME)
        m_format = g_fileAtom;
    else
    {
       wxFAIL_MSG( wxT("invalid dataformat") );
    }
}

wxDataFormatId wxDataFormat::GetType() const
{
    return m_type;
}

wxString wxDataFormat::GetId() const
{
    wxGtkString atom_name(gdk_atom_name(m_format));
    return wxString::FromAscii(atom_name);
}

void wxDataFormat::SetId( NativeFormat format )
{
    PrepareFormats();
    m_format = format;

    if (m_format == g_textAtom)
#if wxUSE_UNICODE
        m_type = wxDF_UNICODETEXT;
#else
        m_type = wxDF_TEXT;
#endif
    else
    if (m_format == g_altTextAtom)
        m_type = wxDF_TEXT;
    else
    if (m_format == g_pngAtom)
        m_type = wxDF_BITMAP;
    else
    if (m_format == g_fileAtom)
        m_type = wxDF_FILENAME;
    else
        m_type = wxDF_PRIVATE;
}

void wxDataFormat::SetId( const wxString& id )
{
    PrepareFormats();
    m_type = wxDF_PRIVATE;
    m_format = gdk_atom_intern( id.ToAscii(), FALSE );
}

void wxDataFormat::PrepareFormats()
{
    // VZ: GNOME included in RedHat 6.1 uses the MIME types below and not the
    //     atoms STRING and file:ALL as the old code was, but normal X apps
    //     use STRING for text selection when transferring the data via
    //     clipboard, for example, so do use STRING for now (GNOME apps will
    //     probably support STRING as well for compatibility anyhow), but use
    //     text/uri-list for file dnd because compatibility is not important
    //     here (with whom?)
    if (!g_textAtom)
    {
#if wxUSE_UNICODE
        g_textAtom = gdk_atom_intern( "UTF8_STRING", FALSE );
        g_altTextAtom = gdk_atom_intern( "STRING", FALSE );
#else
        g_textAtom = gdk_atom_intern( "STRING" /* "text/plain" */, FALSE );
#endif
    }
    if (!g_pngAtom)
        g_pngAtom = gdk_atom_intern( "image/png", FALSE );
    if (!g_fileAtom)
        g_fileAtom = gdk_atom_intern( "text/uri-list", FALSE );
}

//-------------------------------------------------------------------------
// wxDataObject
//-------------------------------------------------------------------------

wxDataObject::wxDataObject()
{
}

wxDataObject::~wxDataObject()
{
    // dtor is empty but needed for Darwin and AIX -- otherwise it doesn't link
}

bool wxDataObject::IsSupportedFormat(const wxDataFormat& format, Direction dir) const
{
    size_t nFormatCount = GetFormatCount(dir);
    if ( nFormatCount == 1 )
    {
        return format == GetPreferredFormat();
    }
    else
    {
        wxDataFormat *formats = new wxDataFormat[nFormatCount];
        GetAllFormats(formats,dir);

        size_t n;
        for ( n = 0; n < nFormatCount; n++ )
        {
            if ( formats[n] == format )
                break;
        }

        delete [] formats;

        // found?
        return n < nFormatCount;
    }
}

// ----------------------------------------------------------------------------
// wxTextDataObject
// ----------------------------------------------------------------------------

#if wxUSE_UNICODE

void
wxTextDataObject::GetAllFormats(wxDataFormat *formats,
                                wxDataObjectBase::Direction WXUNUSED(dir)) const
{
    *formats++ = GetPreferredFormat();
    *formats = g_altTextAtom;
}

#endif // wxUSE_UNICODE

// ----------------------------------------------------------------------------
// wxFileDataObject
// ----------------------------------------------------------------------------

bool wxFileDataObject::GetDataHere(void *buf) const
{
    wxString filenames;

    for (size_t i = 0; i < m_filenames.GetCount(); i++)
    {
        filenames += wxT("file:");
        filenames += m_filenames[i];
        filenames += wxT("\r\n");
    }

    memcpy( buf, filenames.mbc_str(), filenames.length() + 1 );

    return true;
}

size_t wxFileDataObject::GetDataSize() const
{
    size_t res = 0;

    for (size_t i = 0; i < m_filenames.GetCount(); i++)
    {
        // This is junk in UTF-8
        res += m_filenames[i].length();
        res += 5 + 2; // "file:" (5) + "\r\n" (2)
    }

    return res + 1;
}

bool wxFileDataObject::SetData(size_t WXUNUSED(size), const void *buf)
{
    // we get data in the text/uri-list format, i.e. as a sequence of URIs
    // (filenames prefixed by "file:") delimited by "\r\n". size includes
    // the trailing zero (in theory, not for Nautilus in early GNOME
    // versions).

    m_filenames.Empty();

    const gchar *nexttemp = (const gchar*) buf;
    for ( ; ; )
    {
        int len = 0;
        const gchar *temp = nexttemp;
        for (;;)
        {
            if (temp[len] == 0)
            {
                if (len > 0)
                {
                    // if an app omits '\r''\n'
                    nexttemp = temp+len;
                    break;
                }

                return true;
            }
            if (temp[len] == '\r')
            {
                if (temp[len+1] == '\n')
                    nexttemp = temp+len+2;
                else
                    nexttemp = temp+len+1;
                break;
            }
            len++;
        }

        if (len == 0)
            break;

        // required to give it a trailing zero
        gchar *uri = g_strndup( temp, len );

        gchar *fn = g_filename_from_uri( uri, NULL, NULL );

        g_free( uri );

        if (fn)
        {
            AddFile( wxConvFileName->cMB2WX( fn ) );
            g_free( fn );
        }
    }

    return true;
}

void wxFileDataObject::AddFile( const wxString &filename )
{
   m_filenames.Add( filename );
}

// ----------------------------------------------------------------------------
// wxBitmapDataObject
// ----------------------------------------------------------------------------

wxBitmapDataObject::wxBitmapDataObject()
{
    Init();
}

wxBitmapDataObject::wxBitmapDataObject( const wxBitmap& bitmap )
                  : wxBitmapDataObjectBase(bitmap)
{
    Init();

    DoConvertToPng();
}

wxBitmapDataObject::~wxBitmapDataObject()
{
    Clear();
}

void wxBitmapDataObject::SetBitmap( const wxBitmap &bitmap )
{
    ClearAll();

    wxBitmapDataObjectBase::SetBitmap(bitmap);

    DoConvertToPng();
}

bool wxBitmapDataObject::GetDataHere(void *buf) const
{
    if ( !m_pngSize )
    {
        wxFAIL_MSG( wxT("attempt to copy empty bitmap failed") );

        return false;
    }

    memcpy(buf, m_pngData, m_pngSize);

    return true;
}

bool wxBitmapDataObject::SetData(size_t size, const void *buf)
{
    Clear();

    wxCHECK_MSG( wxImage::FindHandler(wxBITMAP_TYPE_PNG) != NULL,
                 false, wxT("You must call wxImage::AddHandler(new wxPNGHandler); to be able to use clipboard with bitmaps!") );

    m_pngSize = size;
    m_pngData = malloc(m_pngSize);

    memcpy(m_pngData, buf, m_pngSize);

    wxMemoryInputStream mstream((char*) m_pngData, m_pngSize);
    wxImage image;
    if ( !image.LoadFile( mstream, wxBITMAP_TYPE_PNG ) )
    {
        return false;
    }

    m_bitmap = wxBitmap(image);

    return m_bitmap.Ok();
}

void wxBitmapDataObject::DoConvertToPng()
{
    if ( !m_bitmap.Ok() )
        return;

    wxCHECK_RET( wxImage::FindHandler(wxBITMAP_TYPE_PNG) != NULL,
                 wxT("You must call wxImage::AddHandler(new wxPNGHandler); to be able to use clipboard with bitmaps!") );

    wxImage image = m_bitmap.ConvertToImage();

    wxCountingOutputStream count;
    image.SaveFile(count, wxBITMAP_TYPE_PNG);

    m_pngSize = count.GetSize() + 100; // sometimes the size seems to vary ???
    m_pngData = malloc(m_pngSize);

    wxMemoryOutputStream mstream((char*) m_pngData, m_pngSize);
    image.SaveFile(mstream, wxBITMAP_TYPE_PNG);
}

// ----------------------------------------------------------------------------
// wxURLDataObject
// ----------------------------------------------------------------------------

wxURLDataObject::wxURLDataObject(const wxString& url) :
   wxDataObjectSimple( wxDataFormat( gdk_atom_intern("text/x-moz-url",FALSE) ) )
{
   m_url = url;
}

size_t wxURLDataObject::GetDataSize() const
{
    if (m_url.empty())
        return 0;

    return 2*m_url.Len()+2;
}

bool wxURLDataObject::GetDataHere(void *buf) const
{
    if (m_url.empty())
        return false;

    wxCSConv conv( "UCS2" );
    conv.FromWChar( (char*) buf, 2*m_url.Len()+2, m_url.wc_str() );

    return true;
}

    // copy data from buffer to our data
bool wxURLDataObject::SetData(size_t len, const void *buf)
{
    if (len == 0)
    {
        m_url = wxEmptyString;
        return false;
    }

    wxCSConv conv( "UCS2" );
    wxWCharBuffer res = conv.cMB2WC( (const char*) buf );
    m_url = res;
    int pos = m_url.Find( '\n' );
    if (pos != wxNOT_FOUND)
        m_url.Remove( pos, m_url.Len() - pos );

    return true;
}


#endif // wxUSE_DATAOBJ
