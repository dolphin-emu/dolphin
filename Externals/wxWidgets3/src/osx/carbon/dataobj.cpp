///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/dataobj.cpp
// Purpose:     implementation of wxDataObject class
// Author:      Stefan Csomor
// Modified by:
// Created:     10/21/99
// Copyright:   (c) 1999 Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_DATAOBJ

#include "wx/dataobj.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/dcmemory.h"
    #include "wx/image.h"
#endif

#include "wx/mstream.h"
#include "wx/metafile.h"
#include "wx/scopedarray.h"
#include "wx/tokenzr.h"
#include "wx/filename.h"

#include "wx/osx/private.h"

// ----------------------------------------------------------------------------
// wxDataFormat
// ----------------------------------------------------------------------------

wxDataFormat::wxDataFormat()
{
    m_type = wxDF_INVALID;
    m_format = 0;
}

wxDataFormat::wxDataFormat( wxDataFormatId vType )
{
    m_format = 0;
    m_type = wxDF_INVALID;
    SetType( vType );
}

wxDataFormat::wxDataFormat( const wxChar *zId )
{
    m_format = 0;
    m_type = wxDF_INVALID;
    SetId( zId );
}

wxDataFormat::wxDataFormat( const wxString& rId )
{
    m_format = 0;
    m_type = wxDF_INVALID;
    SetId( rId );
}

wxDataFormat::wxDataFormat(const wxDataFormat& rFormat)
{
    if ( rFormat.m_format )
        m_format = (NativeFormat) CFStringCreateCopy(NULL, (CFStringRef)rFormat.m_format);
    else
        m_format = 0;
    m_type = rFormat.m_type;
    m_id = rFormat.m_id;
}

wxDataFormat::wxDataFormat( NativeFormat vFormat )
{
    m_format = 0;
    m_type = wxDF_INVALID;
    SetId( vFormat );
}

wxDataFormat::~wxDataFormat()
{
    if ( m_format != 0 )
    {
        CFRelease( (CFStringRef) m_format );
        m_format = 0;
    }
}

// in order to be correct for 10.3 we restrict to the available types there
// http://developer.apple.com/qa/qa2005/qa1406.html
// TODO : Use UTCoreTypes.h constants once we support 10.4+ only

wxDataFormat& wxDataFormat::operator=(const wxDataFormat& rFormat)
{
    if ( m_format != 0 )
    {
        CFRelease( (CFStringRef) m_format );
        m_format = 0;
    }
    if ( rFormat.m_format )
        m_format = (NativeFormat) CFStringCreateCopy(NULL, (CFStringRef)rFormat.m_format);
    m_type = rFormat.m_type;
    m_id = rFormat.m_id;
    return *this;
}

void wxDataFormat::SetType( wxDataFormatId dataType )
{
    m_type = dataType;
    if ( m_format != 0 )
    {
        CFRelease( (CFStringRef) m_format );
        m_format = 0;
    }

    switch (m_type)
    {
    case wxDF_TEXT:
        m_format = (long) CFStringCreateCopy( NULL, kUTTypePlainText );
        break;

    case wxDF_UNICODETEXT:
        m_format = (long) CFStringCreateCopy( NULL, kUTTypeUTF16PlainText );
        break;

    case wxDF_HTML:
        m_format = (long) CFStringCreateCopy( NULL, kUTTypeHTML );
        break;

    case wxDF_BITMAP:
        m_format = (long) CFStringCreateCopy( NULL, kUTTypeTIFF );
        break;
    case wxDF_METAFILE:
        m_format = (long) CFStringCreateCopy( NULL, kUTTypePDF );
        break;

    case wxDF_FILENAME:
        m_format = (long) CFStringCreateCopy( NULL, kUTTypeFileURL );
        break;

    default:
       wxFAIL_MSG( wxT("invalid data format") );
       break;
    }
}

wxString wxDataFormat::GetId() const
{
    return wxCFStringRef(wxCFRetain((CFStringRef)m_format)).AsString();
}

