/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/hyperlink.cpp
// Purpose:     Hyperlink control
// Author:      Francesco Montorsi
// Created:     14/2/2007
// Copyright:   (c) 2007 Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// --------------------------------------------------------------------------
// headers
// --------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_HYPERLINKCTRL && defined(__WXGTK210__) && !defined(__WXUNIVERSAL__)

#include "wx/hyperlink.h"

#ifndef WX_PRECOMP
#endif

#include <gtk/gtk.h>
#include "wx/gtk/private.h"

#ifdef __WXGTK3__
    #include "wx/gtk/private/object.h"
#endif

// ----------------------------------------------------------------------------
// local functions
// ----------------------------------------------------------------------------

static inline bool UseNative()
{
    // native gtk_link_button widget is only available in GTK+ 2.10 and later
#ifdef __WXGTK3__
    return true;
#else
    return !gtk_check_version(2, 10, 0);
#endif
}

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// "clicked"
// ----------------------------------------------------------------------------

#ifdef __WXGTK3__
extern "C" {
static gboolean activate_link(GtkWidget*, wxHyperlinkCtrl* win)
{
    win->SetVisited(true);
    win->SendEvent();
    return true;
}
}
#else
static GSList* gs_hyperlinkctrl_list;
extern "C" {
static void clicked_hook(GtkLinkButton* button, const char*, void*)
{
    for (GSList* p = gs_hyperlinkctrl_list; p; p = p->next)
    {
        wxHyperlinkCtrl* win = static_cast<wxHyperlinkCtrl*>(p->data);
        if (win->m_widget == (GtkWidget*)button)
        {
            win->SetVisited(true);
            win->SendEvent();
            return;
        }
    }
    gtk_link_button_set_uri_hook(NULL, NULL, NULL);
    GTK_BUTTON_GET_CLASS(button)->clicked(GTK_BUTTON(button));
    gtk_link_button_set_uri_hook(clicked_hook, NULL, NULL);
}
}
#endif

#ifdef __WXGTK3__

// Used to store GtkCssProviders we need to change the link colours with GTK+3.
class wxHyperlinkCtrlColData
{
public:
    wxHyperlinkCtrlColData() :
        m_normalLinkCssProvider(gtk_css_provider_new()),
        m_visitedLinkCssProvider(gtk_css_provider_new())
    {}

    wxGtkObject<GtkCssProvider> m_normalLinkCssProvider;
    wxGtkObject<GtkCssProvider> m_visitedLinkCssProvider;
};

#endif // __WXGTK3__

// ----------------------------------------------------------------------------
// wxHyperlinkCtrl
// ----------------------------------------------------------------------------

wxHyperlinkCtrl::wxHyperlinkCtrl()
{
}

wxHyperlinkCtrl::wxHyperlinkCtrl(wxWindow *parent,
                                 wxWindowID id,
                                 const wxString& label,
                                 const wxString& url,
                                 const wxPoint& pos,
                                 const wxSize& size,
                                 long style,
                                 const wxString& name)
{
    (void)Create(parent, id, label, url, pos, size, style, name);
}

wxHyperlinkCtrl::~wxHyperlinkCtrl()
{
#ifndef __WXGTK3__
    gs_hyperlinkctrl_list = g_slist_remove(gs_hyperlinkctrl_list, this);
#endif
}

