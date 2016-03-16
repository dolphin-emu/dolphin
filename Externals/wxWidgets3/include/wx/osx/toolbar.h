/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/toolbar.h
// Purpose:     wxToolBar class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TOOLBAR_H_
#define _WX_TOOLBAR_H_

#if wxUSE_TOOLBAR

#include "wx/tbarbase.h"
#include "wx/dynarray.h"

class WXDLLIMPEXP_CORE wxToolBar: public wxToolBarBase
{
    wxDECLARE_DYNAMIC_CLASS(wxToolBar);
public:
  /*
   * Public interface
   */

   wxToolBar() { Init(); }

  wxToolBar(wxWindow *parent, wxWindowID id,
                   const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                   long style = wxTB_DEFAULT_STYLE,
                   const wxString& name = wxToolBarNameStr)
  {
    Init();
    Create(parent, id, pos, size, style, name);
  }
  virtual ~wxToolBar();

  bool Create(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
            long style = wxTB_DEFAULT_STYLE,
            const wxString& name = wxToolBarNameStr);

    virtual void SetWindowStyleFlag(long style) wxOVERRIDE;

    virtual bool Destroy() wxOVERRIDE;

    // override/implement base class virtuals
    virtual wxToolBarToolBase *FindToolForPosition(wxCoord x, wxCoord y) const wxOVERRIDE;

#ifndef __WXOSX_IPHONE__
    virtual bool Show(bool show = true) wxOVERRIDE;
    virtual bool IsShown() const wxOVERRIDE;
#endif
    virtual bool Realize() wxOVERRIDE;

    virtual void SetToolBitmapSize(const wxSize& size) wxOVERRIDE;
    virtual wxSize GetToolSize() const wxOVERRIDE;

    virtual void SetRows(int nRows) wxOVERRIDE;

    virtual void SetToolNormalBitmap(int id, const wxBitmap& bitmap) wxOVERRIDE;
    virtual void SetToolDisabledBitmap(int id, const wxBitmap& bitmap) wxOVERRIDE;

#ifndef __WXOSX_IPHONE__
    // Add all the buttons

    virtual wxString MacGetToolTipString( wxPoint &where ) wxOVERRIDE;
    void OnPaint(wxPaintEvent& event) ;
    void OnMouse(wxMouseEvent& event) ;
    virtual void MacSuperChangedPosition() wxOVERRIDE;
#endif

#if wxOSX_USE_NATIVE_TOOLBAR
    // make all tools selectable
    void OSXSetSelectableTools(bool set);
    void OSXSelectTool(int toolId);

    bool MacInstallNativeToolbar(bool usesNative);
    void MacUninstallNativeToolbar();
    bool MacWantsNativeToolbar();
    bool MacTopLevelHasNativeToolbar(bool *ownToolbarInstalled) const;
#endif

    virtual wxToolBarToolBase *CreateTool(int id,
                                          const wxString& label,
                                          const wxBitmap& bmpNormal,
                                          const wxBitmap& bmpDisabled = wxNullBitmap,
                                          wxItemKind kind = wxITEM_NORMAL,
                                          wxObject *clientData = NULL,
                                          const wxString& shortHelp = wxEmptyString,
                                          const wxString& longHelp = wxEmptyString) wxOVERRIDE;
    virtual wxToolBarToolBase *CreateTool(wxControl *control,
                                          const wxString& label) wxOVERRIDE;

protected:
    // common part of all ctors
    void Init();
    
    void DoLayout();
    
    void DoSetSize(int x, int y, int width, int height, int sizeFlags) wxOVERRIDE;

#ifndef __WXOSX_IPHONE__
    virtual void DoGetSize(int *width, int *height) const wxOVERRIDE;
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
#endif
#ifdef __WXOSX_COCOA__
    virtual void DoGetPosition(int*x, int *y) const wxOVERRIDE;
#endif
    virtual bool DoInsertTool(size_t pos, wxToolBarToolBase *tool) wxOVERRIDE;
    virtual bool DoDeleteTool(size_t pos, wxToolBarToolBase *tool) wxOVERRIDE;

    virtual void DoEnableTool(wxToolBarToolBase *tool, bool enable) wxOVERRIDE;
    virtual void DoToggleTool(wxToolBarToolBase *tool, bool toggle) wxOVERRIDE;
    virtual void DoSetToggle(wxToolBarToolBase *tool, bool toggle) wxOVERRIDE;

    wxDECLARE_EVENT_TABLE();
#if wxOSX_USE_NATIVE_TOOLBAR
    bool m_macUsesNativeToolbar ;
    void* m_macToolbar ;
#endif
#ifdef __WXOSX_IPHONE__
    WX_UIView m_macToolbar;
#endif
};

#endif // wxUSE_TOOLBAR

#endif
    // _WX_TOOLBAR_H_