void wxDataFormat::SetId( NativeFormat format )
{
    if ( m_format != 0 )
    {
        CFRelease( (CFStringRef) m_format );
        m_format = 0;
    }
    m_format = (NativeFormat) CFStringCreateCopy(NULL, (CFStringRef)format);
    if ( UTTypeConformsTo( (CFStringRef)format, kUTTypeHTML ) )
    {
        m_type = wxDF_HTML;
    }
    if (  UTTypeConformsTo( (CFStringRef)format, kUTTypeUTF16PlainText ) )
    {
        m_type = wxDF_UNICODETEXT;
    }
    else if (  UTTypeConformsTo( (CFStringRef)format,kUTTypeUTF16ExternalPlainText ) )
    {
        m_type = wxDF_UNICODETEXT;
    }
    else if (  UTTypeConformsTo( (CFStringRef)format,kUTTypeUTF8PlainText ) )
    {
        m_type = wxDF_UNICODETEXT;
    }
    else if ( UTTypeConformsTo( (CFStringRef)format, kUTTypePlainText ) )
    {
        m_type = wxDF_TEXT;
    }
    else if (  UTTypeConformsTo( (CFStringRef)format, kUTTypeImage ) )
    {
        m_type = wxDF_BITMAP;
    }
    else if (  UTTypeConformsTo( (CFStringRef)format, kUTTypePDF ) )
    {
        m_type = wxDF_METAFILE;
    }
    else if (  UTTypeConformsTo( (CFStringRef)format, kUTTypeFileURL ) ||
             UTTypeConformsTo( (CFStringRef)format, kPasteboardTypeFileURLPromise))
    {
        m_type = wxDF_FILENAME;
    }
    else
    {
        m_type = wxDF_PRIVATE;
        m_id = wxCFStringRef( (CFStringRef) CFRetain((CFStringRef) format )).AsString();
    }
}

void wxDataFormat::SetId( const wxString& zId )
{
    m_type = wxDF_PRIVATE;
    m_id = zId;
    if ( m_format != 0 )
    {
        CFRelease( (CFStringRef) m_format );
        m_format = 0;
    }
    // since it is private, no need to conform to anything ...
    m_format = (long) wxCFRetain( (CFStringRef) wxCFStringRef(m_id) );
}

bool wxDataFormat::operator==(const wxDataFormat& format) const
{
    if (IsStandard() || format.IsStandard())
        return (format.m_type == m_type);
    else
        return ( UTTypeConformsTo( (CFStringRef) m_format , (CFStringRef) format.m_format ) );
}

//-------------------------------------------------------------------------
// wxDataObject
//-------------------------------------------------------------------------

wxDataObject::wxDataObject()
{
}

bool wxDataObject::IsSupportedFormat( const wxDataFormat& rFormat, Direction vDir ) const
{
    size_t nFormatCount = GetFormatCount( vDir );
    bool found = false;

    if (nFormatCount == 1)
    {
        found = (rFormat == GetPreferredFormat());
    }
    else
    {
        wxDataFormat *pFormats = new wxDataFormat[nFormatCount];
        GetAllFormats( pFormats, vDir );

        for (size_t n = 0; n < nFormatCount; n++)
        {
            if (pFormats[n] == rFormat)
            {
                found = true;
                break;
            }
        }

        delete [] pFormats;
    }

    return found;
}

