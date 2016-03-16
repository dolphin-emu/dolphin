///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dvrenderer.h
// Purpose:     wxDataViewRenderer for GTK wxDataViewCtrl implementation
// Author:      Robert Roebling, Vadim Zeitlin
// Created:     2009-11-07 (extracted from wx/gtk/dataview.h)
// Copyright:   (c) 2006 Robert Roebling
//              (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_DVRENDERER_H_
#define _WX_GTK_DVRENDERER_H_

typedef struct _GtkCellRendererText GtkCellRendererText;
typedef struct _GtkTreeViewColumn GtkTreeViewColumn;

// ----------------------------------------------------------------------------
// wxDataViewRenderer
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewRenderer: public wxDataViewRendererBase
{
public:
    wxDataViewRenderer( const wxString &varianttype,
                        wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                        int align = wxDVR_DEFAULT_ALIGNMENT );

    virtual void SetMode( wxDataViewCellMode mode ) wxOVERRIDE;
    virtual wxDataViewCellMode GetMode() const wxOVERRIDE;

    virtual void SetAlignment( int align ) wxOVERRIDE;
    virtual int GetAlignment() const wxOVERRIDE;

    virtual void EnableEllipsize(wxEllipsizeMode mode = wxELLIPSIZE_MIDDLE) wxOVERRIDE;
    virtual wxEllipsizeMode GetEllipsizeMode() const wxOVERRIDE;

    virtual bool FinishEditing() wxOVERRIDE;

    // GTK-specific implementation
    // ---------------------------

    // pack the GTK cell renderers used by this renderer to the given column
    //
    // by default only a single m_renderer is used but some renderers use more
    // than one GTK cell renderer
    virtual void GtkPackIntoColumn(GtkTreeViewColumn *column);

    // called when the cell value was edited by user with the new value
    //
    // it validates the new value and notifies the model about the change by
    // calling GtkOnCellChanged() if it was accepted
    virtual void GtkOnTextEdited(const char *itempath, const wxString& value);

    GtkCellRenderer* GetGtkHandle() { return m_renderer; }
    void GtkInitHandlers();
    void GtkUpdateAlignment() { GtkApplyAlignment(m_renderer); }

    // return the text renderer used by this renderer for setting text cell
    // specific attributes: can return NULL if this renderer doesn't render any
    // text
    virtual GtkCellRendererText *GtkGetTextRenderer() const { return NULL; }

    // return the widget actually used by the renderer for editing, this may be
    // different from the editor control widget for the custom renderers
    virtual GtkWidget* GtkGetEditorWidget() const;

private:
    // Change the mode at GTK level without touching m_mode, this is useful for
    // temporarily making the renderer insensitive but does mean that GetMode()
    // may return a value different from the actual GTK renderer mode.
    void GtkSetMode(wxDataViewCellMode mode);

protected:
    virtual void SetAttr(const wxDataViewItemAttr& attr) wxOVERRIDE;
    virtual void SetEnabled(bool enabled) wxOVERRIDE;

    virtual void GtkOnCellChanged(const wxVariant& value,
                                  const wxDataViewItem& item,
                                  unsigned col);

    // Apply our effective alignment (i.e. m_alignment if specified or the
    // associated column alignment by default) to the given renderer.
    void GtkApplyAlignment(GtkCellRenderer *renderer);

    GtkCellRenderer    *m_renderer;
    int                 m_alignment;

    // We store the renderer mode at wx level as it can differ from the mode of
    // the corresponding GTK+ renderer as explained above.
    wxDataViewCellMode  m_mode;

    // true if we hadn't changed any visual attributes or restored them since
    // doing this
    bool m_usingDefaultAttrs;

protected:
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewRenderer);
};

#endif // _WX_GTK_DVRENDERER_H_