bool wxHyperlinkCtrl::Create(wxWindow *parent, wxWindowID id,
    const wxString& label, const wxString& url, const wxPoint& pos,
    const wxSize& size, long style, const wxString& name)
{
    if ( UseNative() )
    {
        // do validation checks:
        CheckParams(label, url, style);

        if (!PreCreation( parent, pos, size ) ||
            !CreateBase( parent, id, pos, size, style, wxDefaultValidator, name ))
        {
            wxFAIL_MSG( wxT("wxHyperlinkCtrl creation failed") );
            return false;
        }

        m_widget = gtk_link_button_new("asdfsaf asdfdsaf asdfdsa");
        g_object_ref(m_widget);

        // alignment
        float x_alignment = 0.5;
        if (HasFlag(wxHL_ALIGN_LEFT))
            x_alignment = 0.0;
        else if (HasFlag(wxHL_ALIGN_RIGHT))
            x_alignment = 1.0;

        wxGCC_WARNING_SUPPRESS(deprecated-declarations)
        gtk_button_set_alignment(GTK_BUTTON(m_widget), x_alignment, 0.5);
        wxGCC_WARNING_RESTORE()

        // set to non empty strings both the url and the label
        SetURL(url.empty() ? label : url);
        SetLabel(label.empty() ? url : label);

#ifdef __WXGTK3__
        g_signal_connect(m_widget, "activate_link", G_CALLBACK(activate_link), this);
#else
        gs_hyperlinkctrl_list = g_slist_prepend(gs_hyperlinkctrl_list, this);
        gtk_link_button_set_uri_hook(clicked_hook, NULL, NULL);
#endif

        m_parent->DoAddChild( this );

        PostCreation(size);

        // wxWindowGTK will connect to the enter_notify and leave_notify GTK+ signals
        // thus overriding GTK+'s internal signal handlers which set the cursor of
        // the widget - thus we need to manually set it here:
        SetCursor(wxCursor(wxCURSOR_HAND));
    }
    else
        return wxGenericHyperlinkCtrl::Create(parent, id, label, url, pos, size, style, name);

    return true;
}

wxSize wxHyperlinkCtrl::DoGetBestSize() const
{
    if ( UseNative() )
        return wxControl::DoGetBestSize();
    return wxGenericHyperlinkCtrl::DoGetBestSize();
}

wxSize wxHyperlinkCtrl::DoGetBestClientSize() const
{
    if ( UseNative() )
        return wxControl::DoGetBestClientSize();
    return wxGenericHyperlinkCtrl::DoGetBestClientSize();
}

void wxHyperlinkCtrl::SetLabel(const wxString &label)
{
    if ( UseNative() )
    {
        wxControl::SetLabel(label);
        const wxString labelGTK = GTKConvertMnemonics(label);
        gtk_button_set_label(GTK_BUTTON(m_widget), wxGTK_CONV(labelGTK));
    }
    else
        wxGenericHyperlinkCtrl::SetLabel(label);
}

void wxHyperlinkCtrl::SetURL(const wxString &uri)
{
    if ( UseNative() )
        gtk_link_button_set_uri(GTK_LINK_BUTTON(m_widget), wxGTK_CONV(uri));
    else
        wxGenericHyperlinkCtrl::SetURL(uri);
}

wxString wxHyperlinkCtrl::GetURL() const
{
    if ( UseNative() )
    {
        const gchar *str = gtk_link_button_get_uri(GTK_LINK_BUTTON(m_widget));
        return wxString::FromUTF8(str);
    }

    return wxGenericHyperlinkCtrl::GetURL();
}

void wxHyperlinkCtrl::DoSetLinkColour(LinkKind linkKind, const wxColour& colour)
{
#ifdef __WXGTK3__
    if ( !m_colData )
        m_colData.reset(new wxHyperlinkCtrlColData());

    const char* cssProp = NULL;
    GtkCssProvider* cssProvider = NULL;
    switch ( linkKind )
    {
        case Link_Normal:
            cssProp = "link-color";
            cssProvider = m_colData->m_normalLinkCssProvider;
            break;

        case Link_Visited:
            cssProp = "visited-link-color";
            cssProvider = m_colData->m_visitedLinkCssProvider;
            break;
    }

    wxCHECK_RET( cssProvider, wxS("unknown link kind") );

    const GdkRGBA *col = colour;

    wxGtkString
        css(g_strdup_printf("* { %s: %s; }", cssProp, gdk_rgba_to_string(col)));
    ApplyCssStyle(cssProvider, css);
#else // !__WXGTK3__
    // simply do nothing: GTK+ does not allow us to change it :(
    wxUnusedVar(linkKind);
    wxUnusedVar(colour);
#endif
}

void wxHyperlinkCtrl::SetNormalColour(const wxColour &colour)
{
    if ( UseNative() )
        DoSetLinkColour(Link_Normal, colour);
    else
        wxGenericHyperlinkCtrl::SetNormalColour(colour);
}

