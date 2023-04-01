/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cairo.h
// Purpose:     Cairo library
// Author:      Anthony Bretaudeau
// Created:     2007-08-25
// RCS-ID:      $Id: cairo.h 68935 2011-08-27 23:26:53Z RD $
// Copyright:   (c) Anthony Bretaudeau
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CAIRO_H_BASE_
#define _WX_CAIRO_H_BASE_

#if wxUSE_CAIRO

#include "wx/dynlib.h"
#include <cairo.h>

extern "C"
{
    
bool wxCairoInit();
void wxCairoCleanUp();

}

#endif // wxUSE_CAIRO

#endif // _WX_CAIRO_H_BASE_