void wxDataObject::AddToPasteboard( void * pb, wxIntPtr itemID )
{
    PasteboardRef pasteboard = (PasteboardRef) pb;
   // get formats from wxDataObjects
    wxDataFormat *array = new wxDataFormat[ GetFormatCount() ];
    GetAllFormats( array );

    for (size_t i = 0; i < GetFormatCount(); i++)
    {
        wxDataFormat thisFormat = array[ i ];

        // add four bytes at the end for data objs like text that
        // have a datasize = strlen but still need a buffer for the
        // string including trailing zero

        size_t datasize = GetDataSize( thisFormat );
        if ( datasize == wxCONV_FAILED && thisFormat.GetType() == wxDF_TEXT)
        {
            // conversion to local text failed, so we must use unicode
            // if wxDF_UNICODETEXT is already on the 'todo' list, skip this iteration
            // otherwise force it
            size_t j = 0;
            for (j = 0; j < GetFormatCount(); j++)
            {
                if ( array[j].GetType() == wxDF_UNICODETEXT )
                    break;
            }
            if ( j < GetFormatCount() )
                continue;
            
            thisFormat.SetType(wxDF_UNICODETEXT);
            datasize = GetDataSize( thisFormat );
        }
            
        size_t sz = datasize + 4;
        void* buf = malloc( sz );
        if ( buf != NULL )
        {
            // empty the buffer because in some case GetDataHere does not fill buf
            memset( buf, 0, sz );
            if ( GetDataHere( thisFormat, buf ) )
            {
                wxIntPtr counter = 1 ;
                if ( thisFormat.GetType() == wxDF_FILENAME )
                {
                    // the data is D-normalized UTF8 strings of filenames delimited with \n
                    char *fname = strtok((char*) buf,"\n");
                    while (fname != NULL)
                    {
                        // translate the filepath into a fileurl and put that into the pasteobard
                        CFStringRef path = CFStringCreateWithBytes(NULL,(UInt8*)fname,strlen(fname),kCFStringEncodingUTF8,false);
                        CFURLRef url = CFURLCreateWithFileSystemPath(NULL, path , kCFURLPOSIXPathStyle, false);
                        CFRelease(path);
                        CFDataRef data = CFURLCreateData(NULL,url,kCFStringEncodingUTF8,true);
                        CFRelease(url);
                        PasteboardPutItemFlavor( pasteboard, (PasteboardItemID) counter,
                            (CFStringRef) thisFormat.GetFormatId() , data, kPasteboardFlavorNoFlags);
                        CFRelease( data );
                        counter++;
                        fname = strtok (NULL,"\n");
                    }

                }
                else
                {
                    CFDataRef data = CFDataCreate( kCFAllocatorDefault, (UInt8*)buf, datasize );
                    if ( thisFormat.GetType() == wxDF_TEXT )
                         PasteboardPutItemFlavor( pasteboard, (PasteboardItemID) itemID,
                            CFSTR("com.apple.traditional-mac-plain-text") , data, kPasteboardFlavorNoFlags);
                    else
                        PasteboardPutItemFlavor( pasteboard, (PasteboardItemID) itemID,
                            (CFStringRef) thisFormat.GetFormatId() , data, kPasteboardFlavorNoFlags);
                    CFRelease( data );
                }
            }
            free( buf );
        }
    }

    delete [] array;
}

bool wxDataObject::IsFormatInPasteboard( void * pb, const wxDataFormat &dataFormat )
{
    PasteboardRef pasteboard = (PasteboardRef) pb;
    bool hasData = false;
    OSStatus err = noErr;
    ItemCount itemCount;

    // we synchronize here once again, so we don't mind which flags get returned
    PasteboardSynchronize( pasteboard );

    err = PasteboardGetItemCount( pasteboard, &itemCount );
    if ( err == noErr )
    {
        for( UInt32 itemIndex = 1; itemIndex <= itemCount && hasData == false ; itemIndex++ )
        {
            PasteboardItemID    itemID;
            CFArrayRef          flavorTypeArray;
            CFIndex             flavorCount;

            err = PasteboardGetItemIdentifier( pasteboard, itemIndex, &itemID );
            if ( err != noErr )
                continue;

            err = PasteboardCopyItemFlavors( pasteboard, itemID, &flavorTypeArray );
            if ( err != noErr )
                continue;

            flavorCount = CFArrayGetCount( flavorTypeArray );

            for( CFIndex flavorIndex = 0; flavorIndex < flavorCount && hasData == false ; flavorIndex++ )
            {
                CFStringRef             flavorType;

                flavorType = (CFStringRef)CFArrayGetValueAtIndex( flavorTypeArray,
                                                                     flavorIndex );

                wxDataFormat flavorFormat( (wxDataFormat::NativeFormat) flavorType );
                if ( dataFormat == flavorFormat )
                    hasData = true;
                else if (  dataFormat.GetType() == wxDF_UNICODETEXT && flavorFormat.GetType() == wxDF_TEXT )
                    hasData = true;
            }
            CFRelease (flavorTypeArray);
        }
    }

    return hasData;
}

