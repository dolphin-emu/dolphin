///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/win_gtk.cpp
// Purpose:     native GTK+ widget for wxWindow
// Author:      Paul Cornett
// Id:          $Id: win_gtk.cpp 64725 2010-06-25 04:31:45Z PC $
// Copyright:   (c) 2007 Paul Cornett
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/defs.h"
#include "wx/gtk/private.h"
#include "wx/gtk/private/win_gtk.h"

/*
wxPizza is a custom GTK+ widget derived from GtkFixed.  A custom widget
is needed to adapt GTK+ to wxWidgets needs in 3 areas: scrolling, window
borders, and RTL.

For scrolling, the "set_scroll_adjustments" signal is implemented
to make wxPizza appear scrollable to GTK+, allowing it to be put in a
GtkScrolledWindow.  Child widget positions are adjusted for the scrolling
position in size_allocate.

For borders, space is reserved in realize and size_allocate.  The border is
drawn on wxPizza's parent GdkWindow.

For RTL, child widget positions are mirrored in size_allocate.
*/

static GtkWidgetClass* parent_class;

extern "C" {

struct wxPizzaClass
{
    GtkFixedClass parent;
    void (*set_scroll_adjustments)(GtkWidget*, GtkAdjustment*, GtkAdjustment*);
};

static void size_allocate(GtkWidget* widget, GtkAllocation* alloc)
{
    const bool is_resize =
        widget->allocation.width  != alloc->width ||
        widget->allocation.height != alloc->height;
    const bool is_move =
        widget->allocation.x != alloc->x ||
        widget->allocation.y != alloc->y;

    wxPizza* pizza = WX_PIZZA(widget);
    int border_x, border_y;
    pizza->get_border_widths(border_x, border_y);
    int w = alloc->width - 2 * border_x;
    if (w < 0) w = 0;

    if (GTK_WIDGET_REALIZED(widget) && (is_move || is_resize))
    {
        int h = alloc->height - 2 * border_y;
        if (h < 0) h = 0;

        gdk_window_move_resize(widget->window,
            alloc->x + border_x, alloc->y + border_y, w, h);

        if (is_resize && (border_x || border_y))
        {
            // old and new border areas need to be invalidated,
            // otherwise they will not be erased/redrawn properly
            GdkWindow* parent = gtk_widget_get_parent_window(widget);
            gdk_window_invalidate_rect(parent, &widget->allocation, false);
            gdk_window_invalidate_rect(parent, alloc, false);
        }
    }

    widget->allocation = *alloc;

    // adjust child positions
    for (const GList* list = pizza->m_fixed.children; list; list = list->next)
    {
        const GtkFixedChild* child = static_cast<GtkFixedChild*>(list->data);
        if (GTK_WIDGET_VISIBLE(child->widget))
        {
            GtkAllocation child_alloc;
            // note that child positions do not take border into
            // account, they need to be relative to widget->window,
            // which has already been adjusted
            child_alloc.x = child->x - pizza->m_scroll_x;
            child_alloc.y = child->y - pizza->m_scroll_y;
            GtkRequisition req;
            gtk_widget_get_child_requisition(child->widget, &req);
            child_alloc.width  = req.width;
            child_alloc.height = req.height;
            if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL)
                child_alloc.x = w - child_alloc.x - child_alloc.width;
            gtk_widget_size_allocate(child->widget, &child_alloc);
        }
    }
}

static void realize(GtkWidget* widget)
{
    parent_class->realize(widget);

    wxPizza* pizza = WX_PIZZA(widget);
    if (pizza->m_border_style)
    {
        int border_x, border_y;
        pizza->get_border_widths(border_x, border_y);
        int x = widget->allocation.x + border_x;
        int y = widget->allocation.y + border_y;
        int w = widget->allocation.width  - 2 * border_x;
        int h = widget->allocation.height - 2 * border_y;
        if (w < 0) w = 0;
        if (h < 0) h = 0;
        gdk_window_move_resize(widget->window, x, y, w, h);
    }
}

static void show(GtkWidget* widget)
{
    if (widget->parent && WX_PIZZA(widget)->m_border_style)
    {
        // invalidate whole allocation so borders will be drawn properly
        const GtkAllocation& a = widget->allocation;
        gtk_widget_queue_draw_area(widget->parent, a.x, a.y, a.width, a.height);
    }

    parent_class->show(widget);
}

static void hide(GtkWidget* widget)
{
    if (widget->parent && WX_PIZZA(widget)->m_border_style)
    {
        // invalidate whole allocation so borders will be erased properly
        const GtkAllocation& a = widget->allocation;
        gtk_widget_queue_draw_area(widget->parent, a.x, a.y, a.width, a.height);
    }

    parent_class->hide(widget);
}

// not used, but needs to exist so gtk_widget_set_scroll_adjustments will work
static void set_scroll_adjustments(GtkWidget*, GtkAdjustment*, GtkAdjustment*)
{
}

// Marshaller needed for set_scroll_adjustments signal,
// generated with GLib-2.4.6 glib-genmarshal
#define g_marshal_value_peek_object(v)   g_value_get_object (v)
static void
g_cclosure_user_marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
                                             GValue       * /*return_value*/,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      /*invocation_hint*/,
                                             gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__OBJECT_OBJECT) (gpointer     data1,
                                                    gpointer     arg_1,
                                                    gpointer     arg_2,
                                                    gpointer     data2);
  register GMarshalFunc_VOID__OBJECT_OBJECT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__OBJECT_OBJECT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_object (param_values + 1),
            g_marshal_value_peek_object (param_values + 2),
            data2);
}

