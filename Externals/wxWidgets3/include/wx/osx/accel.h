/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/accel.h
// Purpose:     wxAcceleratorTable class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: accel.h 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ACCEL_H_
#define _WX_ACCEL_H_

#include "wx/string.h"
#include "wx/event.h"

class WXDLLIMPEXP_CORE wxAcceleratorTable: public wxObject
{
DECLARE_DYNAMIC_CLASS(wxAcceleratorTable)
public:
    wxAcceleratorTable();
    wxAcceleratorTable(int n, const wxAcceleratorEntry entries[]); // Load from array

    virtual ~wxAcceleratorTable();

    bool Ok() const { return IsOk(); }
    bool IsOk() const;

    int GetCommand( wxKeyEvent &event );
};

#endif
    // _WX_ACCEL_H_
