/* ///////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/treeentry_gtk.h
// Purpose:     GtkTreeEntry - a string/userdata combo for use with treeview
// Author:      Ryan Norton
// Id:          $Id: treeentry_gtk.h 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) 2006 Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////// */

#ifndef __GTK_TREE_ENTRY_H__
#define __GTK_TREE_ENTRY_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtk.h> /* for gpointer and gchar* etc. */

#include "wx/dlimpexp.h"

#define GTK_TYPE_TREE_ENTRY          (gtk_tree_entry_get_type())
#define GTK_TREE_ENTRY(obj)          (GTK_CHECK_CAST (obj, gtk_tree_entry_get_type (), GtkTreeEntry))
#define GTK_TREE_ENTRY_CLASS(klass)  (GTK_CHECK_CLASS_CAST (klass, gtk_tree_entry_get_type (), GtkTreeEntryClass))
#define GTK_IS_TREE_ENTRY(obj)       (GTK_CHECK_TYPE (obj, gtk_tree_entry_get_type ()))

typedef struct _GtkTreeEntry        GtkTreeEntry;
typedef struct _GtkTreeEntryClass   GtkTreeEntryClass;

typedef void (*GtkTreeEntryDestroy) (GtkTreeEntry* entry, gpointer context);

struct _GtkTreeEntry
{
    GObject parent;                     /* object instance */
    gchar*  label;                      /* label - always copied by this object except on get */
    gchar*  collate_key;                /* collate key used for string comparisons/sorting */
    gpointer userdata;                  /* untouched userdata */
    GtkTreeEntryDestroy destroy_func;   /* called upon destruction - use for freeing userdata etc. */
    gpointer destroy_func_data;         /* context passed to destroy_func */
};

struct _GtkTreeEntryClass
{
    GObjectClass parent;
};

WXDLLIMPEXP_CORE
GtkTreeEntry* gtk_tree_entry_new        (void);

WXDLLIMPEXP_CORE
GtkType    gtk_tree_entry_get_type      (void);

WXDLLIMPEXP_CORE
gchar*     gtk_tree_entry_get_collate_key     (GtkTreeEntry* entry);

WXDLLIMPEXP_CORE
gchar*     gtk_tree_entry_get_label     (GtkTreeEntry* entry);

WXDLLIMPEXP_CORE
gpointer   gtk_tree_entry_get_userdata  (GtkTreeEntry* entry);

WXDLLIMPEXP_CORE
void     gtk_tree_entry_set_label       (GtkTreeEntry* entry, const gchar* label);

WXDLLIMPEXP_CORE
void   gtk_tree_entry_set_userdata      (GtkTreeEntry* entry, gpointer userdata);

WXDLLIMPEXP_CORE
void   gtk_tree_entry_set_destroy_func (GtkTreeEntry* entry,
                                        GtkTreeEntryDestroy destroy_func,
                                        gpointer destroy_func_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_TREE_ENTRY_H__ */