wxColour wxHyperlinkCtrl::GetNormalColour() const
{
    wxColour ret;
    if ( UseNative() )
    {
#ifdef __WXGTK3__
        GdkRGBA *link_color = NULL;
        gtk_widget_style_get(m_widget, "link-color", &link_color, NULL);

        if ( link_color )
        {
            ret = wxColour(*link_color);
            gdk_rgba_free (link_color);
        }
#else // !__WXGTK3__
        GdkColor* link_color;
        GdkColor color = { 0, 0, 0, 0xeeee };

        GtkWidget* widget = gtk_bin_get_child(GTK_BIN(m_widget));
        wxGCC_WARNING_SUPPRESS(deprecated-declarations)
        gtk_widget_ensure_style(widget);
        gtk_widget_style_get(widget, "link-color", &link_color, NULL);
        if (link_color)
        {
            color = *link_color;
            gdk_color_free(link_color);
        }
        wxGCC_WARNING_RESTORE()
        ret = wxColour(color);
#endif // __WXGTK3__/!__WXGTK3__
    }
    else
        ret = wxGenericHyperlinkCtrl::GetNormalColour();

    return ret;
}

void wxHyperlinkCtrl::SetVisitedColour(const wxColour &colour)
{
    if ( UseNative() )
    {
        DoSetLinkColour(Link_Visited, colour);
    }
    else
        wxGenericHyperlinkCtrl::SetVisitedColour(colour);
}

wxColour wxHyperlinkCtrl::GetVisitedColour() const
{
    wxColour ret;
    if ( UseNative() )
    {
#ifdef __WXGTK3__
        GdkRGBA *link_color = NULL;
        gtk_widget_style_get(m_widget, "visited-link-color", &link_color, NULL);

        if ( link_color )
        {
            ret = wxColour(*link_color);
            gdk_rgba_free (link_color);
        }
#else // !__WXGTK3__
        GdkColor* link_color;
        GdkColor color = { 0, 0x5555, 0x1a1a, 0x8b8b };

        GtkWidget* widget = gtk_bin_get_child(GTK_BIN(m_widget));
        wxGCC_WARNING_SUPPRESS(deprecated-declarations)
        gtk_widget_ensure_style(widget);
        gtk_widget_style_get(widget, "visited-link-color", &link_color, NULL);
        if (link_color)
        {
            color = *link_color;
            gdk_color_free(link_color);
        }
        wxGCC_WARNING_RESTORE()
        ret = wxColour(color);
#endif // __WXGTK3__/!__WXGTK3__
    }
    else
        ret = base_type::GetVisitedColour();

    return ret;
}

void wxHyperlinkCtrl::SetVisited(bool visited)
{
    base_type::SetVisited(visited);
#if GTK_CHECK_VERSION(2,14,0)
#ifndef __WXGTK3__
    if (gtk_check_version(2,14,0) == NULL)
#endif
    {
        gtk_link_button_set_visited(GTK_LINK_BUTTON(m_widget), visited);
    }
#endif
}

bool wxHyperlinkCtrl::GetVisited() const
{
#if GTK_CHECK_VERSION(2,14,0)
#ifndef __WXGTK3__
    if (gtk_check_version(2,14,0) == NULL)
#endif
    {
        return gtk_link_button_get_visited(GTK_LINK_BUTTON(m_widget)) != 0;
    }
#endif
    return base_type::GetVisited();
}

void wxHyperlinkCtrl::SetHoverColour(const wxColour &colour)
{
    if ( UseNative() )
    {
        // simply do nothing: GTK+ does not allow us to change it :(
    }
    else
        wxGenericHyperlinkCtrl::SetHoverColour(colour);
}

wxColour wxHyperlinkCtrl::GetHoverColour() const
{
    if ( UseNative() )
    {
        // hover colour == normal colour for native GTK+ widget
        return GetNormalColour();
    }

    return wxGenericHyperlinkCtrl::GetHoverColour();
}

GdkWindow *wxHyperlinkCtrl::GTKGetWindow(wxArrayGdkWindows& windows) const
{
    return UseNative() ? gtk_button_get_event_window(GTK_BUTTON(m_widget))
                       : wxGenericHyperlinkCtrl::GTKGetWindow(windows);
}

#endif // wxUSE_HYPERLINKCTRL && GTK+ 2.10+
