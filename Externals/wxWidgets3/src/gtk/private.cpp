///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/private.cpp
// Purpose:     implementation of wxGTK private functions
// Author:      Marcin Malich
// Modified by:
// Created:     28.06.2008
// RCS-ID:      $Id: private.cpp 64940 2010-07-13 13:29:13Z VZ $
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
        s_first_button = column->button;

        column = gtk_tree_view_column_new();
        gtk_tree_view_append_column(GTK_TREE_VIEW(treewidget), column);
        s_other_button = column->button;

        column = gtk_tree_view_column_new();
        gtk_tree_view_append_column(GTK_TREE_VIEW(treewidget), column);
        s_last_button = column->button;
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

GtkWidget* GetSplitterWidget()
{
    static GtkWidget* widget;

    if (widget == NULL)
    {
        widget = gtk_vpaned_new();
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
    virtual bool OnInit()
    {
        return true;
    }

    virtual void OnExit()
    {
        if ( gs_container )
        {
            GtkWidget* parent = gtk_widget_get_parent(gs_container);
            gtk_widget_destroy(parent);
        }
    }

    DECLARE_DYNAMIC_CLASS(WidgetsCleanupModule)
};

IMPLEMENT_DYNAMIC_CLASS(WidgetsCleanupModule, wxModule)

static WidgetsCleanupModule gs_widgetsCleanupModule;

} // wxGTKPrivate namespace
