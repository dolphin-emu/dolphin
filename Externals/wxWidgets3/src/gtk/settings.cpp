/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/settings.cpp
// Purpose:
// Author:      Robert Roebling
// Modified by: Mart Raudsepp (GetMetric)
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/settings.h"

#ifndef WX_PRECOMP
    #include "wx/toplevel.h"
#endif

#include "wx/fontutil.h"
#include "wx/fontenum.h"

#include <gtk/gtk.h>
#include "wx/gtk/private/win_gtk.h"
#include "wx/gtk/private/gtk2-compat.h"

bool wxGetFrameExtents(GdkWindow* window, int* left, int* right, int* top, int* bottom);

// ----------------------------------------------------------------------------
// wxSystemSettings implementation
// ----------------------------------------------------------------------------

static wxFont gs_fontSystem;

static GtkContainer* ContainerWidget()
{
    static GtkContainer* s_widget;
    if (s_widget == NULL)
    {
        s_widget = GTK_CONTAINER(gtk_fixed_new());
        GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
#ifdef __WXGTK3__
        // need this to initialize style for window
        gtk_widget_get_style_context(GTK_WIDGET(window));
#endif
        gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(s_widget));
    }
    return s_widget;
}

extern "C" {
#ifdef __WXGTK3__
static void style_updated(GtkWidget*, void*)
#else
static void style_set(GtkWidget*, GtkStyle*, void*)
#endif
{
    gs_fontSystem = wxNullFont;
}
}

static GtkWidget* ButtonWidget()
{
    static GtkWidget* s_widget;
    if (s_widget == NULL)
    {
        s_widget = gtk_button_new();
        gtk_container_add(ContainerWidget(), s_widget);
#ifdef __WXGTK3__
        g_signal_connect(s_widget, "style_updated", G_CALLBACK(style_updated), NULL);
#else
        gtk_widget_ensure_style(s_widget);
        g_signal_connect(s_widget, "style_set", G_CALLBACK(style_set), NULL);
#endif
    }
    return s_widget;
}

static GtkWidget* ListWidget()
{
    static GtkWidget* s_widget;
    if (s_widget == NULL)
    {
        s_widget = gtk_tree_view_new_with_model(
            GTK_TREE_MODEL(gtk_list_store_new(1, G_TYPE_INT)));
        gtk_container_add(ContainerWidget(), s_widget);
#ifndef __WXGTK3__
        gtk_widget_ensure_style(s_widget);
#endif
    }
    return s_widget;
}

static GtkWidget* TextCtrlWidget()
{
    static GtkWidget* s_widget;
    if (s_widget == NULL)
    {
        s_widget = gtk_text_view_new();
        gtk_container_add(ContainerWidget(), s_widget);
#ifndef __WXGTK3__
        gtk_widget_ensure_style(s_widget);
#endif
    }
    return s_widget;
}

static GtkWidget* MenuItemWidget()
{
    static GtkWidget* s_widget;
    if (s_widget == NULL)
    {
        s_widget = gtk_menu_item_new();
        gtk_container_add(ContainerWidget(), s_widget);
#ifndef __WXGTK3__
        gtk_widget_ensure_style(s_widget);
#endif
    }
    return s_widget;
}

static GtkWidget* MenuBarWidget()
{
    static GtkWidget* s_widget;
    if (s_widget == NULL)
    {
        s_widget = gtk_menu_bar_new();
        gtk_container_add(ContainerWidget(), s_widget);
#ifndef __WXGTK3__
        gtk_widget_ensure_style(s_widget);
#endif
    }
    return s_widget;
}

static GtkWidget* ToolTipWidget()
{
    static GtkWidget* s_widget;
    if (s_widget == NULL)
    {
        s_widget = gtk_window_new(GTK_WINDOW_POPUP);
        const char* name = "gtk-tooltip";
#ifndef __WXGTK3__
        if (gtk_check_version(2, 11, 0))
            name = "gtk-tooltips";
#endif
        gtk_widget_set_name(s_widget, name);
#ifndef __WXGTK3__
        gtk_widget_ensure_style(s_widget);
#endif
    }
    return s_widget;
}

