/* ///////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/win_gtk.h
// Purpose:     native GTK+ widget for wxWindow
// Author:      Robert Roebling
// Id:          $Id: win_gtk.h 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////// */

#ifndef _WX_GTK_PIZZA_H_
#define _WX_GTK_PIZZA_H_

#include <gtk/gtk.h>

#define WX_PIZZA(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, wxPizza::type(), wxPizza)
#define WX_IS_PIZZA(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, wxPizza::type())

struct WXDLLIMPEXP_CORE wxPizza
{
    // borders styles which can be used with wxPizza
    enum { BORDER_STYLES =
        wxBORDER_SIMPLE | wxBORDER_RAISED | wxBORDER_SUNKEN | wxBORDER_THEME };

    static GtkWidget* New(long windowStyle = 0);
    static GType type();
    void move(GtkWidget* widget, int x, int y);
    void put(GtkWidget* widget, int x, int y);
    void scroll(int dx, int dy);
    void get_border_widths(int& x, int& y);

    GtkFixed m_fixed;
    int m_scroll_x;
    int m_scroll_y;
    int m_border_style;
    bool m_is_scrollable;
};

#endif // _WX_GTK_PIZZA_H_
