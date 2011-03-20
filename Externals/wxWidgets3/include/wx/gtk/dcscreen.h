/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dcscreen.h
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: dcscreen.h 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKDCSCREEN_H_
#define _WX_GTKDCSCREEN_H_

#include "wx/dcscreen.h"
#include "wx/gtk/dcclient.h"

//-----------------------------------------------------------------------------
// wxScreenDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxScreenDCImpl : public wxWindowDCImpl
{
public:
    wxScreenDCImpl( wxScreenDC *owner );
    ~wxScreenDCImpl();

    virtual void DoGetSize(int *width, int *height) const;

protected:
    void Init();

    DECLARE_ABSTRACT_CLASS(wxScreenDCImpl)
};

#endif // _WX_GTKDCSCREEN_H_
