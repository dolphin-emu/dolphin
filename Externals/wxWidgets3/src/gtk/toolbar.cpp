/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/toolbar.cpp
// Purpose:     GTK toolbar
// Author:      Robert Roebling
// Modified:    13.12.99 by VZ to derive from wxToolBarBase
// RCS-ID:      $Id: toolbar.cpp 66633 2011-01-07 18:15:21Z PC $
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_TOOLBAR_NATIVE

#include "wx/toolbar.h"

#include "wx/gtk/private.h"

// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

// data
extern bool       g_blockEventsOnDrag;
extern wxCursor   g_globalCursor;

// ----------------------------------------------------------------------------
// wxToolBarTool
// ----------------------------------------------------------------------------

class wxToolBarTool : public wxToolBarToolBase
{
public:
    wxToolBarTool(wxToolBar *tbar,
                  int id,
                  const wxString& label,
                  const wxBitmap& bitmap1,
                  const wxBitmap& bitmap2,
                  wxItemKind kind,
                  wxObject *clientData,
                  const wxString& shortHelpString,
                  const wxString& longHelpString)
        : wxToolBarToolBase(tbar, id, label, bitmap1, bitmap2, kind,
                            clientData, shortHelpString, longHelpString)
    {
        m_item = NULL;
    }

    wxToolBarTool(wxToolBar *tbar, wxControl *control, const wxString& label)
        : wxToolBarToolBase(tbar, control, label)
    {
        m_item = NULL;
    }

    void SetImage();
    void CreateDropDown();
    void ShowDropdown(GtkToggleButton* button);

    GtkToolItem* m_item;
};

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxToolBar, wxControl)

// ============================================================================
// implementation
// ============================================================================

//-----------------------------------------------------------------------------
// "clicked" from m_item
//-----------------------------------------------------------------------------

extern "C" {
static void item_clicked(GtkToolButton*, wxToolBarTool* tool)
{
    if (g_blockEventsOnDrag) return;

    tool->GetToolBar()->OnLeftClick(tool->GetId(), false);
}
}

//-----------------------------------------------------------------------------
// "toggled" from m_item
//-----------------------------------------------------------------------------

extern "C" {
static void item_toggled(GtkToggleToolButton* button, wxToolBarTool* tool)
{
    if (g_blockEventsOnDrag) return;

    const bool active = gtk_toggle_tool_button_get_active(button) != 0;
    tool->Toggle(active);
    if (!active && tool->GetKind() == wxITEM_RADIO)
        return;

    if (!tool->GetToolBar()->OnLeftClick(tool->GetId(), active))
    {
        // revert back
        tool->Toggle();
    }
}
}

//-----------------------------------------------------------------------------
// "button_press_event" from m_item child
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
button_press_event(GtkWidget*, GdkEventButton* event, wxToolBarTool* tool)
{
    if (event->button != 3)
        return FALSE;

    if (g_blockEventsOnDrag) return TRUE;

    tool->GetToolBar()->OnRightClick(
        tool->GetId(), int(event->x), int(event->y));

    return TRUE;
}
}

//-----------------------------------------------------------------------------
// "child_detached" from m_widget
//-----------------------------------------------------------------------------

extern "C" {
static void child_detached(GtkWidget*, GtkToolbar* toolbar, void*)
{
    // disable showing overflow arrow when toolbar is detached,
    // otherwise toolbar collapses to just an arrow
    gtk_toolbar_set_show_arrow(toolbar, false);
}
}

//-----------------------------------------------------------------------------
// "child_attached" from m_widget
//-----------------------------------------------------------------------------

extern "C" {
static void child_attached(GtkWidget*, GtkToolbar* toolbar, void*)
{
    gtk_toolbar_set_show_arrow(toolbar, true);
}
}

//-----------------------------------------------------------------------------
// "enter_notify_event" / "leave_notify_event" from m_item
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
enter_notify_event(GtkWidget*, GdkEventCrossing* event, wxToolBarTool* tool)
{
    if (g_blockEventsOnDrag) return TRUE;

    int id = -1;
    if (event->type == GDK_ENTER_NOTIFY)
        id = tool->GetId();
    tool->GetToolBar()->OnMouseEnter(id);

    return FALSE;
}
}

