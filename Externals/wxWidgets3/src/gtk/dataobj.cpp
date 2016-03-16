///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/dataobj.cpp
// Purpose:     wxDataObject class
// Author:      Robert Roebling
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
GdkAtom  g_htmlAtom        = 0;

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
    if (m_type == wxDF_HTML)
        m_format = g_htmlAtom;
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
    if (m_format == g_htmlAtom)
        m_type = wxDF_HTML;
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
    if (!g_htmlAtom)
        g_htmlAtom = gdk_atom_intern( "text/html", FALSE );
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
    char* out = static_cast<char*>(buf);

    for (size_t i = 0; i < m_filenames.GetCount(); i++)
    {
        char* uri = g_filename_to_uri(m_filenames[i].mbc_str(), 0, 0);
        if (uri)
        {
            size_t const len = strlen(uri);
            memcpy(out, uri, len);
            out += len;
            *(out++) = '\r';
            *(out++) = '\n';
            g_free(uri);
        }
    }
    *out = 0;

    return true;
}

size_t wxFileDataObject::GetDataSize() const
{
    size_t res = 0;

    for (size_t i = 0; i < m_filenames.GetCount(); i++)
    {
        char* uri = g_filename_to_uri(m_filenames[i].mbc_str(), 0, 0);
        if (uri) {
            res += strlen(uri) + 2; // Including "\r\n"
            g_free(uri);
        }
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

    return m_bitmap.IsOk();
}

void wxBitmapDataObject::DoConvertToPng()
{
    if ( !m_bitmap.IsOk() )
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

class wxTextURIListDataObject : public wxDataObjectSimple
{
public:
    wxTextURIListDataObject(const wxString& url)
        : wxDataObjectSimple(wxDataFormat(g_fileAtom)),
          m_url(url)
    {
    }

    const wxString& GetURL() const { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }


    virtual size_t GetDataSize() const wxOVERRIDE
    {
        // It is not totally clear whether we should include "\r\n" at the end
        // of the string if there is only one URL or not, but not doing it
        // doesn't seem to create any problems, so keep things simple.
        return strlen(m_url.utf8_str()) + 1;
    }

    virtual bool GetDataHere(void *buf) const wxOVERRIDE
    {
        char* const dst = static_cast<char*>(buf);

        strcpy(dst, m_url.utf8_str());

        return true;
    }

    virtual bool SetData(size_t len, const void *buf) wxOVERRIDE
    {
        const char* const src = static_cast<const char*>(buf);

        // The string might be "\r\n"-terminated but this is not necessarily
        // the case (e.g. when dragging an URL from Firefox, it isn't).
        if ( len > 1 && src[len - 1] == '\n' )
        {
            if ( len > 2 && src[len - 2] == '\r' )
                len--;

            len--;
        }

        m_url = wxString::FromUTF8(src, len);

        return true;
    }

    // Must provide overloads to avoid hiding them (and warnings about it)
    virtual size_t GetDataSize(const wxDataFormat&) const wxOVERRIDE
    {
        return GetDataSize();
    }
    virtual bool GetDataHere(const wxDataFormat&, void *buf) const wxOVERRIDE
    {
        return GetDataHere(buf);
    }
    virtual bool SetData(const wxDataFormat&, size_t len, const void *buf) wxOVERRIDE
    {
        return SetData(len, buf);
    }

private:
    wxString m_url;
};

wxURLDataObject::wxURLDataObject(const wxString& url) :
    m_dobjURIList(new wxTextURIListDataObject(url)),
    m_dobjText(new wxTextDataObject(url))
{
    // Use both URL-specific format and a plain text one to ensure that URLs
    // can be pasted into any application.
    Add(m_dobjURIList, true /* preferred */);
    Add(m_dobjText);
}

void wxURLDataObject::SetURL(const wxString& url)
{
    m_dobjURIList->SetURL(url);
    m_dobjText->SetText(url);
}

wxString wxURLDataObject::GetURL() const
{
    if ( GetReceivedFormat() == g_fileAtom )
    {
        // If we received the URL as an URI, use it.
        return m_dobjURIList->GetURL();
    }
    else // Otherwise we either got it as text or didn't get anything yet.
    {
        // In either case using the text format should be fine.
        return m_dobjText->GetText();
    }
}

#endif // wxUSE_DATAOBJ