bool wxDataObject::GetFromPasteboard( void * pb )
{
    PasteboardRef pasteboard = (PasteboardRef) pb;

    size_t formatcount = GetFormatCount(wxDataObject::Set);
    wxScopedArray<wxDataFormat> array(formatcount);
    GetAllFormats(array.get(), wxDataObject::Set);
    
    ItemCount itemCount = 0;
    wxString filenamesPassed;
    bool transferred = false;
    bool pastelocationset = false;

    // we synchronize here once again, so we don't mind which flags get returned
    PasteboardSynchronize( pasteboard );

    OSStatus err = PasteboardGetItemCount( pasteboard, &itemCount );
    if ( err == noErr )
    {
        for (size_t i = 0; !transferred && i < formatcount; i++)
        {
            // go through the data in our order of preference
            wxDataFormat dataFormat = array[ i ];

            for( UInt32 itemIndex = 1; itemIndex <= itemCount && transferred == false ; itemIndex++ )
            {
                PasteboardItemID    itemID = 0;
                CFArrayRef          flavorTypeArray = NULL;
                CFIndex             flavorCount = 0;

                err = PasteboardGetItemIdentifier( pasteboard, itemIndex, &itemID );
                if ( err != noErr )
                    continue;

                err = PasteboardCopyItemFlavors( pasteboard, itemID, &flavorTypeArray );
                if ( err != noErr )
                    continue;

                flavorCount = CFArrayGetCount( flavorTypeArray );

                for( CFIndex flavorIndex = 0; !transferred && flavorIndex < flavorCount ; flavorIndex++ )
                {
                    CFStringRef             flavorType;
                    CFDataRef               flavorData;
                    CFIndex                 flavorDataSize;

                    flavorType = (CFStringRef)CFArrayGetValueAtIndex( flavorTypeArray,
                                                                         flavorIndex );

                    wxDataFormat flavorFormat( (wxDataFormat::NativeFormat) flavorType );

                    if ( dataFormat == flavorFormat )
                    {
                        if ( UTTypeConformsTo( (CFStringRef)flavorType, kPasteboardTypeFileURLPromise) )
                        {
                            if ( !pastelocationset )
                            {
                                wxString tempdir = wxFileName::GetTempDir() + wxFILE_SEP_PATH + "wxtemp.XXXXXX";
                                char* result = mkdtemp((char*)tempdir.fn_str().data());
                                
                                if (!result)
                                    continue;
                                
                                wxCFRef<CFURLRef> dest(CFURLCreateFromFileSystemRepresentation(NULL,(const UInt8*)result,strlen(result),true));
                                PasteboardSetPasteLocation(pasteboard, dest);
                                pastelocationset = true;
                           }
                        }
                        else if ( flavorFormat.GetType() != wxDF_PRIVATE )
                        {
                            // indicate the expected format for the type, benefiting from native conversions eg utf8 -> utf16
                            flavorType = (CFStringRef) wxDataFormat( flavorFormat.GetType()).GetFormatId();
                        }
                        
                        err = PasteboardCopyItemFlavorData( pasteboard, itemID, flavorType , &flavorData );
                        if ( err == noErr )
                        {
                            flavorDataSize = CFDataGetLength( flavorData );
                            if (dataFormat.GetType() == wxDF_FILENAME )
                            {
                                 // revert the translation and decomposition to arrive at a proper utf8 string again
                                CFURLRef url = CFURLCreateWithBytes( kCFAllocatorDefault, CFDataGetBytePtr( flavorData ), flavorDataSize, kCFStringEncodingUTF8, NULL );
                                CFStringRef cfString = CFURLCopyFileSystemPath( url, kCFURLPOSIXPathStyle );
                                CFRelease( url );
                                CFMutableStringRef cfMutableString = CFStringCreateMutableCopy(NULL, 0, cfString);
                                CFRelease( cfString );
                                CFStringNormalize(cfMutableString,kCFStringNormalizationFormC);
                                wxString path = wxCFStringRef(cfMutableString).AsString();
                                if (!path.empty())
                                    filenamesPassed += path + wxT("\n");
                            }
                            else
                            {
                                // because some data implementation expect trailing a trailing NUL, we add some headroom
                                void *buf = malloc( flavorDataSize + 4 );
                                if ( buf )
                                {
                                    memset( buf, 0, flavorDataSize + 4 );
                                    memcpy( buf, CFDataGetBytePtr( flavorData ), flavorDataSize );

                                    if (dataFormat.GetType() == wxDF_TEXT)
                                        wxMacConvertNewlines10To13( (char*) buf );
                                    SetData( flavorFormat, flavorDataSize, buf );
                                    transferred = true;
                                    free( buf );
                                }
                            }
                            CFRelease (flavorData);
                        }
                    }
                    else if ( dataFormat.GetType() == wxDF_UNICODETEXT && flavorFormat.GetType() == wxDF_TEXT )
                    {
                        err = PasteboardCopyItemFlavorData( pasteboard, itemID, flavorType, &flavorData );
                        if ( err == noErr )
                        {
                            flavorDataSize = CFDataGetLength( flavorData );
                            void *asciibuf = malloc( flavorDataSize + 1 );
                            if ( asciibuf )
                            {
                                memset( asciibuf, 0, flavorDataSize + 1 );
                                memcpy( asciibuf, CFDataGetBytePtr( flavorData ), flavorDataSize );
                                CFRelease (flavorData);

                                SetData( wxDF_TEXT, flavorDataSize, asciibuf );
                                transferred = true;
                                free( asciibuf );
                            }
                            else
                                CFRelease (flavorData);
                        }
                    }
                }
                CFRelease( flavorTypeArray );
            }
            if ( !filenamesPassed.empty() )
            {
                wxCharBuffer buf = filenamesPassed.fn_str();
                SetData( wxDF_FILENAME, strlen( buf ), (const char*)buf );
                transferred = true;
            }
        }
    }
    return transferred;
}