//-----------------------------------------------------------------------------
// "size_request" from m_toolbar
//-----------------------------------------------------------------------------

extern "C" {
static void
size_request(GtkWidget*, GtkRequisition* req, wxToolBar* win)
{
    const wxSize margins = win->GetMargins();
    req->width  += margins.x;
    req->height += 2 * margins.y;
}
}

//-----------------------------------------------------------------------------
// "expose_event" from GtkImage inside m_item
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
image_expose_event(GtkWidget* widget, GdkEventExpose*, wxToolBarTool* tool)
{
    const wxBitmap& bitmap = tool->GetDisabledBitmap();
    if (tool->IsEnabled() || !bitmap.IsOk())
        return false;

    // draw disabled bitmap ourselves, GtkImage has no way to specify it
    const GtkAllocation& alloc = widget->allocation;
    gdk_draw_pixbuf(
        widget->window, widget->style->black_gc, bitmap.GetPixbuf(),
        0, 0,
        alloc.x + (alloc.width - widget->requisition.width) / 2,
        alloc.y + (alloc.height - widget->requisition.height) / 2,
        -1, -1, GDK_RGB_DITHER_NORMAL, 0, 0);
    return true;
}
}

//-----------------------------------------------------------------------------
// "toggled" from dropdown menu button
//-----------------------------------------------------------------------------

extern "C" {
static void arrow_toggled(GtkToggleButton* button, wxToolBarTool* tool)
{
    if (gtk_toggle_button_get_active(button))
    {
        tool->ShowDropdown(button);
        gtk_toggle_button_set_active(button, false);
    }
}
}

//-----------------------------------------------------------------------------
// "button_press_event" from dropdown menu button
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
arrow_button_press_event(GtkToggleButton* button, GdkEventButton* event, wxToolBarTool* tool)
{
    if (event->button == 1)
    {
        g_signal_handlers_block_by_func(button, (void*)arrow_toggled, tool);
        gtk_toggle_button_set_active(button, true);
        tool->ShowDropdown(button);
        gtk_toggle_button_set_active(button, false);
        g_signal_handlers_unblock_by_func(button, (void*)arrow_toggled, tool);
        return true;
    }
    return false;
}
}

void wxToolBar::AddChildGTK(wxWindowGTK* child)
{
    GtkWidget* align = gtk_alignment_new(0.5, 0.5, 0, 0);
    gtk_widget_show(align);
    gtk_container_add(GTK_CONTAINER(align), child->m_widget);
    GtkToolItem* item = gtk_tool_item_new();
    gtk_container_add(GTK_CONTAINER(item), align);
    // position will be corrected in DoInsertTool if necessary
    gtk_toolbar_insert(GTK_TOOLBAR(GTK_BIN(m_widget)->child), item, -1);
}

// ----------------------------------------------------------------------------
// wxToolBarTool
// ----------------------------------------------------------------------------

void wxToolBarTool::SetImage()
{
    const wxBitmap& bitmap = GetNormalBitmap();
    wxCHECK_RET(bitmap.IsOk(), "invalid bitmap for wxToolBar icon");

    GtkWidget* image = gtk_tool_button_get_icon_widget(GTK_TOOL_BUTTON(m_item));
    // always use pixbuf, because pixmap mask does not
    // work with disabled images in some themes
    gtk_image_set_from_pixbuf(GTK_IMAGE(image), bitmap.GetPixbuf());
}

// helper to create a dropdown menu item
void wxToolBarTool::CreateDropDown()
{
    gtk_tool_item_set_homogeneous(m_item, false);
    GtkWidget* box;
    GtkWidget* arrow;
    if (GetToolBar()->HasFlag(wxTB_LEFT | wxTB_RIGHT))
    {
        box = gtk_vbox_new(false, 0);
        arrow = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
    }
    else
    {
        box = gtk_hbox_new(false, 0);
        arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
    }
    GtkWidget* tool_button = GTK_BIN(m_item)->child;
    gtk_widget_reparent(tool_button, box);
    GtkWidget* arrow_button = gtk_toggle_button_new();
    gtk_button_set_relief(GTK_BUTTON(arrow_button),
        gtk_tool_item_get_relief_style(GTK_TOOL_ITEM(m_item)));
    gtk_container_add(GTK_CONTAINER(arrow_button), arrow);
    gtk_container_add(GTK_CONTAINER(box), arrow_button);
    gtk_widget_show_all(box);
    gtk_container_add(GTK_CONTAINER(m_item), box);

    g_signal_connect(arrow_button, "toggled", G_CALLBACK(arrow_toggled), this);
    g_signal_connect(arrow_button, "button_press_event",
        G_CALLBACK(arrow_button_press_event), this);
}

