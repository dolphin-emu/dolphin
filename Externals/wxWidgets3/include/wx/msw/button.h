/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/button.h
// Purpose:     wxButton class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: button.h 67065 2011-02-27 12:48:26Z VZ $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BUTTON_H_
#define _WX_BUTTON_H_

// ----------------------------------------------------------------------------
// Pushbutton
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxButton : public wxButtonBase
{
public:
    wxButton() { Init(); }
    wxButton(wxWindow *parent,
             wxWindowID id,
             const wxString& label = wxEmptyString,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = 0,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxButtonNameStr)
    {
        Init();

        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxButtonNameStr);

    virtual ~wxButton();

    virtual wxWindow *SetDefault();

    // overridden base class methods
    virtual void SetLabel(const wxString& label);
    virtual bool SetBackgroundColour(const wxColour &colour);
    virtual bool SetForegroundColour(const wxColour &colour);

    // implementation from now on
    virtual void Command(wxCommandEvent& event);
    virtual WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
    virtual bool MSWCommand(WXUINT param, WXWORD id);

    virtual bool MSWOnDraw(WXDRAWITEMSTRUCT *item);
    virtual WXDWORD MSWGetStyle(long style, WXDWORD *exstyle) const;

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const { return false; }

protected:
    // send a notification event, return true if processed
    bool SendClickEvent();

    // default button handling
    void SetTmpDefault();
    void UnsetTmpDefault();

    // set or unset BS_DEFPUSHBUTTON style
    static void SetDefaultStyle(wxButton *btn, bool on);

    // usually overridden base class virtuals
    virtual wxSize DoGetBestSize() const;

    virtual bool DoGetAuthNeeded() const;
    virtual void DoSetAuthNeeded(bool show);
    virtual wxBitmap DoGetBitmap(State which) const;
    virtual void DoSetBitmap(const wxBitmap& bitmap, State which);
    virtual wxSize DoGetBitmapMargins() const;
    virtual void DoSetBitmapMargins(wxCoord x, wxCoord y);
    virtual void DoSetBitmapPosition(wxDirection dir);

#if wxUSE_MARKUP
    virtual bool DoSetLabelMarkup(const wxString& markup);
#endif // wxUSE_MARKUP

    // Increases the passed in size to account for the button image.
    //
    // Should only be called if we do have a button, i.e. if m_imageData is
    // non-NULL.
    void AdjustForBitmapSize(wxSize& size) const;

    class wxButtonImageData *m_imageData;

#if wxUSE_MARKUP
    class wxMarkupText *m_markupText;
#endif // wxUSE_MARKUP

    // true if the UAC symbol is shown
    bool m_authNeeded;

private:
    void Init()
    {
        m_imageData = NULL;
#if wxUSE_MARKUP
        m_markupText = NULL;
#endif // wxUSE_MARKUP
        m_authNeeded = false;
    }

    // Switches button into owner-drawn mode: this is used if we need to draw
    // something not supported by the native control, such as using non default
    // colours or a bitmap on pre-XP systems.
    void MakeOwnerDrawn();

    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxButton);
};

#endif // _WX_BUTTON_H_
