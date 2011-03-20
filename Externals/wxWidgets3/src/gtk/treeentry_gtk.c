/* ///////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/treeentry_gtk.c
// Purpose:     GtkTreeEntry implementation
// Author:      Ryan Norton
// Id:          $Id: treeentry_gtk.c 65341 2010-08-18 21:28:11Z RR $
// Copyright:   (c) 2006 Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////// */

#ifdef __VMS
#include <types.h>
typedef pid_t GPid;
#define G_GNUC_INTERNAL 
#define GSEAL(x) x
#endif

#include "wx/gtk/treeentry_gtk.h"

/*
        GtkTreeEntry

        The main reason for this class is to have a holder for both a string
        and userdata for us to use in wxListXXX classes.

        This is transformable to a string for the Gtk implementations,
        and the string passed in is duplicated and freed upon destruction.

        As mentioned the real magic here is the transforming it to a string
        which lets us use it as a entry in a GtkTreeView/GtkListStore
        and still display it. Otherwise we would need to implement our
        own model etc..
*/

/* forwards */
static void gtk_tree_entry_class_init(GtkTreeEntryClass* klass);
static void gtk_tree_entry_init (GTypeInstance* instance, gpointer g_class);
static void gtk_tree_entry_string_transform_func(const GValue *src_value,
                                                 GValue *dest_value);
static void gtk_tree_entry_dispose(GObject* obj);


/* public */
GtkTreeEntry*
gtk_tree_entry_new()
{
    return GTK_TREE_ENTRY(g_object_new(GTK_TYPE_TREE_ENTRY, NULL));
}

GtkType
gtk_tree_entry_get_type ()
{
    static GtkType tree_entry_type = 0;

    if (!tree_entry_type)
    {
        const GTypeInfo tree_entry_info =
        {
            sizeof (GtkTreeEntryClass),
            NULL,           /* base_init */
            NULL,           /* base_finalize */
            (GClassInitFunc) gtk_tree_entry_class_init,  /* class_init */
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof (GtkTreeEntry),
            16,             /* n_preallocs */
            (GInstanceInitFunc) gtk_tree_entry_init, /*instance_init*/
            NULL            /* value_table */
        };
        tree_entry_type = g_type_register_static (G_TYPE_OBJECT, "GtkTreeEntry",
                                                  &tree_entry_info,
                                                  (GTypeFlags)0);
        g_value_register_transform_func(tree_entry_type, G_TYPE_STRING,
                                        gtk_tree_entry_string_transform_func);
    }

    return tree_entry_type;
}

gchar*     gtk_tree_entry_get_collate_key (GtkTreeEntry* entry)
{
    return entry->collate_key;
}

gchar*     gtk_tree_entry_get_label     (GtkTreeEntry* entry)
{
    g_assert(GTK_IS_TREE_ENTRY(entry));
    return entry->label;
}

gpointer   gtk_tree_entry_get_userdata  (GtkTreeEntry* entry)
{
    g_assert(GTK_IS_TREE_ENTRY(entry));
    return entry->userdata;
}

void     gtk_tree_entry_set_label       (GtkTreeEntry* entry, const gchar* label)
{
    g_assert(GTK_IS_TREE_ENTRY(entry));
    gchar *temp;

    /* free previous if it exists */
    if(entry->label)
    {
        g_free(entry->label);
        g_free(entry->collate_key);
    }

    entry->label = g_strdup(label);
    temp = g_utf8_casefold(label, -1); /* -1 == null terminated */
    entry->collate_key = g_utf8_collate_key(temp, -1); /* -1 == null terminated */
    g_free( temp );
}

void   gtk_tree_entry_set_userdata      (GtkTreeEntry* entry, gpointer userdata)
{
    g_assert(GTK_IS_TREE_ENTRY(entry));
    entry->userdata = userdata;
}

void   gtk_tree_entry_set_destroy_func  (GtkTreeEntry* entry,
                                         GtkTreeEntryDestroy destroy_func,
                                         gpointer destroy_func_data)
{
    g_assert(GTK_IS_TREE_ENTRY(entry));
    entry->destroy_func = destroy_func;
    entry->destroy_func_data = destroy_func_data;
}

/* private */
static void gtk_tree_entry_class_init(GtkTreeEntryClass* klass)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = gtk_tree_entry_dispose;
}

static void gtk_tree_entry_init (GTypeInstance* instance, gpointer g_class)
{
    GtkTreeEntry* entry = (GtkTreeEntry*) instance;

    /* clear */
    entry->label = NULL;
    entry->collate_key = NULL;
    entry->userdata = NULL;
    entry->destroy_func_data = NULL;
    entry->destroy_func = NULL;
}

static void gtk_tree_entry_string_transform_func(const GValue *src_value,
                                                 GValue *dest_value)
{
    GtkTreeEntry *entry;

    /* Make sure src is a treeentry and dest can hold a string */
    g_assert(GTK_IS_TREE_ENTRY(src_value->data[0].v_pointer));
    g_assert(G_VALUE_HOLDS(dest_value, G_TYPE_STRING));

    /* TODO: Use strdup here or just pass it? */
    entry = GTK_TREE_ENTRY(src_value->data[0].v_pointer);

    g_value_set_string(dest_value, entry->label);
}

static void gtk_tree_entry_dispose(GObject* obj)
{
    GtkTreeEntry *entry;

    g_assert(GTK_IS_TREE_ENTRY(obj));

    entry = GTK_TREE_ENTRY(obj);

    /* free label if it exists */
    if(entry->label)
    {
        g_free(entry->label);
        g_free(entry->collate_key);
        entry->label = NULL;
        entry->collate_key = NULL;
    }

    /* call destroy callback if it exists */
    if(entry->destroy_func)
    {
        (*entry->destroy_func) (entry, entry->destroy_func_data);
        entry->destroy_func = NULL;
        entry->destroy_func_data = NULL;
    }

    /* clear userdata */
    entry->userdata = NULL;
}
