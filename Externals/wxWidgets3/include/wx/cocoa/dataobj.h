///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/dataobj.h
// Purpose:     declaration of the wxDataObject
// Author:      David Elliott <dfe@cox.net>
// Modified by:
// Created:     2003/07/23
// RCS-ID:      $Id: dataobj.h 48095 2007-08-15 13:05:35Z VS $
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_DATAOBJ_H__
#define __WX_COCOA_DATAOBJ_H__

class WXDLLIMPEXP_CORE wxDataObject : public wxDataObjectBase
{
public:
    wxDataObject();
    virtual ~wxDataObject();
    virtual bool IsSupportedFormat(const wxDataFormat& format,
        Direction dir = Get) const;
};

#endif // __WX_COCOA_DATAOBJ_H__