bool wxDataObject::HasDataInPasteboard( void * pb )
{
    PasteboardRef pasteboard = (PasteboardRef) pb;
    size_t formatcount = GetFormatCount(wxDataObject::Set);
    wxScopedArray<wxDataFormat> array(formatcount);
    GetAllFormats(array.get(), wxDataObject::Set);
    ItemCount itemCount = 0;
    bool hasData = false;

    // we synchronize here once again, so we don't mind which flags get returned
    PasteboardSynchronize( pasteboard );

    OSStatus err = PasteboardGetItemCount( pasteboard, &itemCount );
    if ( err == noErr )
    {
        for (size_t i = 0; !hasData && i < formatcount; i++)
        {
            // go through the data in our order of preference
            wxDataFormat dataFormat = array[ i ];

            for( UInt32 itemIndex = 1; itemIndex <= itemCount && hasData == false ; itemIndex++ )
            {
                PasteboardItemID    itemID = 0;
                CFArrayRef          flavorTypeArray = NULL;
                CFIndex             flavorCount = 0;

                err = PasteboardGetItemIdentifier( pasteboard, itemIndex, &itemID );
                if ( err != noErr )
                    continue;

                err = PasteboardCopyItemFlavors( pasteboard, itemID, &flavorTypeArray );
                if ( err != noErr )
                    continue;

                flavorCount = CFArrayGetCount( flavorTypeArray );

                for( CFIndex flavorIndex = 0; !hasData && flavorIndex < flavorCount ; flavorIndex++ )
                {
                    CFStringRef             flavorType;

                    flavorType = (CFStringRef)CFArrayGetValueAtIndex( flavorTypeArray,
                                                                         flavorIndex );

                    wxDataFormat flavorFormat( (wxDataFormat::NativeFormat) flavorType );

                    if ( dataFormat == flavorFormat ||
                        (dataFormat.GetType() == wxDF_UNICODETEXT && flavorFormat.GetType() == wxDF_TEXT) )
                    {
                        hasData = true;
                    }
                }
                CFRelease( flavorTypeArray );
            }
        }
    }
    return hasData;
}

