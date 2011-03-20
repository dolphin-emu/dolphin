///////////////////////////////////////////////////////////////////////////////
// Name:        wx/containr.h
// Purpose:     wxControlContainer class declration: a "mix-in" class which
//              implements the TAB navigation between the controls
// Author:      Vadim Zeitlin
// Modified by:
// Created:     06.08.01
// RCS-ID:      $Id: containr.h 61508 2009-07-23 20:30:22Z VZ $
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CONTAINR_H_
#define _WX_CONTAINR_H_

#include "wx/defs.h"

class WXDLLIMPEXP_FWD_CORE wxWindow;
class WXDLLIMPEXP_FWD_CORE wxWindowBase;

/*
   Implementation note: wxControlContainer is not a real mix-in but rather
   a class meant to be aggregated with (and not inherited from). Although
   logically it should be a mix-in, doing it like this has no advantage from
   the point of view of the existing code but does have some problems (we'd
   need to play tricks with event handlers which may be difficult to do
   safely). The price we pay for this simplicity is the ugly macros below.
 */

// ----------------------------------------------------------------------------
// wxControlContainerBase: common part used in both native and generic cases
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxControlContainerBase
{
public:
    // default ctor, SetContainerWindow() must be called later
    wxControlContainerBase()
    {
        m_winParent = NULL;

        // do accept focus initially, we'll stop doing it if/when any children
        // are added
        m_acceptsFocus = true;
        m_inSetFocus = false;
        m_winLastFocused = NULL;
    }
    virtual ~wxControlContainerBase() {}

    void SetContainerWindow(wxWindow *winParent)
    {
        wxASSERT_MSG( !m_winParent, wxT("shouldn't be called twice") );

        m_winParent = winParent;
    }

    // should be called from SetFocus(), returns false if we did nothing with
    // the focus and the default processing should take place
    bool DoSetFocus();

    // should be called when we decide that we should [stop] accepting focus
    void SetCanFocus(bool acceptsFocus);

    // returns whether we should accept focus ourselves or not
    bool AcceptsFocus() const { return m_acceptsFocus; }

    // returns whether we or one of our children accepts focus: we always do
    // because if we don't have any focusable children it probably means that
    // we're not being used as a container at all (think of wxGrid or generic
    // wxListCtrl) and so should get focus for ourselves
    bool AcceptsFocusRecursively() const { return true; }

    // this is used to determine whether we can accept focus when Tab or
    // another navigation key is pressed -- we alsways can, for the same reason
    // as mentioned above for AcceptsFocusRecursively()
    bool AcceptsFocusFromKeyboard() const { return true; }

    // Call this when the number of children of the window changes.
    // If we have any children, this panel (used just as container for
    // them) shouldn't get focus for itself.
    void UpdateCanFocus() { SetCanFocus(!HasAnyFocusableChildren()); }

protected:
    // set the focus to the child which had it the last time
    virtual bool SetFocusToChild();

    // return true if we have any children accepting focus
    bool HasAnyFocusableChildren() const;

    // the parent window we manage the children for
    wxWindow *m_winParent;

    // the child which had the focus last time this panel was activated
    wxWindow *m_winLastFocused;

private:
    // value returned by AcceptsFocus(), should be changed using SetCanFocus()
    // only
    bool m_acceptsFocus;

    // a guard against infinite recursion
    bool m_inSetFocus;
};

// common part of WX_DECLARE_CONTROL_CONTAINER in the native and generic cases,
// it should be used in the wxWindow-derived class declaration
#define WX_DECLARE_CONTROL_CONTAINER_BASE()                                   \
public:                                                                       \
    virtual bool AcceptsFocus() const;                                        \
    virtual bool AcceptsFocusRecursively() const;                             \
    virtual bool AcceptsFocusFromKeyboard() const;                            \
    virtual void AddChild(wxWindowBase *child);                               \
    virtual void RemoveChild(wxWindowBase *child);                            \
    virtual void SetFocus();                                                  \
    void SetFocusIgnoringChildren();                                          \
    void AcceptFocus(bool acceptFocus)                                        \
    {                                                                         \
        m_container.SetCanFocus(acceptFocus);                                 \
    }                                                                         \
                                                                              \
protected:                                                                    \
    wxControlContainer m_container

// this macro must be used in the derived class ctor
#define WX_INIT_CONTROL_CONTAINER() \
    m_container.SetContainerWindow(this)

// common part of WX_DELEGATE_TO_CONTROL_CONTAINER in the native and generic
// cases, must be used in the wxWindow-derived class implementation
#define WX_DELEGATE_TO_CONTROL_CONTAINER_BASE(classname, basename)            \
    void classname::AddChild(wxWindowBase *child)                             \
    {                                                                         \
        basename::AddChild(child);                                            \
                                                                              \
        m_container.UpdateCanFocus();                                         \
    }                                                                         \
                                                                              \
    bool classname::AcceptsFocusRecursively() const                           \
    {                                                                         \
        return m_container.AcceptsFocusRecursively();                         \
    }                                                                         \
                                                                              \
    void classname::SetFocus()                                                \
    {                                                                         \
        if ( !m_container.DoSetFocus() )                                      \
            basename::SetFocus();                                             \
    }                                                                         \
                                                                              \
    bool classname::AcceptsFocus() const                                      \
    {                                                                         \
        return m_container.AcceptsFocus();                                    \
    }                                                                         \
                                                                              \
    bool classname::AcceptsFocusFromKeyboard() const                          \
    {                                                                         \
        return m_container.AcceptsFocusFromKeyboard();                        \
    }

