/////////////////////////////////////////////////////////////////////////
// File:        src/gtk/taskbar.cpp
// Purpose:     wxTaskBarIcon
// Author:      Vaclav Slavik
// Modified by: Paul Cornett
// Created:     2004/05/29
// RCS-ID:      $Id: taskbar.cpp 58822 2009-02-10 03:43:30Z PC $
// Copyright:   (c) Vaclav Slavik, 2004
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_TASKBARICON

#include "wx/taskbar.h"

#ifndef WX_PRECOMP
    #include "wx/toplevel.h"
    #include "wx/menu.h"
    #include "wx/icon.h"
#endif

#include "eggtrayicon.h"
#include <gtk/gtk.h>

#if !GTK_CHECK_VERSION(2,10,0)
    typedef struct _GtkStatusIcon GtkStatusIcon;
#endif

class wxTaskBarIcon::Private
{
public:
    Private(wxTaskBarIcon* taskBarIcon);
    ~Private();
    void SetIcon();
    void size_allocate(int width, int height);

    // owning wxTaskBarIcon
    wxTaskBarIcon* m_taskBarIcon;
    // used when GTK+ >= 2.10
    GtkStatusIcon* m_statusIcon;
    // used when GTK+ < 2.10
    GtkWidget* m_eggTrayIcon;
    // for PopupMenu
    wxWindow* m_win;
    // for tooltip when GTK+ < 2.10
    GtkTooltips* m_tooltips;
    wxBitmap m_bitmap;
    wxString m_tipText;
    // width and height of available space, only used when GTK+ < 2.10
    int m_size;
};
//-----------------------------------------------------------------------------

extern "C" {
static void
icon_size_allocate(GtkWidget*, GtkAllocation* alloc, wxTaskBarIcon::Private* priv)
{
    priv->size_allocate(alloc->width, alloc->height);
}

static void
icon_destroy(GtkWidget*, wxTaskBarIcon::Private* priv)
{
    // Icon window destroyed, probably because tray program has died.
    // Recreate icon so it will appear if tray program is restarted.
    priv->m_eggTrayIcon = NULL;
    priv->SetIcon();
}

static void
icon_activate(void*, wxTaskBarIcon* taskBarIcon)
{
    // activate occurs from single click with GTK+
    wxTaskBarIconEvent event(wxEVT_TASKBAR_LEFT_DOWN, taskBarIcon);
    if (!taskBarIcon->SafelyProcessEvent(event))
    {
        // if single click not handled, send double click for compatibility
        event.SetEventType(wxEVT_TASKBAR_LEFT_DCLICK);
        taskBarIcon->SafelyProcessEvent(event);
    }
}

static gboolean
icon_popup_menu(GtkWidget*, wxTaskBarIcon* taskBarIcon)
{
    wxTaskBarIconEvent event(wxEVT_TASKBAR_CLICK, taskBarIcon);
    taskBarIcon->SafelyProcessEvent(event);
    return true;
}

static gboolean
icon_button_press_event(GtkWidget*, GdkEventButton* event, wxTaskBarIcon* taskBarIcon)
{
    if (event->type == GDK_BUTTON_PRESS)
    {
        if (event->button == 1)
            icon_activate(NULL, taskBarIcon);
        else if (event->button == 3)
            icon_popup_menu(NULL, taskBarIcon);
    }
    return false;
}

#if GTK_CHECK_VERSION(2,10,0)
static void
status_icon_popup_menu(GtkStatusIcon*, guint, guint, wxTaskBarIcon* taskBarIcon)
{
    icon_popup_menu(NULL, taskBarIcon);
}
#endif
} // extern "C"
//-----------------------------------------------------------------------------

bool wxTaskBarIconBase::IsAvailable()
{
    char name[32];
    g_snprintf(name, sizeof(name), "_NET_SYSTEM_TRAY_S%d",
        gdk_x11_get_default_screen());
    Atom atom = gdk_x11_get_xatom_by_name(name);

    Window manager = XGetSelectionOwner(gdk_x11_get_default_xdisplay(), atom);

    return manager != None;
}
//-----------------------------------------------------------------------------

wxTaskBarIcon::Private::Private(wxTaskBarIcon* taskBarIcon)
{
    m_taskBarIcon = taskBarIcon;
    m_statusIcon = NULL;
    m_eggTrayIcon = NULL;
    m_win = NULL;
    m_tooltips = NULL;
    m_size = 0;
}

wxTaskBarIcon::Private::~Private()
{
    if (m_statusIcon)
        g_object_unref(m_statusIcon);
    else if (m_eggTrayIcon)
    {
        g_signal_handlers_disconnect_by_func(m_eggTrayIcon, (void*)icon_destroy, this);
        gtk_widget_destroy(m_eggTrayIcon);
    }
    if (m_win)
    {
        m_win->PopEventHandler();
        m_win->Destroy();
    }
    if (m_tooltips)
    {
        gtk_object_destroy(GTK_OBJECT(m_tooltips));
        g_object_unref(m_tooltips);
    }
}