void wxToolBarTool::ShowDropdown(GtkToggleButton* button)
{
    wxToolBarBase* toolbar = GetToolBar();
    wxCommandEvent event(wxEVT_COMMAND_TOOL_DROPDOWN_CLICKED, GetId());
    if (!toolbar->HandleWindowEvent(event))
    {
        wxMenu* menu = GetDropdownMenu();
        if (menu)
        {
            const GtkAllocation& alloc = GTK_WIDGET(button)->allocation;
            int x = alloc.x;
            int y = alloc.y;
            if (toolbar->HasFlag(wxTB_LEFT | wxTB_RIGHT))
                x += alloc.width;
            else
                y += alloc.height;
            toolbar->PopupMenu(menu, x, y);
        }
    }
}

wxToolBarToolBase *wxToolBar::CreateTool(int id,
                                         const wxString& text,
                                         const wxBitmap& bitmap1,
                                         const wxBitmap& bitmap2,
                                         wxItemKind kind,
                                         wxObject *clientData,
                                         const wxString& shortHelpString,
                                         const wxString& longHelpString)
{
    return new wxToolBarTool(this, id, text, bitmap1, bitmap2, kind,
                             clientData, shortHelpString, longHelpString);
}

wxToolBarToolBase *
wxToolBar::CreateTool(wxControl *control, const wxString& label)
{
    return new wxToolBarTool(this, control, label);
}

//-----------------------------------------------------------------------------
// wxToolBar construction
//-----------------------------------------------------------------------------

void wxToolBar::Init()
{
    m_toolbar = NULL;
    m_tooltips = NULL;
}

wxToolBar::~wxToolBar()
{
    if (m_tooltips) // always NULL if GTK >= 2.12
    {
        gtk_object_destroy(GTK_OBJECT(m_tooltips));
        g_object_unref(m_tooltips);
    }
}

bool wxToolBar::Create( wxWindow *parent,
                        wxWindowID id,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxString& name )
{
    if ( !PreCreation( parent, pos, size ) ||
         !CreateBase( parent, id, pos, size, style, wxDefaultValidator, name ))
    {
        wxFAIL_MSG( wxT("wxToolBar creation failed") );

        return false;
    }

    FixupStyle();

    m_toolbar = GTK_TOOLBAR( gtk_toolbar_new() );
    if (gtk_check_version(2, 12, 0))
    {
        m_tooltips = gtk_tooltips_new();
        g_object_ref(m_tooltips);
        gtk_object_sink(GTK_OBJECT(m_tooltips));
    }
    GtkSetStyle();

    if (style & wxTB_DOCKABLE)
    {
        m_widget = gtk_handle_box_new();

        g_signal_connect(m_widget, "child_detached",
            G_CALLBACK(child_detached), NULL);
        g_signal_connect(m_widget, "child_attached",
            G_CALLBACK(child_attached), NULL);

        if (style & wxTB_FLAT)
            gtk_handle_box_set_shadow_type( GTK_HANDLE_BOX(m_widget), GTK_SHADOW_NONE );
    }
    else
    {
        m_widget = gtk_event_box_new();
        ConnectWidget( m_widget );
    }
    g_object_ref(m_widget);
    gtk_container_add(GTK_CONTAINER(m_widget), GTK_WIDGET(m_toolbar));
    gtk_widget_show(GTK_WIDGET(m_toolbar));

    m_parent->DoAddChild( this );

    PostCreation(size);

    g_signal_connect_after(m_toolbar, "size_request",
        G_CALLBACK(size_request), this);

    return true;
}