#if wxOSX_USE_COCOA

void wxDataObject::AddSupportedTypes( void* cfarray)
{
    size_t nFormats = GetFormatCount(wxDataObject::Set);
    wxDataFormat *array = new wxDataFormat[nFormats];
    GetAllFormats(array, wxDataObject::Set);
    
    for (size_t i = 0; i < nFormats; i++)
    {
        wxDataFormat dataFormat = array[ i ];
        
        if ( dataFormat.GetType() == wxDF_UNICODETEXT || dataFormat.GetType() == wxDF_TEXT )
        {
            CFArrayAppendValue((CFMutableArrayRef)cfarray, kUTTypeUTF16PlainText);
            CFArrayAppendValue((CFMutableArrayRef)cfarray, kUTTypePlainText);
        }
        else if ( dataFormat.GetType() == wxDF_FILENAME )
        {
            CFArrayAppendValue((CFMutableArrayRef)cfarray, kUTTypeFileURL);
            CFArrayAppendValue((CFMutableArrayRef)cfarray, kPasteboardTypeFileURLPromise);
        }
        else if ( dataFormat.GetType() == wxDF_HTML )
        {
            CFArrayAppendValue((CFMutableArrayRef)cfarray, kUTTypeHTML);
        }
        else if ( dataFormat.GetType() == wxDF_BITMAP )
        {
            CFArrayAppendValue((CFMutableArrayRef)cfarray, kUTTypeTIFF);
            CFArrayAppendValue((CFMutableArrayRef)cfarray, kUTTypePICT);
        }
        else if ( dataFormat.GetType() == wxDF_METAFILE )
        {
            CFArrayAppendValue((CFMutableArrayRef)cfarray, kUTTypePDF);
        }
        else if ( dataFormat.GetType() == wxDF_PRIVATE )
        {
            CFArrayAppendValue((CFMutableArrayRef)cfarray, (CFStringRef) dataFormat.GetFormatId());
        }
    }
    delete[] array;
}

#endif

// ----------------------------------------------------------------------------
// wxTextDataObject
// ----------------------------------------------------------------------------

#if wxUSE_UNICODE
void wxTextDataObject::GetAllFormats(wxDataFormat *formats,
                                     wxDataObjectBase::Direction WXUNUSED(dir)) const
{
    *formats++ = wxDataFormat(wxDF_UNICODETEXT);
    *formats = wxDataFormat(wxDF_TEXT);
}
#endif

// ----------------------------------------------------------------------------
// wxFileDataObject
// ----------------------------------------------------------------------------

void wxFileDataObject::GetFileNames( wxCharBuffer &buf ) const
{
    wxString filenames;

    for (size_t i = 0; i < m_filenames.GetCount(); i++)
    {
        filenames += m_filenames[i];
        filenames += wxT('\n');
    }

    buf = filenames.fn_str();
}

bool wxFileDataObject::GetDataHere( void *pBuf ) const
{
    if (pBuf == NULL)
        return false;

    wxCharBuffer buf;
    size_t buffLength;

    GetFileNames( buf );
    buffLength = strlen( buf );
    memcpy( pBuf, (const char*)buf, buffLength + 1 );

    return true;
}

size_t wxFileDataObject::GetDataSize() const
{
    wxCharBuffer buf;
    size_t buffLength;

    GetFileNames( buf );
    buffLength = strlen( buf );
    // terminating 0
    return buffLength + 1;
}

bool wxFileDataObject::SetData( size_t WXUNUSED(nSize), const void *pBuf )
{
    wxString filenames;

#if wxUSE_UNICODE
    filenames = wxString( (const char*)pBuf, *wxConvFileName );
#else
    filenames = wxString (wxConvLocal.cWC2WX(wxConvFileName->cMB2WC( (const char*)pBuf)));
#endif

    m_filenames = wxStringTokenize( filenames, wxT("\n"), wxTOKEN_STRTOK );

    return true;
}

