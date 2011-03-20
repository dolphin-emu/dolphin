/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/button.h
// Purpose:     wxGTK wxButton class declaration
// Author:      Robert Roebling
// Id:          $Id: button.h 67066 2011-02-27 12:48:30Z VZ $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_BUTTON_H_
#define _WX_GTK_BUTTON_H_

//-----------------------------------------------------------------------------
// wxButton
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxButton : public wxButtonBase
{
public:
    wxButton() { Init(); }
    wxButton(wxWindow *parent, wxWindowID id,
           const wxString& label = wxEmptyString,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxButtonNameStr)
    {
        Init();

        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
           const wxString& label = wxEmptyString,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxButtonNameStr);

    virtual wxWindow *SetDefault();
    virtual void SetLabel( const wxString &label );
    virtual bool Enable( bool enable = true );

    // implementation
    // --------------

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // helper to allow access to protected member from GTK callback
    void MoveWindow(int x, int y, int width, int height) { DoMoveWindow(x, y, width, height); }

    // called from GTK callbacks: they update the button state and call
    // GTKUpdateBitmap()
    void GTKMouseEnters();
    void GTKMouseLeaves();
    void GTKPressed();
    void GTKReleased();

protected:
    virtual wxSize DoGetBestSize() const;
    virtual void DoApplyWidgetStyle(GtkRcStyle *style);

    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

    virtual wxBitmap DoGetBitmap(State which) const;
    virtual void DoSetBitmap(const wxBitmap& bitmap, State which);
    virtual void DoSetBitmapPosition(wxDirection dir);

#if wxUSE_MARKUP
    virtual bool DoSetLabelMarkup(const wxString& markup);
#endif // wxUSE_MARKUP

private:
    typedef wxButtonBase base_type;

    // common part of all ctors
    void Init()
    {
        m_isCurrent =
        m_isPressed = false;
    }

    // focus event handler: calls GTKUpdateBitmap()
    void GTKOnFocus(wxFocusEvent& event);

    // update the bitmap to correspond to the current button state
    void GTKUpdateBitmap();

    // return the current button state from m_isXXX flags (which means that it
    // might not correspond to the real current state as e.g. m_isCurrent will
    // never be true if we don't have a valid current bitmap)
    State GTKGetCurrentState() const;

    // show the given bitmap (must be valid)
    void GTKDoShowBitmap(const wxBitmap& bitmap);

    // Return the GtkLabel used by this button.
    GtkLabel *GTKGetLabel() const;


    // the bitmaps for the different state of the buttons, all of them may be
    // invalid and the button only shows a bitmap at all if State_Normal bitmap
    // is valid
    wxBitmap m_bitmaps[State_Max];

    // true iff mouse is currently over the button
    bool m_isCurrent;

    // true iff the button is in pressed state
    bool m_isPressed;

    DECLARE_DYNAMIC_CLASS(wxButton)
};

#endif // _WX_GTK_BUTTON_H_