#ifdef __WXGTK3__
static void get_color(const char* name, GtkWidget* widget, GtkStateFlags state, GdkRGBA& gdkRGBA)
{
    GtkStyleContext* sc = gtk_widget_get_style_context(widget);
    GdkRGBA* rgba;
    gtk_style_context_set_state(sc, state);
    gtk_style_context_get(sc, state, name, &rgba, NULL);
    gdkRGBA = *rgba;
    gdk_rgba_free(rgba);
    if (gdkRGBA.alpha <= 0)
    {
        widget = gtk_widget_get_parent(GTK_WIDGET(ContainerWidget()));
        sc = gtk_widget_get_style_context(widget);
        gtk_style_context_set_state(sc, state);
        gtk_style_context_get(sc, state, name, &rgba, NULL);
        gdkRGBA = *rgba;
        gdk_rgba_free(rgba);
    }
}
static void bg(GtkWidget* widget, GtkStateFlags state, GdkRGBA& gdkRGBA)
{
    get_color("background-color", widget, state, gdkRGBA);
}
static void fg(GtkWidget* widget, GtkStateFlags state, GdkRGBA& gdkRGBA)
{
    get_color("color", widget, state, gdkRGBA);
}
static void border(GtkWidget* widget, GtkStateFlags state, GdkRGBA& gdkRGBA)
{
    get_color("border-color", widget, state, gdkRGBA);
}

wxColour wxSystemSettingsNative::GetColour(wxSystemColour index)
{
    GdkRGBA gdkRGBA = { 0, 0, 0, 1 };
    switch (index)
    {
    case wxSYS_COLOUR_3DLIGHT:
    case wxSYS_COLOUR_ACTIVEBORDER:
    case wxSYS_COLOUR_BTNFACE:
    case wxSYS_COLOUR_DESKTOP:
    case wxSYS_COLOUR_INACTIVEBORDER:
    case wxSYS_COLOUR_INACTIVECAPTION:
    case wxSYS_COLOUR_SCROLLBAR:
    case wxSYS_COLOUR_WINDOWFRAME:
        bg(ButtonWidget(), GTK_STATE_FLAG_NORMAL, gdkRGBA);
        break;
    case wxSYS_COLOUR_BTNHIGHLIGHT:
    case wxSYS_COLOUR_HIGHLIGHT:
        bg(ButtonWidget(), GTK_STATE_FLAG_SELECTED, gdkRGBA);
        break;
    case wxSYS_COLOUR_BTNSHADOW:
        border(ButtonWidget(), GTK_STATE_FLAG_NORMAL, gdkRGBA);
        break;
    case wxSYS_COLOUR_BTNTEXT:
    case wxSYS_COLOUR_WINDOWTEXT:
        fg(ButtonWidget(), GTK_STATE_FLAG_NORMAL, gdkRGBA);
        break;
    case wxSYS_COLOUR_GRAYTEXT:
    case wxSYS_COLOUR_INACTIVECAPTIONTEXT:
        fg(ButtonWidget(), GTK_STATE_FLAG_INSENSITIVE, gdkRGBA);
        break;
    case wxSYS_COLOUR_HIGHLIGHTTEXT:
        fg(ButtonWidget(), GTK_STATE_FLAG_SELECTED, gdkRGBA);
        break;
    case wxSYS_COLOUR_HOTLIGHT:
        {
            static GtkWidget* s_widget;
            if (s_widget == NULL)
            {
                s_widget = gtk_link_button_new("");
                gtk_container_add(ContainerWidget(), s_widget);
            }
            fg(s_widget, GTK_STATE_FLAG_NORMAL, gdkRGBA);
        }
        break;
    case wxSYS_COLOUR_INFOBK:
        bg(ToolTipWidget(), GTK_STATE_FLAG_NORMAL, gdkRGBA);
        break;
    case wxSYS_COLOUR_INFOTEXT:
        fg(ToolTipWidget(), GTK_STATE_FLAG_NORMAL, gdkRGBA);
        break;
    case wxSYS_COLOUR_LISTBOX:
        bg(ListWidget(), GTK_STATE_FLAG_NORMAL, gdkRGBA);
        break;
    case wxSYS_COLOUR_LISTBOXHIGHLIGHTTEXT:
        fg(ListWidget(), GTK_STATE_FLAG_SELECTED, gdkRGBA);
        break;
    case wxSYS_COLOUR_LISTBOXTEXT:
        fg(ListWidget(), GTK_STATE_FLAG_NORMAL, gdkRGBA);
        break;
    case wxSYS_COLOUR_MENU:
        bg(MenuItemWidget(), GTK_STATE_FLAG_NORMAL, gdkRGBA);
        break;
    case wxSYS_COLOUR_MENUBAR:
        bg(MenuBarWidget(), GTK_STATE_FLAG_NORMAL, gdkRGBA);
        break;
    case wxSYS_COLOUR_ACTIVECAPTION:
    case wxSYS_COLOUR_MENUHILIGHT:
        bg(MenuItemWidget(), GTK_STATE_FLAG_PRELIGHT, gdkRGBA);
        break;
    case wxSYS_COLOUR_MENUTEXT:
        fg(MenuItemWidget(), GTK_STATE_FLAG_NORMAL, gdkRGBA);
        break;
    case wxSYS_COLOUR_APPWORKSPACE:
    case wxSYS_COLOUR_WINDOW:
        {
            GtkWidget* widget = TextCtrlWidget();
            GtkStyleContext* sc = gtk_widget_get_style_context(widget);
            gtk_style_context_save(sc);
            gtk_style_context_add_class(sc, GTK_STYLE_CLASS_VIEW);
            bg(widget, GTK_STATE_FLAG_NORMAL, gdkRGBA);
            gtk_style_context_restore(sc);
        }
        break;
    case wxSYS_COLOUR_CAPTIONTEXT:
        {
            GdkRGBA c = { 1, 1, 1, 1 };
            gdkRGBA = c;
        }
        break;
    default:
        wxFAIL_MSG("unknown system colour index");
        // fallthrough
    case wxSYS_COLOUR_3DDKSHADOW:
    case wxSYS_COLOUR_GRADIENTACTIVECAPTION:
    case wxSYS_COLOUR_GRADIENTINACTIVECAPTION:
        // black
        break;
    }
    return wxColour(gdkRGBA);
}
#else
static const GtkStyle* ButtonStyle()
{
    return gtk_widget_get_style(ButtonWidget());
}