GdkWindow *wxToolBar::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return GTK_WIDGET(m_toolbar)->window;
}

void wxToolBar::GtkSetStyle()
{
    GtkOrientation orient = GTK_ORIENTATION_HORIZONTAL;
    if (HasFlag(wxTB_LEFT | wxTB_RIGHT))
        orient = GTK_ORIENTATION_VERTICAL;

    GtkToolbarStyle style = GTK_TOOLBAR_ICONS;
    if (HasFlag(wxTB_NOICONS))
        style = GTK_TOOLBAR_TEXT;
    else if (HasFlag(wxTB_TEXT))
    {
        style = GTK_TOOLBAR_BOTH;
        if (HasFlag(wxTB_HORZ_LAYOUT))
            style = GTK_TOOLBAR_BOTH_HORIZ;
    }

    gtk_toolbar_set_orientation(m_toolbar, orient);
    gtk_toolbar_set_style(m_toolbar, style);
}

void wxToolBar::SetWindowStyleFlag( long style )
{
    wxToolBarBase::SetWindowStyleFlag(style);

    if ( m_toolbar )
        GtkSetStyle();
}

bool wxToolBar::Realize()
{
    if ( !wxToolBarBase::Realize() )
        return false;

    // bring the initial state of all the toolbar items in line with the
    // internal state if the latter was changed by calling wxToolBarTool::
    // Enable(): this works under MSW, where the toolbar items are only created
    // in Realize() which uses the internal state to determine the initial
    // button state, so make it work under GTK too
    for ( wxToolBarToolsList::const_iterator i = m_tools.begin();
          i != m_tools.end();
          ++i )
    {
        // by default the toolbar items are enabled and not toggled, so we only
        // have to do something if their internal state doesn't correspond to
        // this
        if ( !(*i)->IsEnabled() )
            DoEnableTool(*i, false);
        if ( (*i)->IsToggled() )
            DoToggleTool(*i, true);
    }

    return true;
}

