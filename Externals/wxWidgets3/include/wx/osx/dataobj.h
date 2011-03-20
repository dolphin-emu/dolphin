///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/dataobj.h
// Purpose:     declaration of the wxDataObject
// Author:      Stefan Csomor (adapted from Robert Roebling's gtk port)
// Modified by:
// Created:     10/21/99
// RCS-ID:      $Id: dataobj.h 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) 1998, 1999 Vadim Zeitlin, Robert Roebling
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MAC_DATAOBJ_H_
#define _WX_MAC_DATAOBJ_H_

// ----------------------------------------------------------------------------
// wxDataObject is the same as wxDataObjectBase under wxGTK
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDataObject : public wxDataObjectBase
{
public:
    wxDataObject();
#ifdef __DARWIN__
    virtual ~wxDataObject() { }
#endif

    virtual bool IsSupportedFormat( const wxDataFormat& format, Direction dir = Get ) const;
    void AddToPasteboard( void * pasteboardRef , int itemID );
    // returns true if the passed in format is present in the pasteboard
    static bool IsFormatInPasteboard( void * pasteboardRef, const wxDataFormat &dataFormat );
    // returns true if any of the accepted formats of this dataobj is in the pasteboard
    bool HasDataInPasteboard( void * pasteboardRef );
    bool GetFromPasteboard( void * pasteboardRef );
};

#endif // _WX_MAC_DATAOBJ_H_

