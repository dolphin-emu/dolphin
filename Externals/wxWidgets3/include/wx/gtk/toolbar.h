/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/toolbar.h
// Purpose:     GTK toolbar
// Author:      Robert Roebling
// RCS-ID:      $Id: toolbar.h 66633 2011-01-07 18:15:21Z PC $
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_TOOLBAR_H_
#define _WX_GTK_TOOLBAR_H_

#if wxUSE_TOOLBAR

// ----------------------------------------------------------------------------
// wxToolBar
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxToolBar : public wxToolBarBase
{
public:
    // construction/destruction
    wxToolBar() { Init(); }
    wxToolBar( wxWindow *parent,
               wxWindowID id,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxTB_HORIZONTAL,
               const wxString& name = wxToolBarNameStr )
    {
        Init();

        Create(parent, id, pos, size, style, name);
    }

    bool Create( wxWindow *parent,
                 wxWindowID id,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxTB_HORIZONTAL,
                 const wxString& name = wxToolBarNameStr );

    virtual ~wxToolBar();

    virtual wxToolBarToolBase *FindToolForPosition(wxCoord x, wxCoord y) const;

    virtual void SetToolShortHelp(int id, const wxString& helpString);

    virtual void SetWindowStyleFlag( long style );

    virtual void SetToolNormalBitmap(int id, const wxBitmap& bitmap);
    virtual void SetToolDisabledBitmap(int id, const wxBitmap& bitmap);

    virtual bool Realize();

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation from now on
    // --------------------------

protected:
    virtual wxSize DoGetBestSize() const;
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

    // implement base class pure virtuals
    virtual bool DoInsertTool(size_t pos, wxToolBarToolBase *tool);
    virtual bool DoDeleteTool(size_t pos, wxToolBarToolBase *tool);

    virtual void DoEnableTool(wxToolBarToolBase *tool, bool enable);
    virtual void DoToggleTool(wxToolBarToolBase *tool, bool toggle);
    virtual void DoSetToggle(wxToolBarToolBase *tool, bool toggle);

    virtual wxToolBarToolBase *CreateTool(int id,
                                          const wxString& label,
                                          const wxBitmap& bitmap1,
                                          const wxBitmap& bitmap2,
                                          wxItemKind kind,
                                          wxObject *clientData,
                                          const wxString& shortHelpString,
                                          const wxString& longHelpString);
    virtual wxToolBarToolBase *CreateTool(wxControl *control,
                                          const wxString& label);

private:
    void Init();
    void GtkSetStyle();
    GSList* GetRadioGroup(size_t pos);
    virtual void AddChildGTK(wxWindowGTK* child);

    GtkToolbar* m_toolbar;
    GtkTooltips* m_tooltips;

    DECLARE_DYNAMIC_CLASS(wxToolBar)
};

#endif // wxUSE_TOOLBAR

#endif
    // _WX_GTK_TOOLBAR_H_
