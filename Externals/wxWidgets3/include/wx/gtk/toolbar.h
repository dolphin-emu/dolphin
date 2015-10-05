/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/toolbar.h
// Purpose:     GTK toolbar
// Author:      Robert Roebling
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_TOOLBAR_H_
#define _WX_GTK_TOOLBAR_H_

typedef struct _GtkTooltips GtkTooltips;

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
               long style = wxTB_DEFAULT_STYLE,
               const wxString& name = wxToolBarNameStr )
    {
        Init();

        Create(parent, id, pos, size, style, name);
    }

    bool Create( wxWindow *parent,
                 wxWindowID id,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxTB_DEFAULT_STYLE,
                 const wxString& name = wxToolBarNameStr );

    virtual ~wxToolBar();

    virtual wxToolBarToolBase *FindToolForPosition(wxCoord x, wxCoord y) const wxOVERRIDE;

    virtual void SetToolShortHelp(int id, const wxString& helpString) wxOVERRIDE;

    virtual void SetWindowStyleFlag( long style ) wxOVERRIDE;

    virtual void SetToolNormalBitmap(int id, const wxBitmap& bitmap) wxOVERRIDE;
    virtual void SetToolDisabledBitmap(int id, const wxBitmap& bitmap) wxOVERRIDE;

    virtual bool Realize() wxOVERRIDE;

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    virtual wxToolBarToolBase *CreateTool(int id,
                                          const wxString& label,
                                          const wxBitmap& bitmap1,
                                          const wxBitmap& bitmap2 = wxNullBitmap,
                                          wxItemKind kind = wxITEM_NORMAL,
                                          wxObject *clientData = NULL,
                                          const wxString& shortHelpString = wxEmptyString,
                                          const wxString& longHelpString = wxEmptyString) wxOVERRIDE;
    virtual wxToolBarToolBase *CreateTool(wxControl *control,
                                          const wxString& label) wxOVERRIDE;

    // implementation from now on
    // --------------------------

    GtkToolbar* GTKGetToolbar() const { return m_toolbar; }

protected:
    // choose the default border for this window
    virtual wxBorder GetDefaultBorder() const wxOVERRIDE { return wxBORDER_DEFAULT; }

    virtual wxSize DoGetBestSize() const wxOVERRIDE;
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const wxOVERRIDE;

    // implement base class pure virtuals
    virtual bool DoInsertTool(size_t pos, wxToolBarToolBase *tool) wxOVERRIDE;
    virtual bool DoDeleteTool(size_t pos, wxToolBarToolBase *tool) wxOVERRIDE;

    virtual void DoEnableTool(wxToolBarToolBase *tool, bool enable) wxOVERRIDE;
    virtual void DoToggleTool(wxToolBarToolBase *tool, bool toggle) wxOVERRIDE;
    virtual void DoSetToggle(wxToolBarToolBase *tool, bool toggle) wxOVERRIDE;

private:
    void Init();
    void GtkSetStyle();
    GSList* GetRadioGroup(size_t pos);
    virtual void AddChildGTK(wxWindowGTK* child) wxOVERRIDE;

    GtkToolbar* m_toolbar;
    GtkTooltips* m_tooltips;

    wxDECLARE_DYNAMIC_CLASS(wxToolBar);
};

#endif
    // _WX_GTK_TOOLBAR_H_
