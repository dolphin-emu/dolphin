/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/statbmp.cpp
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:           wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_STATBMP

#include "wx/statbmp.h"

#include <gtk/gtk.h>

//-----------------------------------------------------------------------------
// wxStaticBitmap
//-----------------------------------------------------------------------------

wxStaticBitmap::wxStaticBitmap(void)
{
}

wxStaticBitmap::wxStaticBitmap( wxWindow *parent, wxWindowID id, const wxBitmap &bitmap,
      const wxPoint &pos, const wxSize &size,
      long style, const wxString &name )
{
    Create( parent, id, bitmap, pos, size, style, name );
}

bool wxStaticBitmap::Create( wxWindow *parent, wxWindowID id, const wxBitmap &bitmap,
                             const wxPoint &pos, const wxSize &size,
                             long style, const wxString &name )
{
    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, wxDefaultValidator, name ))
    {
        wxFAIL_MSG( wxT("wxStaticBitmap creation failed") );
        return false;
    }

    m_bitmap = bitmap;

    m_widget = gtk_image_new();
    g_object_ref(m_widget);

    if (bitmap.IsOk())
        SetBitmap(bitmap);

    PostCreation(size);
    m_parent->DoAddChild( this );

    return true;
}

void wxStaticBitmap::SetBitmap( const wxBitmap &bitmap )
{
    m_bitmap = bitmap;

    // always use pixbuf, because pixmap mask does not
    // work with disabled images in some themes
    if (m_bitmap.IsOk())
        gtk_image_set_from_pixbuf(GTK_IMAGE(m_widget), m_bitmap.GetPixbuf());
    else
        gtk_image_set_from_pixbuf(GTK_IMAGE(m_widget), NULL);

    InvalidateBestSize();
    SetSize(GetBestSize());
}

// static
wxVisualAttributes
wxStaticBitmap::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_image_new());
}

#endif // wxUSE_STATBMP