static void class_init(void* g_class, void*)
{
    GtkWidgetClass* widget_class = (GtkWidgetClass*)g_class;
    widget_class->size_allocate = size_allocate;
    widget_class->realize = realize;
    widget_class->show = show;
    widget_class->hide = hide;
    wxPizzaClass* klass = (wxPizzaClass*)g_class;

    // needed to make widget appear scrollable to GTK+
    klass->set_scroll_adjustments = set_scroll_adjustments;
    widget_class->set_scroll_adjustments_signal =
        g_signal_new(
            "set_scroll_adjustments",
            G_TYPE_FROM_CLASS(g_class),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(wxPizzaClass, set_scroll_adjustments),
            NULL, NULL,
            g_cclosure_user_marshal_VOID__OBJECT_OBJECT,
            G_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

    parent_class = GTK_WIDGET_CLASS(g_type_class_peek_parent(g_class));
}

} // extern "C"

GType wxPizza::type()
{
    static GtkType type;
    if (type == 0)
    {
        const GTypeInfo info = {
            sizeof(wxPizzaClass),
            NULL, NULL,
            class_init,
            NULL, NULL,
            sizeof(wxPizza), 0,
            NULL, NULL
        };
        type = g_type_register_static(
            GTK_TYPE_FIXED, "wxPizza", &info, GTypeFlags(0));
    }
    return type;
}

GtkWidget* wxPizza::New(long windowStyle)
{
    GtkWidget* widget = GTK_WIDGET(g_object_new(type(), NULL));
    wxPizza* pizza = WX_PIZZA(widget);
    pizza->m_scroll_x = 0;
    pizza->m_scroll_y = 0;
    pizza->m_is_scrollable = (windowStyle & (wxHSCROLL | wxVSCROLL)) != 0;
    // mask off border styles not useable with wxPizza
    pizza->m_border_style = int(windowStyle & BORDER_STYLES);
    gtk_fixed_set_has_window(GTK_FIXED(widget), true);
    gtk_widget_add_events(widget,
        GDK_EXPOSURE_MASK |
        GDK_SCROLL_MASK |
        GDK_POINTER_MOTION_MASK |
        GDK_POINTER_MOTION_HINT_MASK |
        GDK_BUTTON_MOTION_MASK |
        GDK_BUTTON1_MOTION_MASK |
        GDK_BUTTON2_MOTION_MASK |
        GDK_BUTTON3_MOTION_MASK |
        GDK_BUTTON_PRESS_MASK |
        GDK_BUTTON_RELEASE_MASK |
        GDK_KEY_PRESS_MASK |
        GDK_KEY_RELEASE_MASK |
        GDK_ENTER_NOTIFY_MASK |
        GDK_LEAVE_NOTIFY_MASK |
        GDK_FOCUS_CHANGE_MASK);
    return widget;
}

// gtk_fixed_move does not check for a change before issuing a queue_resize,
// we need to avoid that to prevent endless sizing loops, so check first
void wxPizza::move(GtkWidget* widget, int x, int y)
{
    GtkFixed* fixed = &m_fixed;
    for (const GList* list = fixed->children; list; list = list->next)
    {
        const GtkFixedChild* child = static_cast<GtkFixedChild*>(list->data);
        if (child->widget == widget)
        {
            if (child->x != x || child->y != y)
                gtk_fixed_move(fixed, widget, x, y);
            break;
        }
    }
}

void wxPizza::put(GtkWidget* widget, int x, int y)
{
    gtk_fixed_put(&m_fixed, widget, x, y);
}

struct AdjustData {
    GdkWindow* window;
    int dx, dy;
};

// Adjust allocations for all widgets using the GdkWindow which was just scrolled
extern "C" {
static void scroll_adjust(GtkWidget* widget, void* data)
{
    const AdjustData* p = static_cast<AdjustData*>(data);
    widget->allocation.x += p->dx;
    widget->allocation.y += p->dy;

    if (widget->window == p->window)
    {
        // GtkFrame requires a queue_resize, otherwise parts of
        // the frame newly exposed by the scroll are not drawn.
        // To be safe, do it for all widgets.
        gtk_widget_queue_resize_no_redraw(widget);
        if (GTK_IS_CONTAINER(widget))
            gtk_container_forall(GTK_CONTAINER(widget), scroll_adjust, data);
    }
}
}

void wxPizza::scroll(int dx, int dy)
{
    GtkWidget* widget = GTK_WIDGET(this);
    if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL)
        dx = -dx;
    m_scroll_x -= dx;
    m_scroll_y -= dy;
    if (widget->window)
    {
        gdk_window_scroll(widget->window, dx, dy);
        // Adjust child allocations. Doing a queue_resize on the children is not
        // enough, sometimes they redraw in the wrong place during fast scrolling.
        AdjustData data = { widget->window, dx, dy };
        gtk_container_forall(GTK_CONTAINER(widget), scroll_adjust, &data);
    }
}

void wxPizza::get_border_widths(int& x, int& y)
{
    x = y = 0;
    if (m_border_style == 0)
        return;

#ifndef __WXUNIVERSAL__
    if (m_border_style & wxBORDER_SIMPLE)
        x = y = 1;
    else if (m_is_scrollable /* || (m_border_style & wxBORDER_THEME) */)
    {
        GtkWidget *style_widget = wxGTKPrivate::GetTreeWidget();

        if (style_widget->style)
        {
            x = style_widget->style->xthickness;
            y = style_widget->style->ythickness;
        }
    }
    else
    {
        GtkWidget *style_widget = wxGTKPrivate::GetEntryWidget();

        if (style_widget->style)
        {
            x = style_widget->style->xthickness;
            y = style_widget->style->ythickness;
        }
    }
#endif
}
