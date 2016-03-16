/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/notebook.h
// Purpose:     wxNotebook class
// Author:      Robert Roebling
// Modified by:
// Copyright:   (c) Julian Smart and Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKNOTEBOOK_H_
#define _WX_GTKNOTEBOOK_H_

//-----------------------------------------------------------------------------
// internal class
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxGtkNotebookPage;

#include "wx/list.h"
WX_DECLARE_LIST(wxGtkNotebookPage, wxGtkNotebookPagesList);

//-----------------------------------------------------------------------------
// wxNotebook
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxNotebook : public wxNotebookBase
{
public:
      // default for dynamic class
    wxNotebook();
      // the same arguments as for wxControl
    wxNotebook(wxWindow *parent,
             wxWindowID id,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = 0,
             const wxString& name = wxNotebookNameStr);
      // Create() function
    bool Create(wxWindow *parent,
              wxWindowID id,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = 0,
              const wxString& name = wxNotebookNameStr);
      // dtor
    virtual ~wxNotebook();

  // accessors
  // ---------

    // set the currently selected page, return the index of the previously
    // selected one (or wxNOT_FOUND on error)
    // NB: this function will _not_ generate wxEVT_NOTEBOOK_PAGE_xxx events
    int SetSelection(size_t nPage) wxOVERRIDE { return DoSetSelection(nPage, SetSelection_SendEvent); }
    // get the currently selected page
  int GetSelection() const wxOVERRIDE;

  // changes selected page without sending events
  int ChangeSelection(size_t nPage) wxOVERRIDE { return DoSetSelection(nPage); }

    // set/get the title of a page
  bool SetPageText(size_t nPage, const wxString& strText) wxOVERRIDE;
  wxString GetPageText(size_t nPage) const wxOVERRIDE;

    // sets/returns item's image index in the current image list
  int  GetPageImage(size_t nPage) const wxOVERRIDE;
  bool SetPageImage(size_t nPage, int nImage) wxOVERRIDE;

  // control the appearance of the notebook pages
    // set the padding between tabs (in pixels)
  void SetPadding(const wxSize& padding) wxOVERRIDE;
    // sets the size of the tabs (assumes all tabs are the same size)
  void SetTabSize(const wxSize& sz) wxOVERRIDE;

  // geometry
  virtual wxSize CalcSizeFromPage(const wxSize& sizePage) const wxOVERRIDE;
  virtual int HitTest(const wxPoint& pt, long *flags = NULL) const wxOVERRIDE;

  // operations
  // ----------
    // remove all pages
  bool DeleteAllPages() wxOVERRIDE;

    // adds a new page to the notebook (it will be deleted by the notebook,
    // don't delete it yourself). If bSelect, this page becomes active.
    // the same as AddPage(), but adds it at the specified position
    bool InsertPage( size_t position,
                     wxNotebookPage *win,
                     const wxString& strText,
                     bool bSelect = false,
                     int imageId = NO_IMAGE ) wxOVERRIDE;

    // handler for tab navigation
    // --------------------------
    void OnNavigationKey(wxNavigationKeyEvent& event);


    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation
    // --------------

#if wxUSE_CONSTRAINTS
    void SetConstraintSizes(bool recurse) wxOVERRIDE;
    bool DoPhase(int phase) wxOVERRIDE;
#endif

    // Called by GTK event handler when the current page is definitely changed.
    void GTKOnPageChanged();

    // helper function
    wxGtkNotebookPage* GetNotebookPage(int page) const;

    // the additional page data (the pages themselves are in m_pages array)
    wxGtkNotebookPagesList m_pagesData;

    // we need to store the old selection since there
    // is no other way to know about it at the time
    // of the change selection event
    int m_oldSelection;

protected:
    // set all page's attributes
    virtual void DoApplyWidgetStyle(GtkRcStyle *style) wxOVERRIDE;
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const wxOVERRIDE;

    // remove one page from the notebook but do not destroy it
    virtual wxNotebookPage *DoRemovePage(size_t nPage) wxOVERRIDE;

    int DoSetSelection(size_t nPage, int flags = 0) wxOVERRIDE;

private:
    // the padding set by SetPadding()
    int m_padding;

    void Init();
    virtual void AddChildGTK(wxWindowGTK* child) wxOVERRIDE;

    wxDECLARE_DYNAMIC_CLASS(wxNotebook);
    wxDECLARE_EVENT_TABLE();
};

#endif // _WX_GTKNOTEBOOK_H_
