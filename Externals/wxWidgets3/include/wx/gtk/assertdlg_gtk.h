/* ///////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/assertdlg_gtk.h
// Purpose:     GtkAssertDialog
// Author:      Francesco Montorsi
// Id:          $Id: assertdlg_gtk.h 67254 2011-03-20 00:14:35Z DS $
// Copyright:   (c) 2006 Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////// */

#ifndef __GTK_ASSERTDLG_H__
#define __GTK_ASSERTDLG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtk.h>

#define GTK_TYPE_ASSERT_DIALOG            (gtk_assert_dialog_get_type ())
#define GTK_ASSERT_DIALOG(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GTK_TYPE_ASSERT_DIALOG, GtkAssertDialog))
#define GTK_ASSERT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ASSERT_DIALOG, GtkAssertDialogClass))
#define GTK_IS_ASSERT_DIALOG(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_ASSERT_DIALOG))
#define GTK_IS_ASSERT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ASSERT_DIALOG))
#define GTK_ASSERT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ASSERT_DIALOG, GtkAssertDialogClass))

typedef struct _GtkAssertDialog        GtkAssertDialog;
typedef struct _GtkAssertDialogClass   GtkAssertDialogClass;
typedef void (*GtkAssertDialogStackFrameCallback)(void *);

struct _GtkAssertDialog
{
    GtkDialog parent_instance;

    /* GtkAssertDialog widgets */
    GtkWidget *expander;
    GtkWidget *message;
    GtkWidget *treeview;

    GtkWidget *shownexttime;

    /* callback for processing the stack frame */
    GtkAssertDialogStackFrameCallback callback;
    void *userdata;
};

struct _GtkAssertDialogClass
{
    GtkDialogClass parent_class;
};

typedef enum
{
    GTK_ASSERT_DIALOG_STOP,
    GTK_ASSERT_DIALOG_CONTINUE,
    GTK_ASSERT_DIALOG_CONTINUE_SUPPRESSING
} GtkAssertDialogResponseID;




GType gtk_assert_dialog_get_type(void);
GtkWidget *gtk_assert_dialog_new(void);

/* get the assert message */
gchar *gtk_assert_dialog_get_message(GtkAssertDialog *assertdlg);

/* set the assert message */
void gtk_assert_dialog_set_message(GtkAssertDialog *assertdlg, const gchar *msg);

/* get a string containing all stack frames appended to the dialog */
gchar *gtk_assert_dialog_get_backtrace(GtkAssertDialog *assertdlg);

/* sets the callback to use when the user wants to see the stackframe */
void gtk_assert_dialog_set_backtrace_callback(GtkAssertDialog *assertdlg,
                                              GtkAssertDialogStackFrameCallback callback,
                                              void *userdata);

/* appends a stack frame to the dialog */
void gtk_assert_dialog_append_stack_frame(GtkAssertDialog *dlg,
                                          const gchar *function,
                                          const gchar *arguments,
                                          const gchar *sourcefile,
                                          guint line_number);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_ASSERTDLG_H__ */


