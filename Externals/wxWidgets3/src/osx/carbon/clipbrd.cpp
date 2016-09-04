/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/clipbrd.cpp
// Purpose:     Clipboard functionality
// Author:      Stefan Csomor;
//              Generalized clipboard implementation by Matthew Flatt
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_CLIPBOARD

#include "wx/clipbrd.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/frame.h"
    #include "wx/bitmap.h"
#endif

#include "wx/metafile.h"

#include "wx/osx/private.h"

#define wxUSE_DATAOBJ 1

#include <string.h>

// the trace mask we use with wxLogTrace() - call
// wxLog::AddTraceMask(TRACE_CLIPBOARD) to enable the trace messages from here
// (there will be a *lot* of them!)
#define TRACE_CLIPBOARD wxT("clipboard")

wxIMPLEMENT_DYNAMIC_CLASS(wxClipboard, wxObject);

wxClipboard::wxClipboard()
{
    m_open = false;
    m_data = NULL ;
    PasteboardRef clipboard = 0;
    OSStatus err = PasteboardCreate( kPasteboardClipboard, &clipboard );
    if (err != noErr)
    {
        wxLogSysError( wxT("Failed to create the clipboard.") );
    }
    m_pasteboard.reset(clipboard);
}

wxClipboard::~wxClipboard()
{
    m_pasteboard.reset((PasteboardRef)0);
    delete m_data;
}

void wxClipboard::Clear()
{
    wxDELETE(m_data);

    wxCHECK_RET( m_pasteboard, "Clipboard creation failed." );

    OSStatus err = PasteboardClear( m_pasteboard );
    if (err != noErr)
    {
        wxLogSysError( wxT("Failed to empty the clipboard.") );
    }
}

bool wxClipboard::Flush()
{
    return false;
}

bool wxClipboard::Open()
{
    wxCHECK_MSG( !m_open, false, wxT("clipboard already open") );

    m_open = true;

    return true;
}

bool wxClipboard::IsOpened() const
{
    return m_open;
}

bool wxClipboard::SetData( wxDataObject *data )
{
    if ( IsUsingPrimarySelection() )
        return false;

    wxCHECK_MSG( m_open, false, wxT("clipboard not open") );
    wxCHECK_MSG( data, false, wxT("data is invalid") );

    Clear();

    // as we can only store one wxDataObject,
    // this is the same in this implementation
    return AddData( data );
}

bool wxClipboard::AddData( wxDataObject *data )
{
    if ( IsUsingPrimarySelection() )
        return false;

    wxCHECK_MSG( m_open, false, wxT("clipboard not open") );
    wxCHECK_MSG( data, false, wxT("data is invalid") );

    // we can only store one wxDataObject
    Clear();

    PasteboardSyncFlags syncFlags = PasteboardSynchronize( m_pasteboard );
    wxCHECK_MSG( !(syncFlags&kPasteboardModified), false, wxT("clipboard modified after clear") );
    wxCHECK_MSG( (syncFlags&kPasteboardClientIsOwner), false, wxT("client couldn't own clipboard") );

    m_data = data;

    data->AddToPasteboard( m_pasteboard, 1 );

    return true;
}

void wxClipboard::Close()
{
    wxCHECK_RET( m_open, wxT("clipboard not open") );

    m_open = false;

    // Get rid of cached object.
    // If this is not done, copying data from
    // another application will only work once
    wxDELETE(m_data);
}

bool wxClipboard::IsSupported( const wxDataFormat &dataFormat )
{
    wxLogTrace(TRACE_CLIPBOARD, wxT("Checking if format %s is available"),
               dataFormat.GetId().c_str());

    if ( m_data )
        return m_data->IsSupported( dataFormat );
    return wxDataObject::IsFormatInPasteboard( m_pasteboard, dataFormat );
}

bool wxClipboard::GetData( wxDataObject& data )
{
    if ( IsUsingPrimarySelection() )
        return false;

    wxCHECK_MSG( m_open, false, wxT("clipboard not open") );

    size_t formatcount = data.GetFormatCount(wxDataObject::Set) + 1;
    wxDataFormat *array = new wxDataFormat[ formatcount ];
    array[0] = data.GetPreferredFormat(wxDataObject::Set);
    data.GetAllFormats( &array[1], wxDataObject::Set );

    bool transferred = false;

    if ( m_data )
    {
        for (size_t i = 0; !transferred && i < formatcount; i++)
        {
            wxDataFormat format = array[ i ];
            if ( m_data->IsSupported( format ) )
            {
                int dataSize = m_data->GetDataSize( format );
                transferred = true;

                if (dataSize == 0)
                {
                    data.SetData( format, 0, 0 );
                }
                else
                {
                    char *d = new char[ dataSize ];
                    m_data->GetDataHere( format, (void*)d );
                    data.SetData( format, dataSize, d );
                    delete [] d;
                }
            }
        }
    }

    // get formats from wxDataObjects
    if ( !transferred )
    {
        transferred = data.GetFromPasteboard( m_pasteboard ) ;
    }

    delete [] array;

    return transferred;
}

#endif
