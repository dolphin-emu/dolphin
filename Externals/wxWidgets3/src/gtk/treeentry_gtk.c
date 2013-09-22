/* ///////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/treeentry_gtk.c
// Purpose:     GtkTreeEntry implementation
// Author:      Ryan Norton
// Copyright:   (c) 2006 Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////// */

#ifdef __VMS
#include <types.h>
typedef pid_t GPid;
#define G_GNUC_INTERNAL 
#define GSEAL(x) x
#endif

#include "wx/gtk/private/treeentry_gtk.h"

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
static void wx_tree_entry_class_init(void* g_class, void* class_data);
static void wx_tree_entry_string_transform_func(const GValue *src_value,
                                                 GValue *dest_value);
static void wx_tree_entry_dispose(GObject* obj);

static GObjectClass* parent_class;

/* public */
wxTreeEntry*
wx_tree_entry_new()
{
    return WX_TREE_ENTRY(g_object_new(WX_TYPE_TREE_ENTRY, NULL));
}

GType
wx_tree_entry_get_type()
{
    static GType tree_entry_type = 0;

    if (!tree_entry_type)
    {
        const GTypeInfo tree_entry_info =
        {
            sizeof(GObjectClass),
            NULL,           /* base_init */
            NULL,           /* base_finalize */
            wx_tree_entry_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof(wxTreeEntry),
            16,             /* n_preallocs */
            NULL,           /* instance_init */
            NULL            /* value_table */
        };
        tree_entry_type = g_type_register_static (G_TYPE_OBJECT, "wxTreeEntry",
                                                  &tree_entry_info,
                                                  (GTypeFlags)0);
        g_value_register_transform_func(tree_entry_type, G_TYPE_STRING,
                                        wx_tree_entry_string_transform_func);
    }

    return tree_entry_type;
}

char* wx_tree_entry_get_collate_key(wxTreeEntry* entry)
{
    if (entry->collate_key == NULL)
    {
        char* temp = g_utf8_casefold(entry->label, -1);
        entry->collate_key = g_utf8_collate_key(temp, -1);
        g_free(temp);
    }
    return entry->collate_key;
}

char* wx_tree_entry_get_label(wxTreeEntry* entry)
{
    g_assert(WX_IS_TREE_ENTRY(entry));
    return entry->label;
}

void* wx_tree_entry_get_userdata(wxTreeEntry* entry)
{
    g_assert(WX_IS_TREE_ENTRY(entry));
    return entry->userdata;
}

void wx_tree_entry_set_label(wxTreeEntry* entry, const char* label)
{
    g_assert(WX_IS_TREE_ENTRY(entry));

    /* free previous if it exists */
    if(entry->label)
    {
        g_free(entry->label);
        g_free(entry->collate_key);
    }

    entry->label = g_strdup(label);
    entry->collate_key = NULL;
}

void wx_tree_entry_set_userdata(wxTreeEntry* entry, void* userdata)
{
    g_assert(WX_IS_TREE_ENTRY(entry));
    entry->userdata = userdata;
}

void wx_tree_entry_set_destroy_func(wxTreeEntry* entry,
                                         wxTreeEntryDestroy destroy_func,
                                         gpointer destroy_func_data)
{
    g_assert(WX_IS_TREE_ENTRY(entry));
    entry->destroy_func = destroy_func;
    entry->destroy_func_data = destroy_func_data;
}

/* private */
static void wx_tree_entry_class_init(void* g_class, void* class_data)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS(g_class);
    gobject_class->dispose = wx_tree_entry_dispose;
    parent_class = G_OBJECT_CLASS(g_type_class_peek_parent(g_class));
}

static void wx_tree_entry_string_transform_func(const GValue *src_value,
                                                 GValue *dest_value)
{
    wxTreeEntry* entry;
    void* src_ptr = g_value_peek_pointer(src_value);

    /* Make sure src is a treeentry and dest can hold a string */
    g_assert(WX_IS_TREE_ENTRY(src_ptr));
    g_assert(G_VALUE_HOLDS(dest_value, G_TYPE_STRING));

    entry = WX_TREE_ENTRY(src_ptr);
    g_value_set_string(dest_value, entry->label);
}

static void wx_tree_entry_dispose(GObject* obj)
{
    wxTreeEntry* entry;

    g_assert(WX_IS_TREE_ENTRY(obj));

    entry = WX_TREE_ENTRY(obj);

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

    parent_class->dispose(obj);
}
