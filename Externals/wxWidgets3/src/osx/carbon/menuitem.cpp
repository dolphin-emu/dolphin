///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/menuitem.cpp
// Purpose:     wxMenuItem implementation
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: menuitem.cpp 61724 2009-08-21 10:41:26Z VZ $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/menuitem.h"
#include "wx/stockitem.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/menu.h"
#endif // WX_PRECOMP

#include "wx/osx/private.h"

// because on mac carbon everything is done through MenuRef APIs both implementation
// classes are in menu.cpp
