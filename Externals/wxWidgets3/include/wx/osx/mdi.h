/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/mdi.h
// Purpose:     MDI (Multiple Document Interface) classes.
// Author:      Stefan Csomor
// Modified by: 2008-10-31 Vadim Zeitlin: derive from the base classes
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
//              (c) 2008 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_CARBON_MDI_H_
#define _WX_OSX_CARBON_MDI_H_

class WXDLLIMPEXP_CORE wxMDIParentFrame : public wxMDIParentFrameBase
{
public:
    wxMDIParentFrame() { Init(); }
    wxMDIParentFrame(wxWindow *parent,
                     wxWindowID id,
                     const wxString& title,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     long style = wxDEFAULT_FRAME_STYLE | wxVSCROLL | wxHSCROLL,
                     const wxString& name = wxFrameNameStr)
    {
        Init();
        Create(parent, id, title, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE | wxVSCROLL | wxHSCROLL,
                const wxString& name = wxFrameNameStr);

    virtual ~wxMDIParentFrame();

    // implement/override base class [pure] virtuals
    // ---------------------------------------------

    static bool IsTDI() { return false; }

    virtual void AddChild(wxWindowBase *child) wxOVERRIDE;
    virtual void RemoveChild(wxWindowBase *child) wxOVERRIDE;

    virtual void ActivateNext() wxOVERRIDE { /* TODO */ }
    virtual void ActivatePrevious() wxOVERRIDE { /* TODO */ }

    virtual bool Show(bool show = true) wxOVERRIDE;


    // Mac-specific implementation from now on
    // ---------------------------------------

    // Mac OS activate event
    virtual void MacActivate(long timestamp, bool activating) wxOVERRIDE;

    // wxWidgets activate event
    void OnActivate(wxActivateEvent& event);
    void OnSysColourChanged(wxSysColourChangedEvent& event);

    void SetMenuBar(wxMenuBar *menu_bar) wxOVERRIDE;

    // Get rect to be used to center top-level children
    virtual void GetRectForTopLevelChildren(int *x, int *y, int *w, int *h) wxOVERRIDE;

protected:
    // common part of all ctors
    void Init();

    // returns true if this frame has some contents and so should be visible,
    // false if it's used solely as container for its children
    bool ShouldBeVisible() const;


    wxMenu *m_windowMenu;

    // true if MDI Frame is intercepting commands, not child
    bool m_parentFrameActive;

    // true if the frame should be shown but is not because it is empty and
    // useless otherwise than a container for its children
    bool m_shouldBeShown;

private:
    friend class WXDLLIMPEXP_FWD_CORE wxMDIChildFrame;

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS(wxMDIParentFrame);
};

class WXDLLIMPEXP_CORE wxMDIChildFrame : public wxMDIChildFrameBase
{
public:
    wxMDIChildFrame() { Init(); }
    wxMDIChildFrame(wxMDIParentFrame *parent,
                    wxWindowID id,
                    const wxString& title,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = wxDEFAULT_FRAME_STYLE,
                    const wxString& name = wxFrameNameStr)
    {
        Init() ;
        Create(parent, id, title, pos, size, style, name);
    }

    bool Create(wxMDIParentFrame *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxFrameNameStr);

    virtual ~wxMDIChildFrame();

    // un-override the base class override
    virtual bool IsTopLevel() const { return true; }

    // implement MDI operations
    virtual void Activate();


    // Mac OS activate event
    virtual void MacActivate(long timestamp, bool activating);

protected:
    // common part of all ctors
    void Init();

    wxDECLARE_DYNAMIC_CLASS(wxMDIChildFrame);
};

class WXDLLIMPEXP_CORE wxMDIClientWindow : public wxMDIClientWindowBase
{
public:
    wxMDIClientWindow() { }
    virtual ~wxMDIClientWindow();

    virtual bool CreateClient(wxMDIParentFrame *parent,
                              long style = wxVSCROLL | wxHSCROLL);

protected:
    virtual void DoGetClientSize(int *width, int *height) const;

    wxDECLARE_DYNAMIC_CLASS(wxMDIClientWindow);
};

#endif // _WX_OSX_CARBON_MDI_H_