void wxTaskBarIcon::Private::SetIcon()
{
#if GTK_CHECK_VERSION(2,10,0)
    if (gtk_check_version(2,10,0) == NULL)
    {
        if (m_statusIcon)
            gtk_status_icon_set_from_pixbuf(m_statusIcon, m_bitmap.GetPixbuf());
        else
        {
            m_statusIcon = gtk_status_icon_new_from_pixbuf(m_bitmap.GetPixbuf());
            g_signal_connect(m_statusIcon, "activate",
                G_CALLBACK(icon_activate), m_taskBarIcon);
            g_signal_connect(m_statusIcon, "popup_menu",
                G_CALLBACK(status_icon_popup_menu), m_taskBarIcon);
        }
    }
    else
#endif
    {
        m_size = 0;
        if (m_eggTrayIcon)
        {
            GtkWidget* image = GTK_BIN(m_eggTrayIcon)->child;
            gtk_image_set_from_pixbuf(GTK_IMAGE(image), m_bitmap.GetPixbuf());
        }
        else
        {
            m_eggTrayIcon = GTK_WIDGET(egg_tray_icon_new("wxTaskBarIcon"));
            gtk_widget_add_events(m_eggTrayIcon, GDK_BUTTON_PRESS_MASK);
            g_signal_connect(m_eggTrayIcon, "size_allocate",
                G_CALLBACK(icon_size_allocate), this);
            g_signal_connect(m_eggTrayIcon, "destroy",
                G_CALLBACK(icon_destroy), this);
            g_signal_connect(m_eggTrayIcon, "button_press_event",
                G_CALLBACK(icon_button_press_event), m_taskBarIcon);
            g_signal_connect(m_eggTrayIcon, "popup_menu",
                G_CALLBACK(icon_popup_menu), m_taskBarIcon);
            GtkWidget* image = gtk_image_new_from_pixbuf(m_bitmap.GetPixbuf());
            gtk_container_add(GTK_CONTAINER(m_eggTrayIcon), image);
            gtk_widget_show_all(m_eggTrayIcon);
        }
    }
#if wxUSE_TOOLTIPS
    const char *tip_text = NULL;
    if (!m_tipText.empty())
        tip_text = m_tipText.c_str();

#if GTK_CHECK_VERSION(2,10,0)
    if (m_statusIcon)
        gtk_status_icon_set_tooltip(m_statusIcon, tip_text);
    else
#endif
    {
        if (tip_text && m_tooltips == NULL)
        {
            m_tooltips = gtk_tooltips_new();
            g_object_ref(m_tooltips);
            gtk_object_sink(GTK_OBJECT(m_tooltips));
        }
        if (m_tooltips)
            gtk_tooltips_set_tip(m_tooltips, m_eggTrayIcon, tip_text, "");
    }
#endif // wxUSE_TOOLTIPS
}

void wxTaskBarIcon::Private::size_allocate(int width, int height)
{
    int size = height;
    EggTrayIcon* icon = EGG_TRAY_ICON(m_eggTrayIcon);
    if (egg_tray_icon_get_orientation(icon) == GTK_ORIENTATION_VERTICAL)
        size = width;
    if (m_size == size)
        return;
    m_size = size;
    int w = m_bitmap.GetWidth();
    int h = m_bitmap.GetHeight();
    if (w > size || h > size)
    {
        if (w > size) w = size;
        if (h > size) h = size;
        GdkPixbuf* pixbuf =
            gdk_pixbuf_scale_simple(m_bitmap.GetPixbuf(), w, h, GDK_INTERP_BILINEAR);
        GtkImage* image = GTK_IMAGE(GTK_BIN(m_eggTrayIcon)->child);
        gtk_image_set_from_pixbuf(image, pixbuf);
        g_object_unref(pixbuf);
    }
}
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxTaskBarIcon, wxEvtHandler)

wxTaskBarIcon::wxTaskBarIcon()
{
    m_priv = new Private(this);
}

wxTaskBarIcon::~wxTaskBarIcon()
{
    delete m_priv;
}

bool wxTaskBarIcon::SetIcon(const wxIcon& icon, const wxString& tooltip)
{
    m_priv->m_bitmap = icon;
    m_priv->m_tipText = tooltip;
    m_priv->SetIcon();
    return true;
}

bool wxTaskBarIcon::RemoveIcon()
{
    delete m_priv;
    m_priv = new Private(this);
    return true;
}

bool wxTaskBarIcon::IsIconInstalled() const
{
    return m_priv->m_statusIcon || m_priv->m_eggTrayIcon;
}

bool wxTaskBarIcon::PopupMenu(wxMenu* menu)
{
#if wxUSE_MENUS
    if (m_priv->m_win == NULL)
    {
        m_priv->m_win = new wxTopLevelWindow(
            NULL, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize, 0);
        m_priv->m_win->PushEventHandler(this);
    }
    wxPoint point(-1, -1);
#ifdef __WXUNIVERSAL__
    point = wxGetMousePosition();
#endif
    m_priv->m_win->PopupMenu(menu, point);
#endif // wxUSE_MENUS
    return true;
}

#endif // wxUSE_TASKBARICON