static const GtkStyle* ListStyle()
{
    return gtk_widget_get_style(ListWidget());
}

static const GtkStyle* TextCtrlStyle()
{
    return gtk_widget_get_style(TextCtrlWidget());
}

static const GtkStyle* MenuItemStyle()
{
    return gtk_widget_get_style(MenuItemWidget());
}

static const GtkStyle* MenuBarStyle()
{
    return gtk_widget_get_style(MenuBarWidget());
}

static const GtkStyle* ToolTipStyle()
{
    return gtk_widget_get_style(ToolTipWidget());
}

wxColour wxSystemSettingsNative::GetColour( wxSystemColour index )
{
    wxColor color;
    switch (index)
    {
        case wxSYS_COLOUR_SCROLLBAR:
        case wxSYS_COLOUR_BACKGROUND:
        //case wxSYS_COLOUR_DESKTOP:
        case wxSYS_COLOUR_INACTIVECAPTION:
        case wxSYS_COLOUR_MENU:
        case wxSYS_COLOUR_WINDOWFRAME:
        case wxSYS_COLOUR_ACTIVEBORDER:
        case wxSYS_COLOUR_INACTIVEBORDER:
        case wxSYS_COLOUR_BTNFACE:
        //case wxSYS_COLOUR_3DFACE:
        case wxSYS_COLOUR_3DLIGHT:
            color = wxColor(ButtonStyle()->bg[GTK_STATE_NORMAL]);
            break;

        case wxSYS_COLOUR_WINDOW:
            color = wxColor(TextCtrlStyle()->base[GTK_STATE_NORMAL]);
            break;

        case wxSYS_COLOUR_MENUBAR:
            color = wxColor(MenuBarStyle()->bg[GTK_STATE_NORMAL]);
            break;

        case wxSYS_COLOUR_3DDKSHADOW:
            color = *wxBLACK;
            break;

        case wxSYS_COLOUR_GRAYTEXT:
        case wxSYS_COLOUR_BTNSHADOW:
        //case wxSYS_COLOUR_3DSHADOW:
            {
                wxColour faceColour(GetColour(wxSYS_COLOUR_3DFACE));
                color =
                   wxColour((unsigned char) (faceColour.Red() * 2 / 3),
                            (unsigned char) (faceColour.Green() * 2 / 3),
                            (unsigned char) (faceColour.Blue() * 2 / 3));
            }
            break;

        case wxSYS_COLOUR_BTNHIGHLIGHT:
        //case wxSYS_COLOUR_BTNHILIGHT:
        //case wxSYS_COLOUR_3DHIGHLIGHT:
        //case wxSYS_COLOUR_3DHILIGHT:
            color = *wxWHITE;
            break;

        case wxSYS_COLOUR_HIGHLIGHT:
            color = wxColor(ButtonStyle()->bg[GTK_STATE_SELECTED]);
            break;

        case wxSYS_COLOUR_LISTBOX:
            color = wxColor(ListStyle()->base[GTK_STATE_NORMAL]);
            break;

        case wxSYS_COLOUR_LISTBOXTEXT:
            color = wxColor(ListStyle()->text[GTK_STATE_NORMAL]);
            break;

        case wxSYS_COLOUR_LISTBOXHIGHLIGHTTEXT:
            // This is for the text in a list control (or tree) when the
            // item is selected, but not focused
            color = wxColor(ListStyle()->text[GTK_STATE_ACTIVE]);
            break;

        case wxSYS_COLOUR_MENUTEXT:
        case wxSYS_COLOUR_WINDOWTEXT:
        case wxSYS_COLOUR_CAPTIONTEXT:
        case wxSYS_COLOUR_INACTIVECAPTIONTEXT:
        case wxSYS_COLOUR_BTNTEXT:
            color = wxColor(ButtonStyle()->fg[GTK_STATE_NORMAL]);
            break;

        case wxSYS_COLOUR_INFOBK:
            color = wxColor(ToolTipStyle()->bg[GTK_STATE_NORMAL]);
            break;

        case wxSYS_COLOUR_INFOTEXT:
            color = wxColor(ToolTipStyle()->fg[GTK_STATE_NORMAL]);
            break;

        case wxSYS_COLOUR_HIGHLIGHTTEXT:
            color = wxColor(ButtonStyle()->fg[GTK_STATE_SELECTED]);
            break;

        case wxSYS_COLOUR_APPWORKSPACE:
            color = *wxWHITE;    // ?
            break;

        case wxSYS_COLOUR_ACTIVECAPTION:
        case wxSYS_COLOUR_MENUHILIGHT:
            color = wxColor(MenuItemStyle()->bg[GTK_STATE_SELECTED]);
            break;

        case wxSYS_COLOUR_HOTLIGHT:
        case wxSYS_COLOUR_GRADIENTACTIVECAPTION:
        case wxSYS_COLOUR_GRADIENTINACTIVECAPTION:
            // TODO
            color = *wxBLACK;
            break;

        case wxSYS_COLOUR_MAX:
        default:
            wxFAIL_MSG( wxT("unknown system colour index") );
            color = *wxWHITE;
            break;
    }

    wxASSERT(color.IsOk());
    return color;
}
#endif

