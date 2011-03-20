/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/gdiobj.cpp
// Purpose:     wxGDIObject class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: gdiobj.cpp 63917 2010-04-08 18:47:41Z SC $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/gdiobj.h"

#ifndef WX_PRECOMP
    #include "wx/gdicmn.h"
    #include "wx/module.h"
#endif

#include "wx/link.h"
#include "wx/osx/private.h"
#include "wx/font.h"

// Linker will discard entire object file without this
wxFORCE_LINK_THIS_MODULE(gdiobj)

class wxStockGDIMac: public wxStockGDI, public wxModule
{
public:
    virtual const wxFont* GetFont(Item item);

    virtual bool OnInit();
    virtual void OnExit();

private:
    typedef wxStockGDI super;
    DECLARE_DYNAMIC_CLASS(wxStockGDIMac)
};

IMPLEMENT_DYNAMIC_CLASS(wxStockGDIMac, wxModule)

bool wxStockGDIMac::OnInit()
{
    // Override default instance
    ms_instance = this;
    return true;
}

void wxStockGDIMac::OnExit()
{
}

const wxFont* wxStockGDIMac::GetFont(Item item)
{
    wxFont* font = static_cast<wxFont*>(ms_stockObject[item]);
    if (font == NULL)
    {
        switch (item)
        {
        case FONT_NORMAL:
            font = new wxFont(wxOSX_SYSTEM_FONT_NORMAL);
            break;
        case FONT_SMALL:
            font = new wxFont(wxOSX_SYSTEM_FONT_SMALL);
            break;
        default:
            font = const_cast<wxFont*>(super::GetFont(item));
            break;
        }
        ms_stockObject[item] = font;
    }
    return font;
}