bool wxToolBar::DoInsertTool(size_t pos, wxToolBarToolBase *toolBase)
{
    wxToolBarTool* tool = static_cast<wxToolBarTool*>(toolBase);

    GSList* radioGroup;
    switch ( tool->GetStyle() )
    {
        case wxTOOL_STYLE_BUTTON:
            switch (tool->GetKind())
            {
                case wxITEM_CHECK:
                    tool->m_item = gtk_toggle_tool_button_new();
                    g_signal_connect(tool->m_item, "toggled",
                        G_CALLBACK(item_toggled), tool);
                    break;
                case wxITEM_RADIO:
                    radioGroup = GetRadioGroup(pos);
                    if (radioGroup)
                    {
                        // this is the first button in the radio button group,
                        // it will be toggled automatically by GTK so bring the
                        // internal flag in sync
                        tool->Toggle(true);
                    }
                    tool->m_item = gtk_radio_tool_button_new(radioGroup);
                    g_signal_connect(tool->m_item, "toggled",
                        G_CALLBACK(item_toggled), tool);
                    break;
                default:
                    wxFAIL_MSG("unknown toolbar child type");
                    // fall through
                case wxITEM_DROPDOWN:
                case wxITEM_NORMAL:
                    tool->m_item = gtk_tool_button_new(NULL, "");
                    g_signal_connect(tool->m_item, "clicked",
                        G_CALLBACK(item_clicked), tool);
                    break;
            }
            if (!HasFlag(wxTB_NOICONS))
            {
                GtkWidget* image = gtk_image_new();
                gtk_tool_button_set_icon_widget(
                    GTK_TOOL_BUTTON(tool->m_item), image);
                tool->SetImage();
                gtk_widget_show(image);
                g_signal_connect(image, "expose_event",
                    G_CALLBACK(image_expose_event), tool);
            }
            if (!tool->GetLabel().empty())
            {
                gtk_tool_button_set_label(
                    GTK_TOOL_BUTTON(tool->m_item), wxGTK_CONV(tool->GetLabel()));
                // needed for labels in horizontal toolbar with wxTB_HORZ_LAYOUT
                gtk_tool_item_set_is_important(tool->m_item, true);
            }
            if (!HasFlag(wxTB_NO_TOOLTIPS) && !tool->GetShortHelp().empty())
            {
#if GTK_CHECK_VERSION(2, 12, 0)
                if (!gtk_check_version(2, 12, 0))
                {
                    gtk_tool_item_set_tooltip_text(tool->m_item,
                        wxGTK_CONV(tool->GetShortHelp()));
                }
                else
#endif
                {
                    gtk_tool_item_set_tooltip(tool->m_item,
                        m_tooltips, wxGTK_CONV(tool->GetShortHelp()), "");
                }
            }
            g_signal_connect(GTK_BIN(tool->m_item)->child, "button_press_event",
                G_CALLBACK(button_press_event), tool);
            g_signal_connect(tool->m_item, "enter_notify_event",
                G_CALLBACK(enter_notify_event), tool);
            g_signal_connect(tool->m_item, "leave_notify_event",
                G_CALLBACK(enter_notify_event), tool);

            if (tool->GetKind() == wxITEM_DROPDOWN)
                tool->CreateDropDown();
            gtk_toolbar_insert(m_toolbar, tool->m_item, int(pos));
            break;

        case wxTOOL_STYLE_SEPARATOR:
            tool->m_item = gtk_separator_tool_item_new();
            if ( tool->IsStretchable() )
            {
                gtk_separator_tool_item_set_draw
                (
                    GTK_SEPARATOR_TOOL_ITEM(tool->m_item),
                    FALSE
                );
                gtk_tool_item_set_expand(tool->m_item, TRUE);
            }
            gtk_toolbar_insert(m_toolbar, tool->m_item, int(pos));
            break;

        case wxTOOL_STYLE_CONTROL:
            wxWindow* control = tool->GetControl();
            if (control->m_widget->parent == NULL)
                AddChildGTK(control);
            tool->m_item = GTK_TOOL_ITEM(control->m_widget->parent->parent);
            if (gtk_toolbar_get_item_index(m_toolbar, tool->m_item) != int(pos))
            {
                g_object_ref(tool->m_item);
                gtk_container_remove(
                    GTK_CONTAINER(m_toolbar), GTK_WIDGET(tool->m_item));
                gtk_toolbar_insert(m_toolbar, tool->m_item, int(pos));
                g_object_unref(tool->m_item);
            }
            // Inserted items "slide" into place using an animated effect that
            // causes multiple size events on the item. Must set size request
            // to keep item size from getting permanently set too small by the
            // first of these size events.
            const wxSize size = control->GetSize();
            gtk_widget_set_size_request(control->m_widget, size.x, size.y);
            break;
    }
    gtk_widget_show(GTK_WIDGET(tool->m_item));

    InvalidateBestSize();

    return true;
}

bool wxToolBar::DoDeleteTool(size_t /* pos */, wxToolBarToolBase* toolBase)
{
    wxToolBarTool* tool = static_cast<wxToolBarTool*>(toolBase);

    if (tool->GetStyle() == wxTOOL_STYLE_CONTROL)
    {
        // don't destroy the control here as we can be called from
        // RemoveTool() and then we need to keep the control alive;
        // while if we're called from DeleteTool() the control will
        // be destroyed when wxToolBarToolBase itself is deleted
        GtkWidget* widget = tool->GetControl()->m_widget;
        gtk_container_remove(GTK_CONTAINER(widget->parent), widget);
    }
    gtk_object_destroy(GTK_OBJECT(tool->m_item));
    tool->m_item = NULL;

    InvalidateBestSize();
    return true;
}

GSList* wxToolBar::GetRadioGroup(size_t pos)
{
    GSList* radioGroup = NULL;
    GtkToolItem* item = NULL;
    if (pos > 0)
    {
        item = gtk_toolbar_get_nth_item(m_toolbar, int(pos) - 1);
        if (!GTK_IS_RADIO_TOOL_BUTTON(item))
            item = NULL;
    }
    if (item == NULL && pos < m_tools.size())
    {
        item = gtk_toolbar_get_nth_item(m_toolbar, int(pos));
        if (!GTK_IS_RADIO_TOOL_BUTTON(item))
            item = NULL;
    }
    if (item)
        radioGroup = gtk_radio_tool_button_get_group((GtkRadioToolButton*)item);
    return radioGroup;
}