void wxFileDataObject::AddFile( const wxString& rFilename )
{
    m_filenames.Add( rFilename );
}

// ----------------------------------------------------------------------------
// wxBitmapDataObject
// ----------------------------------------------------------------------------

wxBitmapDataObject::wxBitmapDataObject()
{
    Init();
}

wxBitmapDataObject::wxBitmapDataObject( const wxBitmap& rBitmap )
: wxBitmapDataObjectBase( rBitmap )
{
    Init();

    if (m_bitmap.IsOk())
    {
        SetBitmap( rBitmap );
    }
}

wxBitmapDataObject::~wxBitmapDataObject()
{
    Clear();
}

void wxBitmapDataObject::SetBitmap( const wxBitmap& rBitmap )
{
    Clear();
    wxBitmapDataObjectBase::SetBitmap( rBitmap );
    if (m_bitmap.IsOk())
    {
        CGImageRef cgImageRef = (CGImageRef) m_bitmap.CreateCGImage();

        CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
        CGImageDestinationRef destination = CGImageDestinationCreateWithData( data , kUTTypeTIFF , 1 , NULL );
        if ( destination )
        {
            CGImageDestinationAddImage( destination, cgImageRef, NULL );
            CGImageDestinationFinalize( destination );
            CFRelease( destination );
        }
        m_pictHandle = NewHandle(CFDataGetLength(data));
        if ( m_pictHandle )
        {
            memcpy( *(Handle)m_pictHandle, (const char *)CFDataGetBytePtr(data), CFDataGetLength(data) );
        }
        CFRelease( data );

        CGImageRelease(cgImageRef);
    }
}

void wxBitmapDataObject::Init()
{
    m_pictHandle = NULL;
    m_pictCreated = false;
}

void wxBitmapDataObject::Clear()
{
    if (m_pictHandle != NULL)
    {
        DisposeHandle( (Handle) m_pictHandle );
        m_pictHandle = NULL;
    }
    m_pictCreated = false;
}

bool wxBitmapDataObject::GetDataHere( void *pBuf ) const
{
    if (m_pictHandle == NULL)
    {
        wxFAIL_MSG( wxT("attempt to copy empty bitmap failed") );
        return false;
    }

    if (pBuf == NULL)
        return false;

    memcpy( pBuf, *(Handle)m_pictHandle, GetHandleSize( (Handle)m_pictHandle ) );

    return true;
}

size_t wxBitmapDataObject::GetDataSize() const
{
    if (m_pictHandle != NULL)
        return GetHandleSize( (Handle)m_pictHandle );
    else
        return 0;
}

Handle MacCreateDataReferenceHandle(Handle theDataHandle)
{
    Handle  dataRef = NULL;
    OSErr   err     = noErr;

    // Create a data reference handle for our data.
    err = PtrToHand( &theDataHandle, &dataRef, sizeof(Handle));

    return dataRef;
}

bool wxBitmapDataObject::SetData( size_t nSize, const void *pBuf )
{
    Clear();

    if ((pBuf == NULL) || (nSize == 0))
        return false;

    Handle picHandle = NewHandle( nSize );
    memcpy( *picHandle, pBuf, nSize );
    m_pictHandle = picHandle;
    CGImageRef cgImageRef = 0;

    CFDataRef data = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault, (const UInt8*) pBuf, nSize, kCFAllocatorNull);
    CGImageSourceRef source = CGImageSourceCreateWithData( data, NULL );
    if ( source )
    {
        cgImageRef = CGImageSourceCreateImageAtIndex(source, 0, NULL);
        CFRelease( source );
    }
    CFRelease( data );

    if ( cgImageRef )
    {
        m_bitmap.Create( cgImageRef );
        CGImageRelease(cgImageRef);
        cgImageRef = NULL;
    }

    return m_bitmap.IsOk();
}

#endif