wxFont wxSystemSettingsNative::GetFont( wxSystemFont index )
{
    wxFont font;
    switch (index)
    {
        case wxSYS_OEM_FIXED_FONT:
        case wxSYS_ANSI_FIXED_FONT:
        case wxSYS_SYSTEM_FIXED_FONT:
            font = *wxNORMAL_FONT;
            break;

        case wxSYS_ANSI_VAR_FONT:
        case wxSYS_SYSTEM_FONT:
        case wxSYS_DEVICE_DEFAULT_FONT:
        case wxSYS_DEFAULT_GUI_FONT:
            if (!gs_fontSystem.IsOk())
            {
                wxNativeFontInfo info;
#ifdef __WXGTK3__
                GtkStyleContext* sc = gtk_widget_get_style_context(ButtonWidget());
                gtk_style_context_set_state(sc, GTK_STATE_FLAG_NORMAL);
                gtk_style_context_get(sc, GTK_STATE_FLAG_NORMAL,
                    GTK_STYLE_PROPERTY_FONT, &info.description, NULL);
#else
                info.description = ButtonStyle()->font_desc;
#endif
                gs_fontSystem = wxFont(info);

#if wxUSE_FONTENUM
                // (try to) heal the default font (on some common systems e.g. Ubuntu
                // it's "Sans Serif" but the real font is called "Sans"):
                if (!wxFontEnumerator::IsValidFacename(gs_fontSystem.GetFaceName()) &&
                    gs_fontSystem.GetFaceName() == "Sans Serif")
                {
                    gs_fontSystem.SetFaceName("Sans");
                }
#endif // wxUSE_FONTENUM

#ifndef __WXGTK3__
                info.description = NULL;
#endif
            }
            font = gs_fontSystem;
            break;

        default:
            break;
    }

    wxASSERT( font.IsOk() );

    return font;
}

// helper: return the GtkSettings either for the screen the current window is
// on or for the default screen if window is NULL
static GtkSettings *GetSettingsForWindowScreen(GdkWindow *window)
{
    return window ? gtk_settings_get_for_screen(gdk_window_get_screen(window))
                  : gtk_settings_get_default();
}

static int GetBorderWidth(wxSystemMetric index, wxWindow* win)
{
    if (win->m_wxwindow)
    {
        wxPizza* pizza = WX_PIZZA(win->m_wxwindow);
        GtkBorder border;
        pizza->get_border(border);
        switch (index)
        {
            case wxSYS_BORDER_X:
            case wxSYS_EDGE_X:
            case wxSYS_FRAMESIZE_X:
                return border.left;
            default:
                return border.top;
        }
    }
    return -1;
}

