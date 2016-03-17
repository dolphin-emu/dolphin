///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/private.cpp
// Purpose:     implementation of wxGTK private functions
// Author:      Marcin Malich
// Modified by:
// Created:     28.06.2008
// Copyright:   (c) 2008 Marcin Malich <me@malcom.pl>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/module.h"
#endif

#include <gtk/gtk.h>
#include "wx/gtk/private.h"

// ----------------------------------------------------------------------------
// wxGTKPrivate functions implementation
// ----------------------------------------------------------------------------

namespace wxGTKPrivate
{

static GtkWidget *gs_container = NULL;

static GtkContainer* GetContainer()
{
    if ( gs_container == NULL )
    {
        GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
        gs_container = gtk_fixed_new();
        gtk_container_add(GTK_CONTAINER(window), gs_container);
    }
    return GTK_CONTAINER(gs_container);
}

GtkWidget *GetButtonWidget()
{
    static GtkWidget *s_button = NULL;

    if ( !s_button )
    {
        s_button = gtk_button_new();
        gtk_container_add(GetContainer(), s_button);
        gtk_widget_realize(s_button);
    }

    return s_button;
}

GtkWidget *GetNotebookWidget()
{
    static GtkWidget *s_notebook = NULL;

    if ( !s_notebook )
    {
        s_notebook = gtk_notebook_new();
        gtk_container_add(GetContainer(), s_notebook);
        gtk_widget_realize(s_notebook);
    }

    return s_notebook;
}

GtkWidget *GetCheckButtonWidget()
{
    static GtkWidget *s_button = NULL;

    if ( !s_button )
    {
        s_button = gtk_check_button_new();
        gtk_container_add(GetContainer(), s_button);
        gtk_widget_realize(s_button);
    }

    return s_button;
}

GtkWidget * GetComboBoxWidget()
{
    static GtkWidget *s_button = NULL;
    static GtkWidget *s_window = NULL;

    if ( !s_button )
    {
        s_window = gtk_window_new( GTK_WINDOW_POPUP );
        gtk_widget_realize( s_window );
        s_button = gtk_combo_box_new();
        gtk_container_add( GTK_CONTAINER(s_window), s_button );
        gtk_widget_realize( s_button );
    }

    return s_button;
}


GtkWidget *GetEntryWidget()
{
    static GtkWidget *s_entry = NULL;

    if ( !s_entry )
    {
        s_entry = gtk_entry_new();
        gtk_container_add(GetContainer(), s_entry);
        gtk_widget_realize(s_entry);
    }

    return s_entry;
}

// This one just gets the button used by the column header. Although it's
// still a gtk_button the themes will typically differentiate and draw them
// differently if the button is in a treeview.
static GtkWidget *s_first_button = NULL;
static GtkWidget *s_other_button = NULL;
static GtkWidget *s_last_button = NULL;

static void CreateHeaderButtons()
{
        // Get the dummy tree widget, give it a column, and then use the
        // widget in the column header for the rendering code.
        GtkWidget* treewidget = GetTreeWidget();

        GtkTreeViewColumn *column = gtk_tree_view_column_new();
        gtk_tree_view_append_column(GTK_TREE_VIEW(treewidget), column);
#ifdef __WXGTK3__
        s_first_button = gtk_tree_view_column_get_button(column);
#else
        s_first_button = column->button;
#endif
        wxASSERT(s_first_button);

        column = gtk_tree_view_column_new();
        gtk_tree_view_append_column(GTK_TREE_VIEW(treewidget), column);
#ifdef __WXGTK3__
        s_other_button = gtk_tree_view_column_get_button(column);
#else
        s_other_button = column->button;
#endif

        column = gtk_tree_view_column_new();
        gtk_tree_view_append_column(GTK_TREE_VIEW(treewidget), column);
#ifdef __WXGTK3__
        s_last_button = gtk_tree_view_column_get_button(column);
#else
        s_last_button = column->button;
#endif
}

GtkWidget *GetHeaderButtonWidgetFirst()
{
    if (!s_first_button)
      CreateHeaderButtons();

    return s_first_button;
}

GtkWidget *GetHeaderButtonWidgetLast()
{
    if (!s_last_button)
      CreateHeaderButtons();

    return s_last_button;
}

GtkWidget *GetHeaderButtonWidget()
{
    if (!s_other_button)
      CreateHeaderButtons();

    return s_other_button;
}

GtkWidget * GetRadioButtonWidget()
{
    static GtkWidget *s_button = NULL;
    static GtkWidget *s_window = NULL;

    if ( !s_button )
    {
        s_window = gtk_window_new( GTK_WINDOW_POPUP );
        gtk_widget_realize( s_window );
        s_button = gtk_radio_button_new(NULL);
        gtk_container_add( GTK_CONTAINER(s_window), s_button );
        gtk_widget_realize( s_button );
    }

    return s_button;
}

GtkWidget* GetSplitterWidget(wxOrientation orient)
{
    static GtkWidget* widgets[2];
    const GtkOrientation gtkOrient =
        orient == wxHORIZONTAL ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    GtkWidget*& widget = widgets[gtkOrient];
    if (widget == NULL)
    {
#ifdef __WXGTK3__
        widget = gtk_paned_new(gtkOrient);
#else
        if (orient == wxHORIZONTAL)
            widget = gtk_hpaned_new();
        else
            widget = gtk_vpaned_new();
#endif
        gtk_container_add(GetContainer(), widget);
        gtk_widget_realize(widget);
    }

    return widget;
}

GtkWidget * GetTextEntryWidget()
{
    static GtkWidget *s_button = NULL;
    static GtkWidget *s_window = NULL;

    if ( !s_button )
    {
        s_window = gtk_window_new( GTK_WINDOW_POPUP );
        gtk_widget_realize( s_window );
        s_button = gtk_entry_new();
        gtk_container_add( GTK_CONTAINER(s_window), s_button );
        gtk_widget_realize( s_button );
    }

    return s_button;
}

GtkWidget *GetTreeWidget()
{
    static GtkWidget *s_tree = NULL;

    if ( !s_tree )
    {
        s_tree = gtk_tree_view_new();
        gtk_container_add(GetContainer(), s_tree);
        gtk_widget_realize(s_tree);
    }

    return s_tree;
}

// Module for destroying created widgets
class WidgetsCleanupModule : public wxModule
{
public:
    virtual bool OnInit() wxOVERRIDE
    {
        return true;
    }

    virtual void OnExit() wxOVERRIDE
    {
        if ( gs_container )
        {
            GtkWidget* parent = gtk_widget_get_parent(gs_container);
            gtk_widget_destroy(parent);
        }
    }

    wxDECLARE_DYNAMIC_CLASS(WidgetsCleanupModule);
};

wxIMPLEMENT_DYNAMIC_CLASS(WidgetsCleanupModule, wxModule);

static WidgetsCleanupModule gs_widgetsCleanupModule;

} // wxGTKPrivate namespace
