///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/treeview.h
// Purpose:     Private helpers for wxGTK controls using GtkTreeView
// Author:      Vadim Zeitlin
// Created:     2016-02-06 (extracted from src/gtk/dataview.cpp)
// Copyright:   (c) 2016 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _GTK_PRIVATE_TREEVIEW_H_
#define _GTK_PRIVATE_TREEVIEW_H_

// ----------------------------------------------------------------------------
// wxGtkTreePath: RAII wrapper for GtkTreePath
// ----------------------------------------------------------------------------

// Usually this object is initialized with the associated GtkTreePath
// immediately when it's constructed but it can also be changed later either by
// using Assign() or by getting the pointer to the internally stored pointer
// value using ByRef(). The latter should be avoided but is very convenient
// when using GTK functions with GtkTreePath output parameters.
class wxGtkTreePath
{
public:
    // Ctor takes ownership of the given path and will free it if non-NULL.
    wxGtkTreePath(GtkTreePath *path = NULL) : m_path(path) { }

    // Creates a tree path for the given string path.
    wxGtkTreePath(const gchar *strpath)
        : m_path(gtk_tree_path_new_from_string(strpath))
    {
    }

    // Set the stored pointer if not done by ctor.
    void Assign(GtkTreePath *path)
    {
        wxASSERT_MSG( !m_path, "shouldn't be already initialized" );

        m_path = path;
    }

    // Return the pointer to the internally stored pointer. This should only be
    // used to initialize the object by passing it to some GTK function.
    GtkTreePath **ByRef()
    {
        wxASSERT_MSG( !m_path, "shouldn't be already initialized" );

        return &m_path;
    }


    operator GtkTreePath *() const { return m_path; }

    ~wxGtkTreePath() { if ( m_path ) gtk_tree_path_free(m_path); }

private:
    GtkTreePath *m_path;

    wxDECLARE_NO_COPY_CLASS(wxGtkTreePath);
};

#endif // _GTK_PRIVATE_TREEVIEW_H_
