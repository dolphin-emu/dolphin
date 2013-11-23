/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/bmpbuttn.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_BMPBUTTON

#include "wx/bmpbuttn.h"

bool wxBitmapButton::Create(wxWindow *parent,
                            wxWindowID id,
                            const wxBitmap& bitmap,
                            const wxPoint& pos,
                            const wxSize& size,
                            long style,
                            const wxValidator& validator,
                            const wxString& name)
{
    if ( !wxBitmapButtonBase::Create(parent, id, pos, size, style,
                                     validator, name) )
        return false;

    if ( bitmap.IsOk() )
    {
        SetBitmapLabel(bitmap);

        // we need to adjust the size after setting the bitmap as it may be too
        // big for the default button size
        SetInitialSize(size);
    }

    return true;
}
#endif // wxUSE_BMPBUTTON
