/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/bmpbuttn.h
// Purpose:     wxBitmapButton class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: bmpbuttn.h 61071 2009-06-15 23:10:16Z VZ $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BMPBUTTN_H_
#define _WX_BMPBUTTN_H_

#include "wx/button.h"
#include "wx/bitmap.h"
#include "wx/brush.h"

class WXDLLIMPEXP_CORE wxBitmapButton : public wxBitmapButtonBase
{
public:
    wxBitmapButton() { Init(); }

    wxBitmapButton(wxWindow *parent,
                   wxWindowID id,
                   const wxBitmap& bitmap,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = wxBU_AUTODRAW,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxButtonNameStr)
    {
        Init();

        Create(parent, id, bitmap, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxBitmap& bitmap,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxBU_AUTODRAW,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxButtonNameStr);

protected:
    // common part of all ctors
    void Init()
    {
        m_disabledSetByUser =
        m_hoverSetByUser = false;
    }

    // reimplement some base class virtuals
    virtual void DoSetBitmap(const wxBitmap& bitmap, State which);

    // true if disabled bitmap was set by user, false if we created it
    // ourselves from the normal one
    bool m_disabledSetByUser;

    // true if hover bitmap was set by user, false if it was set from focused
    // one
    bool m_hoverSetByUser;


    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxBitmapButton)
};

#endif // _WX_BMPBUTTN_H_
