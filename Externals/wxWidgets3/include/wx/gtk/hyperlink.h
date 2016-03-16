/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/hyperlink.h
// Purpose:     Hyperlink control
// Author:      Francesco Montorsi
// Modified by:
// Created:     14/2/2007
// Copyright:   (c) 2007 Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKHYPERLINKCTRL_H_
#define _WX_GTKHYPERLINKCTRL_H_

#include "wx/generic/hyperlink.h"

#include "wx/scopedptr.h"

#ifdef __WXGTK3__
class wxHyperlinkCtrlColData;
#endif

// ----------------------------------------------------------------------------
// wxHyperlinkCtrl
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxHyperlinkCtrl : public wxGenericHyperlinkCtrl
{
    typedef wxGenericHyperlinkCtrl base_type;
public:
    // Constructors (notice that they can't be defined inline for this class
    // because of m_colData which uses incomplete wxHyperlinkCtrlColData).
    wxHyperlinkCtrl();
    wxHyperlinkCtrl(wxWindow *parent,
                    wxWindowID id,
                    const wxString& label, const wxString& url,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = wxHL_DEFAULT_STYLE,
                    const wxString& name = wxHyperlinkCtrlNameStr);

    virtual ~wxHyperlinkCtrl();

    // Creation function (for two-step construction).
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label, const wxString& url,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxHL_DEFAULT_STYLE,
                const wxString& name = wxHyperlinkCtrlNameStr);


    // get/set
    virtual wxColour GetHoverColour() const wxOVERRIDE;
    virtual void SetHoverColour(const wxColour &colour) wxOVERRIDE;

    virtual wxColour GetNormalColour() const wxOVERRIDE;
    virtual void SetNormalColour(const wxColour &colour) wxOVERRIDE;

    virtual wxColour GetVisitedColour() const wxOVERRIDE;
    virtual void SetVisitedColour(const wxColour &colour) wxOVERRIDE;

    virtual wxString GetURL() const wxOVERRIDE;
    virtual void SetURL(const wxString &url) wxOVERRIDE;

    virtual void SetVisited(bool visited = true) wxOVERRIDE;
    virtual bool GetVisited() const wxOVERRIDE;

    virtual void SetLabel(const wxString &label) wxOVERRIDE;

protected:
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
    virtual wxSize DoGetBestClientSize() const wxOVERRIDE;

    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const wxOVERRIDE;

private:
    enum LinkKind
    {
        Link_Normal,
        Link_Visited
    };

    void DoSetLinkColour(LinkKind linkKind, const wxColour& colour);

#ifdef __WXGTK3__
    wxScopedPtr<wxHyperlinkCtrlColData> m_colData;
#endif

    wxDECLARE_DYNAMIC_CLASS(wxHyperlinkCtrl);
};

#endif // _WX_GTKHYPERLINKCTRL_H_