#ifdef wxHAS_NATIVE_TAB_TRAVERSAL

// ----------------------------------------------------------------------------
// wxControlContainer for native TAB navigation
// ----------------------------------------------------------------------------

// this must be a real class as we forward-declare it elsewhere
class WXDLLIMPEXP_CORE wxControlContainer : public wxControlContainerBase
{
protected:
    // set the focus to the child which had it the last time
    virtual bool SetFocusToChild();
};

#define WX_EVENT_TABLE_CONTROL_CONTAINER(classname)

#define WX_DECLARE_CONTROL_CONTAINER WX_DECLARE_CONTROL_CONTAINER_BASE

#define WX_DELEGATE_TO_CONTROL_CONTAINER(classname, basename)                 \
    WX_DELEGATE_TO_CONTROL_CONTAINER_BASE(classname, basename)                \
                                                                              \
    void classname::RemoveChild(wxWindowBase *child)                          \
    {                                                                         \
        basename::RemoveChild(child);                                         \
                                                                              \
        m_container.UpdateCanFocus();                                         \
    }                                                                         \
                                                                              \
    void classname::SetFocusIgnoringChildren()                                \
    {                                                                         \
        basename::SetFocus();                                                 \
    }

#else // !wxHAS_NATIVE_TAB_TRAVERSAL

class WXDLLIMPEXP_FWD_CORE wxFocusEvent;
class WXDLLIMPEXP_FWD_CORE wxNavigationKeyEvent;

// ----------------------------------------------------------------------------
// wxControlContainer for TAB navigation implemented in wx itself
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxControlContainer : public wxControlContainerBase
{
public:
    // default ctor, SetContainerWindow() must be called later
    wxControlContainer();

    // the methods to be called from the window event handlers
    void HandleOnNavigationKey(wxNavigationKeyEvent& event);
    void HandleOnFocus(wxFocusEvent& event);
    void HandleOnWindowDestroy(wxWindowBase *child);

    // called from OnChildFocus() handler, i.e. when one of our (grand)
    // children gets the focus
    void SetLastFocus(wxWindow *win);

protected:

    wxDECLARE_NO_COPY_CLASS(wxControlContainer);
};

// ----------------------------------------------------------------------------
// macros which may be used by the classes wishing to implement TAB navigation
// among their children
// ----------------------------------------------------------------------------

// declare the methods to be forwarded
#define WX_DECLARE_CONTROL_CONTAINER()                                        \
    WX_DECLARE_CONTROL_CONTAINER_BASE();                                      \
                                                                              \
public:                                                                       \
    void OnNavigationKey(wxNavigationKeyEvent& event);                        \
    void OnFocus(wxFocusEvent& event);                                        \
    virtual void OnChildFocus(wxChildFocusEvent& event)

// implement the event table entries for wxControlContainer
#define WX_EVENT_TABLE_CONTROL_CONTAINER(classname) \
    EVT_SET_FOCUS(classname::OnFocus) \
    EVT_CHILD_FOCUS(classname::OnChildFocus) \
    EVT_NAVIGATION_KEY(classname::OnNavigationKey)

// implement the methods forwarding to the wxControlContainer
#define WX_DELEGATE_TO_CONTROL_CONTAINER(classname, basename)                 \
    WX_DELEGATE_TO_CONTROL_CONTAINER_BASE(classname, basename)                \
                                                                              \
    void classname::RemoveChild(wxWindowBase *child)                          \
    {                                                                         \
        m_container.HandleOnWindowDestroy(child);                             \
                                                                              \
        basename::RemoveChild(child);                                         \
                                                                              \
        m_container.UpdateCanFocus();                                         \
    }                                                                         \
                                                                              \
    void classname::OnNavigationKey( wxNavigationKeyEvent& event )            \
    {                                                                         \
        m_container.HandleOnNavigationKey(event);                             \
    }                                                                         \
                                                                              \
    void classname::SetFocusIgnoringChildren()                                \
    {                                                                         \
        basename::SetFocus();                                                 \
    }                                                                         \
                                                                              \
    void classname::OnChildFocus(wxChildFocusEvent& event)                    \
    {                                                                         \
        m_container.SetLastFocus(event.GetWindow());                          \
        event.Skip();                                                         \
    }                                                                         \
                                                                              \
    void classname::OnFocus(wxFocusEvent& event)                              \
    {                                                                         \
        m_container.HandleOnFocus(event);                                     \
    }

#endif // wxHAS_NATIVE_TAB_TRAVERSAL/!wxHAS_NATIVE_TAB_TRAVERSAL

// this function is for wxWidgets internal use only
extern bool wxSetFocusToChild(wxWindow *win, wxWindow **child);

#endif // _WX_CONTAINR_H_
