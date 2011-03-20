///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/menuitem.h
// Purpose:     wxMenuItem class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     11.11.97
// RCS-ID:      $Id: menuitem.h 66904 2011-02-16 16:37:24Z SC $
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef   _MENUITEM_H
#define   _MENUITEM_H

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"
#include "wx/bitmap.h"

// ----------------------------------------------------------------------------
// wxMenuItem: an item in the menu, optionally implements owner-drawn behaviour
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxMenuItemImpl ;

class WXDLLIMPEXP_CORE wxMenuItem: public wxMenuItemBase
{
public:
    // ctor & dtor
    wxMenuItem(wxMenu *parentMenu = NULL,
               int id = wxID_SEPARATOR,
               const wxString& name = wxEmptyString,
               const wxString& help = wxEmptyString,
               wxItemKind kind = wxITEM_NORMAL,
               wxMenu *subMenu = NULL);
    virtual ~wxMenuItem();

    // override base class virtuals
    virtual void SetItemLabel(const wxString& strName);

    virtual void Enable(bool bDoEnable = true);
    virtual void Check(bool bDoCheck = true);

    virtual void SetBitmap(const wxBitmap& bitmap) ;
    virtual const wxBitmap& GetBitmap() const { return m_bitmap; }

    // update the os specific representation
    void UpdateItemBitmap() ;
    void UpdateItemText() ;
    void UpdateItemStatus() ;

    // mark item as belonging to the given radio group
    void SetAsRadioGroupStart();
    void SetRadioGroupStart(int start);
    void SetRadioGroupEnd(int end);

    wxMenuItemImpl* GetPeer() { return m_peer; }
private:
    void UncheckRadio() ;

    // the positions of the first and last items of the radio group this item
    // belongs to or -1: start is the radio group start and is valid for all
    // but first radio group items (m_isRadioGroupStart == FALSE), end is valid
    // only for the first one
    union
    {
        int start;
        int end;
    } m_radioGroup;

    // does this item start a radio group?
    bool m_isRadioGroupStart;

    wxBitmap  m_bitmap; // Bitmap for menuitem, if any

    wxMenuItemImpl* m_peer;

    DECLARE_DYNAMIC_CLASS(wxMenuItem)
};

#endif  //_MENUITEM_H
