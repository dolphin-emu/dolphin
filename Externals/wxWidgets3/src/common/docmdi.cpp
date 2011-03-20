/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/docmdi.cpp
// Purpose:     Frame classes for MDI document/view applications
// Author:      Julian Smart, Vadim Zeitlin
// Created:     01/02/97
// RCS-ID:      $Id: docmdi.cpp 64295 2010-05-12 14:34:18Z VZ $
// Copyright:   (c) 1997 Julian Smart
//              (c) 2010 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
  #pragma hdrstop
#endif

#if wxUSE_MDI_ARCHITECTURE

#include "wx/docmdi.h"

IMPLEMENT_CLASS(wxDocMDIParentFrame, wxMDIParentFrame)
IMPLEMENT_CLASS(wxDocMDIChildFrame, wxMDIChildFrame)

#endif // wxUSE_DOC_VIEW_ARCHITECTURE