int wxSystemSettingsNative::GetMetric( wxSystemMetric index, wxWindow* win )
{
    GdkWindow *window = NULL;
    if (win)
        window = gtk_widget_get_window(win->GetHandle());

    switch (index)
    {
        case wxSYS_BORDER_X:
        case wxSYS_BORDER_Y:
        case wxSYS_EDGE_X:
        case wxSYS_EDGE_Y:
        case wxSYS_FRAMESIZE_X:
        case wxSYS_FRAMESIZE_Y:
            if (win)
            {
                wxTopLevelWindow *tlw = wxDynamicCast(win, wxTopLevelWindow);
                if (!tlw)
                    return GetBorderWidth(index, win);
                else if (window)
                {
                    // Get the frame extents from the windowmanager.
                    // In most cases the top extent is the titlebar, so we use the bottom extent
                    // for the heights.
                    int right, bottom;
                    if (wxGetFrameExtents(window, NULL, &right, NULL, &bottom))
                    {
                        switch (index)
                        {
                            case wxSYS_BORDER_X:
                            case wxSYS_EDGE_X:
                            case wxSYS_FRAMESIZE_X:
                                return right; // width of right extent
                            default:
                                return bottom; // height of bottom extent
                        }
                    }
                }
            }

            return -1; // no window specified

        case wxSYS_CURSOR_X:
        case wxSYS_CURSOR_Y:
                return gdk_display_get_default_cursor_size(
                            window ? gdk_window_get_display(window)
                                   : gdk_display_get_default());

        case wxSYS_DCLICK_X:
        case wxSYS_DCLICK_Y:
            gint dclick_distance;
            g_object_get(GetSettingsForWindowScreen(window),
                            "gtk-double-click-distance", &dclick_distance, NULL);

            return dclick_distance * 2;

        case wxSYS_DCLICK_MSEC:
            gint dclick;
            g_object_get(GetSettingsForWindowScreen(window),
                            "gtk-double-click-time", &dclick, NULL);
            return dclick;

        case wxSYS_DRAG_X:
        case wxSYS_DRAG_Y:
            gint drag_threshold;
            g_object_get(GetSettingsForWindowScreen(window),
                            "gtk-dnd-drag-threshold", &drag_threshold, NULL);

            // The correct thing here would be to double the value
            // since that is what the API wants. But the values
            // are much bigger under GNOME than under Windows and
            // just seem to much in many cases to be useful.
            // drag_threshold *= 2;

            return drag_threshold;

        case wxSYS_ICON_X:
        case wxSYS_ICON_Y:
            return 32;

        case wxSYS_SCREEN_X:
            if (window)
                return gdk_screen_get_width(gdk_window_get_screen(window));
            else
                return gdk_screen_width();

        case wxSYS_SCREEN_Y:
            if (window)
                return gdk_screen_get_height(gdk_window_get_screen(window));
            else
                return gdk_screen_height();

        case wxSYS_HSCROLL_Y:
        case wxSYS_VSCROLL_X:
            return 15;

        case wxSYS_CAPTION_Y:
            if (!window)
                // No realized window specified, and no implementation for that case yet.
                return -1;

            wxASSERT_MSG( wxDynamicCast(win, wxTopLevelWindow),
                          wxT("Asking for caption height of a non toplevel window") );

            // Get the height of the top windowmanager border.
            // This is the titlebar in most cases. The titlebar might be elsewhere, and
            // we could check which is the thickest wm border to decide on which side the
            // titlebar is, but this might lead to interesting behaviours in used code.
            // Reconsider when we have a way to report to the user on which side it is.
            {
                int top;
                if (wxGetFrameExtents(window, NULL, NULL, &top, NULL))
                {
                    return top; // top frame extent
                }
            }

            // Try a default approach without a window pointer, if possible
            // ...

            return -1;

        case wxSYS_PENWINDOWS_PRESENT:
            // No MS Windows for Pen computing extension available in X11 based gtk+.
            return 0;

        default:
            return -1;   // metric is unknown
    }
}

bool wxSystemSettingsNative::HasFeature(wxSystemFeature index)
{
    switch (index)
    {
        case wxSYS_CAN_ICONIZE_FRAME:
            return false;

        case wxSYS_CAN_DRAW_FRAME_DECORATIONS:
            return true;

        default:
            return false;
    }
}