// ----------------------------------------------------------------------------
// wxToolBar tools state
// ----------------------------------------------------------------------------

void wxToolBar::DoEnableTool(wxToolBarToolBase *toolBase, bool enable)
{
    wxToolBarTool* tool = static_cast<wxToolBarTool*>(toolBase);

    if (tool->m_item)
        gtk_widget_set_sensitive(GTK_WIDGET(tool->m_item), enable);
}

void wxToolBar::DoToggleTool( wxToolBarToolBase *toolBase, bool toggle )
{
    wxToolBarTool* tool = static_cast<wxToolBarTool*>(toolBase);

    if (tool->m_item)
    {
        g_signal_handlers_block_by_func(tool->m_item, (void*)item_toggled, tool);

        gtk_toggle_tool_button_set_active(
            GTK_TOGGLE_TOOL_BUTTON(tool->m_item), toggle);

        g_signal_handlers_unblock_by_func(tool->m_item, (void*)item_toggled, tool);
    }
}

void wxToolBar::DoSetToggle(wxToolBarToolBase * WXUNUSED(tool),
                            bool WXUNUSED(toggle))
{
    // VZ: absolutely no idea about how to do it
    wxFAIL_MSG( wxT("not implemented") );
}

// ----------------------------------------------------------------------------
// wxToolBar geometry
// ----------------------------------------------------------------------------

wxSize wxToolBar::DoGetBestSize() const
{
    // Unfortunately, if overflow arrow is enabled GtkToolbar only reports size
    // of arrow. To get the real size, the arrow is temporarily disabled here.
    // This is gross, since it will cause a queue_resize, and could potentially
    // lead to an infinite loop. But there seems to be no alternative, short of
    // disabling the arrow entirely.
    gtk_toolbar_set_show_arrow(m_toolbar, false);
    const wxSize size = wxToolBarBase::DoGetBestSize();
    gtk_toolbar_set_show_arrow(m_toolbar, true);
    return size;
}

wxToolBarToolBase *wxToolBar::FindToolForPosition(wxCoord WXUNUSED(x),
                                                  wxCoord WXUNUSED(y)) const
{
    // VZ: GTK+ doesn't seem to have such thing
    wxFAIL_MSG( wxT("wxToolBar::FindToolForPosition() not implemented") );

    return NULL;
}

void wxToolBar::SetToolShortHelp( int id, const wxString& helpString )
{
    wxToolBarTool* tool = static_cast<wxToolBarTool*>(FindById(id));

    if ( tool )
    {
        (void)tool->SetShortHelp(helpString);
        if (tool->m_item)
        {
#if GTK_CHECK_VERSION(2, 12, 0)
            if (!gtk_check_version(2, 12, 0))
            {
                gtk_tool_item_set_tooltip_text(tool->m_item,
                    wxGTK_CONV(helpString));
            }
            else
#endif
            {
                gtk_tool_item_set_tooltip(tool->m_item,
                    m_tooltips, wxGTK_CONV(helpString), "");
            }
        }
    }
}

void wxToolBar::SetToolNormalBitmap( int id, const wxBitmap& bitmap )
{
    wxToolBarTool* tool = static_cast<wxToolBarTool*>(FindById(id));
    if ( tool )
    {
        wxCHECK_RET( tool->IsButton(), wxT("Can only set bitmap on button tools."));

        tool->SetNormalBitmap(bitmap);
        tool->SetImage();
    }
}

void wxToolBar::SetToolDisabledBitmap( int id, const wxBitmap& bitmap )
{
    wxToolBarTool* tool = static_cast<wxToolBarTool*>(FindById(id));
    if ( tool )
    {
        wxCHECK_RET( tool->IsButton(), wxT("Can only set bitmap on button tools."));

        tool->SetDisabledBitmap(bitmap);
    }
}

// ----------------------------------------------------------------------------

// static
wxVisualAttributes
wxToolBar::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_toolbar_new);
}

#endif // wxUSE_TOOLBAR_NATIVE
