/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/button.cpp
// Purpose:     wxButton
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: button.cpp 67284 2011-03-22 17:15:34Z VZ $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_BUTTON

#include "wx/button.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/brush.h"
    #include "wx/panel.h"
    #include "wx/bmpbuttn.h"
    #include "wx/settings.h"
    #include "wx/dcscreen.h"
    #include "wx/dcclient.h"
    #include "wx/toplevel.h"
    #include "wx/msw/wrapcctl.h"
    #include "wx/msw/private.h"
    #include "wx/msw/missing.h"
#endif

#include "wx/imaglist.h"
#include "wx/stockitem.h"
#include "wx/msw/private/button.h"
#include "wx/msw/private/dc.h"
#include "wx/private/window.h"

#if wxUSE_MARKUP
    #include "wx/generic/private/markuptext.h"
#endif // wxUSE_MARKUP

using namespace wxMSWImpl;

#if wxUSE_UXTHEME
    #include "wx/msw/uxtheme.h"

    // no need to include tmschema.h
    #ifndef BP_PUSHBUTTON
        #define BP_PUSHBUTTON 1

        #define PBS_NORMAL    1
        #define PBS_HOT       2
        #define PBS_PRESSED   3
        #define PBS_DISABLED  4
        #define PBS_DEFAULTED 5

        #define TMT_CONTENTMARGINS 3602
    #endif

    // provide the necessary declarations ourselves if they're missing from
    // headers
    #ifndef BCM_SETIMAGELIST
        #define BCM_SETIMAGELIST    0x1602
        #define BCM_SETTEXTMARGIN   0x1604

        enum
        {
            BUTTON_IMAGELIST_ALIGN_LEFT,
            BUTTON_IMAGELIST_ALIGN_RIGHT,
            BUTTON_IMAGELIST_ALIGN_TOP,
            BUTTON_IMAGELIST_ALIGN_BOTTOM
        };

        struct BUTTON_IMAGELIST
        {
            HIMAGELIST himl;
            RECT margin;
            UINT uAlign;
        };
    #endif
#endif // wxUSE_UXTHEME

#ifndef WM_THEMECHANGED
    #define WM_THEMECHANGED     0x031A
#endif

#ifndef ODS_NOACCEL
    #define ODS_NOACCEL         0x0100
#endif

#ifndef ODS_NOFOCUSRECT
    #define ODS_NOFOCUSRECT     0x0200
#endif

#ifndef DT_HIDEPREFIX
    #define DT_HIDEPREFIX       0x00100000
#endif

// set the value for BCM_SETSHIELD (for the UAC shield) if it's not defined in
// the header
#ifndef BCM_SETSHIELD
    #define BCM_SETSHIELD       0x160c
#endif

#if wxUSE_UXTHEME
extern wxWindowMSW *wxWindowBeingErased; // From src/msw/window.cpp
#endif // wxUSE_UXTHEME

// ----------------------------------------------------------------------------
// button image data
// ----------------------------------------------------------------------------

// we use different data classes for owner drawn buttons and for themed XP ones

class wxButtonImageData
{
public:
    wxButtonImageData() { }
    virtual ~wxButtonImageData() { }

    virtual wxBitmap GetBitmap(wxButton::State which) const = 0;
    virtual void SetBitmap(const wxBitmap& bitmap, wxButton::State which) = 0;

    virtual wxSize GetBitmapMargins() const = 0;
    virtual void SetBitmapMargins(wxCoord x, wxCoord y) = 0;

    virtual wxDirection GetBitmapPosition() const = 0;
    virtual void SetBitmapPosition(wxDirection dir) = 0;

private:
    wxDECLARE_NO_COPY_CLASS(wxButtonImageData);
};

namespace
{

// the gap between button edge and the interior area used by Windows for the
// standard buttons
const int OD_BUTTON_MARGIN = 4;

class wxODButtonImageData : public wxButtonImageData
{
public:
    wxODButtonImageData(wxButton *btn, const wxBitmap& bitmap)
    {
        SetBitmap(bitmap, wxButton::State_Normal);
        SetBitmap(bitmap.ConvertToDisabled(), wxButton::State_Disabled);

        m_dir = wxLEFT;

        // we use margins when we have both bitmap and text, but when we have
        // only the bitmap it should take up the entire button area
        if ( btn->ShowsLabel() )
        {
            m_margin.x = btn->GetCharWidth();
            m_margin.y = btn->GetCharHeight() / 2;
        }
    }

    virtual wxBitmap GetBitmap(wxButton::State which) const
    {
        return m_bitmaps[which];
    }

    virtual void SetBitmap(const wxBitmap& bitmap, wxButton::State which)
    {
        m_bitmaps[which] = bitmap;
    }

    virtual wxSize GetBitmapMargins() const
    {
        return m_margin;
    }

    virtual void SetBitmapMargins(wxCoord x, wxCoord y)
    {
        m_margin = wxSize(x, y);
    }

    virtual wxDirection GetBitmapPosition() const
    {
        return m_dir;
    }

    virtual void SetBitmapPosition(wxDirection dir)
    {
        m_dir = dir;
    }

private:
    // just store the values passed to us to be able to retrieve them later
    // from the drawing code
    wxBitmap m_bitmaps[wxButton::State_Max];
    wxSize m_margin;
    wxDirection m_dir;

    wxDECLARE_NO_COPY_CLASS(wxODButtonImageData);
};

#if wxUSE_UXTHEME

// somehow the margin is one pixel greater than the value returned by
// GetThemeMargins() call
const int XP_BUTTON_EXTRA_MARGIN = 1;

class wxXPButtonImageData : public wxButtonImageData
{
public:
    // we must be constructed with the size of our images as we need to create
    // the image list
    wxXPButtonImageData(wxButton *btn, const wxBitmap& bitmap)
        : m_iml(bitmap.GetWidth(), bitmap.GetHeight(), true /* use mask */,
                wxButton::State_Max),
          m_hwndBtn(GetHwndOf(btn))
    {
        // initialize all bitmaps except for the disabled one to normal state
        for ( int n = 0; n < wxButton::State_Max; n++ )
        {
            m_iml.Add(n == wxButton::State_Disabled ? bitmap.ConvertToDisabled()
                                                    : bitmap);
        }

        m_data.himl = GetHimagelistOf(&m_iml);

        // no margins by default
        m_data.margin.left =
        m_data.margin.right =
        m_data.margin.top =
        m_data.margin.bottom = 0;

        // use default alignment
        m_data.uAlign = BUTTON_IMAGELIST_ALIGN_LEFT;

        UpdateImageInfo();
    }

    virtual wxBitmap GetBitmap(wxButton::State which) const
    {
        return m_iml.GetBitmap(which);
    }

    virtual void SetBitmap(const wxBitmap& bitmap, wxButton::State which)
    {
        m_iml.Replace(which, bitmap);

        UpdateImageInfo();
    }

    virtual wxSize GetBitmapMargins() const
    {
        return wxSize(m_data.margin.left, m_data.margin.top);
    }

    virtual void SetBitmapMargins(wxCoord x, wxCoord y)
    {
        RECT& margin = m_data.margin;
        margin.left =
        margin.right = x;
        margin.top =
        margin.bottom = y;

        if ( !::SendMessage(m_hwndBtn, BCM_SETTEXTMARGIN, 0, (LPARAM)&margin) )
        {
            wxLogDebug("SendMessage(BCM_SETTEXTMARGIN) failed");
        }
    }

    virtual wxDirection GetBitmapPosition() const
    {
        switch ( m_data.uAlign )
        {
            default:
                wxFAIL_MSG( "invalid image alignment" );
                // fall through

            case BUTTON_IMAGELIST_ALIGN_LEFT:
                return wxLEFT;

            case BUTTON_IMAGELIST_ALIGN_RIGHT:
                return wxRIGHT;

            case BUTTON_IMAGELIST_ALIGN_TOP:
                return wxTOP;

            case BUTTON_IMAGELIST_ALIGN_BOTTOM:
                return wxBOTTOM;
        }
    }

    virtual void SetBitmapPosition(wxDirection dir)
    {
        UINT alignNew;
        switch ( dir )
        {
            default:
                wxFAIL_MSG( "invalid direction" );
                // fall through

            case wxLEFT:
                alignNew = BUTTON_IMAGELIST_ALIGN_LEFT;
                break;

            case wxRIGHT:
                alignNew = BUTTON_IMAGELIST_ALIGN_RIGHT;
                break;

            case wxTOP:
                alignNew = BUTTON_IMAGELIST_ALIGN_TOP;
                break;

            case wxBOTTOM:
                alignNew = BUTTON_IMAGELIST_ALIGN_BOTTOM;
                break;
        }

        if ( alignNew != m_data.uAlign )
        {
            m_data.uAlign = alignNew;
            UpdateImageInfo();
        }
    }

private:
    void UpdateImageInfo()
    {
        if ( !::SendMessage(m_hwndBtn, BCM_SETIMAGELIST, 0, (LPARAM)&m_data) )
        {
            wxLogDebug("SendMessage(BCM_SETIMAGELIST) failed");
        }
    }

    // we store image list separately to be able to use convenient wxImageList
    // methods instead of working with raw HIMAGELIST
    wxImageList m_iml;

    // store the rest of the data in BCM_SETIMAGELIST-friendly form
    BUTTON_IMAGELIST m_data;

    // the button we're associated with
    const HWND m_hwndBtn;


    wxDECLARE_NO_COPY_CLASS(wxXPButtonImageData);
};

#endif // wxUSE_UXTHEME

} // anonymous namespace

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// helper functions from wx/msw/private/button.h
// ----------------------------------------------------------------------------

void wxMSWButton::UpdateMultilineStyle(HWND hwnd, const wxString& label)
{
    // update BS_MULTILINE style depending on the new label (resetting it
    // doesn't seem to do anything very useful but it shouldn't hurt and we do
    // have to set it whenever the label becomes multi line as otherwise it
    // wouldn't be shown correctly as we don't use BS_MULTILINE when creating
    // the control unless it already has new lines in its label)
    long styleOld = ::GetWindowLong(hwnd, GWL_STYLE),
         styleNew;
    if ( label.find(wxT('\n')) != wxString::npos )
        styleNew = styleOld | BS_MULTILINE;
    else
        styleNew = styleOld & ~BS_MULTILINE;

    if ( styleNew != styleOld )
        ::SetWindowLong(hwnd, GWL_STYLE, styleNew);
}

wxSize wxMSWButton::GetFittingSize(wxWindow *win,
                                   const wxSize& sizeLabel,
                                   int flags)
{
    // FIXME: this is pure guesswork, need to retrieve the real button margins
    wxSize sizeBtn = sizeLabel;

    sizeBtn.x += 3*win->GetCharWidth();
    sizeBtn.y += win->GetCharHeight()/2;

    // account for the shield UAC icon if we have it
    if ( flags & Size_AuthNeeded )
        sizeBtn.x += wxSystemSettings::GetMetric(wxSYS_SMALLICON_X);

    return sizeBtn;
}

wxSize wxMSWButton::ComputeBestFittingSize(wxControl *btn, int flags)
{
    wxClientDC dc(btn);

    wxSize sizeBtn;
    dc.GetMultiLineTextExtent(btn->GetLabelText(), &sizeBtn.x, &sizeBtn.y);

    return GetFittingSize(btn, sizeBtn, flags);
}

wxSize wxMSWButton::IncreaseToStdSizeAndCache(wxControl *btn, const wxSize& size)
{
    wxSize sizeBtn(size);

    // All buttons have at least the standard height and, unless the user
    // explicitly wants them to be as small as possible and used wxBU_EXACTFIT
    // style to indicate this, of at least the standard width too.
    //
    // Notice that we really want to make all buttons equally high, otherwise
    // they look ugly and the existing code using wxBU_EXACTFIT only uses it to
    // control width and not height.

    // The 50x14 button size is documented in the "Recommended sizing and
    // spacing" section of MSDN layout article.
    //
    // Note that we intentionally don't use GetDefaultSize() here, because
    // it's inexact -- dialog units depend on this dialog's font.
    const wxSize sizeDef = btn->ConvertDialogToPixels(wxSize(50, 14));
    if ( !btn->HasFlag(wxBU_EXACTFIT) )
    {
        if ( sizeBtn.x < sizeDef.x )
            sizeBtn.x = sizeDef.x;
    }
    if ( sizeBtn.y < sizeDef.y )
        sizeBtn.y = sizeDef.y;

    btn->CacheBestSize(sizeBtn);

    return sizeBtn;
}

// ----------------------------------------------------------------------------
// creation/destruction
// ----------------------------------------------------------------------------

bool wxButton::Create(wxWindow *parent,
                      wxWindowID id,
                      const wxString& lbl,
                      const wxPoint& pos,
                      const wxSize& size,
                      long style,
                      const wxValidator& validator,
                      const wxString& name)
{
    wxString label(lbl);
    if (label.empty() && wxIsStockID(id))
    {
        // On Windows, some buttons aren't supposed to have mnemonics
        label = wxGetStockLabel
                (
                    id,
                    id == wxID_OK || id == wxID_CANCEL || id == wxID_CLOSE
                        ? wxSTOCK_NOFLAGS
                        : wxSTOCK_WITH_MNEMONIC
                );
    }

    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    WXDWORD exstyle;
    WXDWORD msStyle = MSWGetStyle(style, &exstyle);

    // if the label contains several lines we must explicitly tell the button
    // about it or it wouldn't draw it correctly ("\n"s would just appear as
    // black boxes)
    //
    // NB: we do it here and not in MSWGetStyle() because we need the label
    //     value and the label is not set yet when MSWGetStyle() is called
    msStyle |= wxMSWButton::GetMultilineStyle(label);

    return MSWCreateControl(wxT("BUTTON"), msStyle, pos, size, label, exstyle);
}

wxButton::~wxButton()
{
    wxTopLevelWindow *tlw = wxDynamicCast(wxGetTopLevelParent(this), wxTopLevelWindow);
    if ( tlw && tlw->GetTmpDefaultItem() == this )
    {
        UnsetTmpDefault();
    }

    delete m_imageData;
#if wxUSE_MARKUP
    delete m_markupText;
#endif // wxUSE_MARKUP
}

// ----------------------------------------------------------------------------
// flags
// ----------------------------------------------------------------------------

WXDWORD wxButton::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    // buttons never have an external border, they draw their own one
    WXDWORD msStyle = wxControl::MSWGetStyle
                      (
                        (style & ~wxBORDER_MASK) | wxBORDER_NONE, exstyle
                      );

    // we must use WS_CLIPSIBLINGS with the buttons or they would draw over
    // each other in any resizeable dialog which has more than one button in
    // the bottom
    msStyle |= WS_CLIPSIBLINGS;

    // don't use "else if" here: weird as it is, but you may combine wxBU_LEFT
    // and wxBU_RIGHT to get BS_CENTER!
    if ( style & wxBU_LEFT )
        msStyle |= BS_LEFT;
    if ( style & wxBU_RIGHT )
        msStyle |= BS_RIGHT;
    if ( style & wxBU_TOP )
        msStyle |= BS_TOP;
    if ( style & wxBU_BOTTOM )
        msStyle |= BS_BOTTOM;
#ifndef __WXWINCE__
    // flat 2d buttons
    if ( style & wxNO_BORDER )
        msStyle |= BS_FLAT;
#endif // __WXWINCE__

    return msStyle;
}

void wxButton::SetLabel(const wxString& label)
{
    wxMSWButton::UpdateMultilineStyle(GetHwnd(), label);

    wxButtonBase::SetLabel(label);

#if wxUSE_MARKUP
    // If we have a plain text label, we shouldn't be using markup any longer.
    if ( m_markupText )
    {
        delete m_markupText;
        m_markupText = NULL;

        // Unfortunately we don't really know whether we can reset the button
        // to be non-owner-drawn or not: if we had made it owner-drawn just
        // because of a call to SetLabelMarkup(), we could, but not if there
        // were [also] calls to Set{Fore,Back}groundColour(). If it's really a
        // problem to have button remain owner-drawn forever just because it
        // had markup label once, we should record the reason for our current
        // owner-drawnness and check it here.
    }
#endif // wxUSE_MARKUP
}

// ----------------------------------------------------------------------------
// size management including autosizing
// ----------------------------------------------------------------------------

void wxButton::AdjustForBitmapSize(wxSize &size) const
{
    wxCHECK_RET( m_imageData, wxT("shouldn't be called if no image") );

    // account for the bitmap size
    const wxSize sizeBmp = m_imageData->GetBitmap(State_Normal).GetSize();
    const wxDirection dirBmp = m_imageData->GetBitmapPosition();
    if ( dirBmp == wxLEFT || dirBmp == wxRIGHT )
    {
        size.x += sizeBmp.x;
        if ( sizeBmp.y > size.y )
            size.y = sizeBmp.y;
    }
    else // bitmap on top/below the text
    {
        size.y += sizeBmp.y;
        if ( sizeBmp.x > size.x )
            size.x = sizeBmp.x;
    }

    // account for the user-specified margins
    size += 2*m_imageData->GetBitmapMargins();

    // and also for the margins we always add internally (unless we have no
    // border at all in which case the button has exactly the same size as
    // bitmap and so no margins should be used)
    if ( !HasFlag(wxBORDER_NONE) )
    {
        int marginH = 0,
            marginV = 0;
#if wxUSE_UXTHEME
        if ( wxUxThemeEngine::GetIfActive() )
        {
            wxUxThemeHandle theme(const_cast<wxButton *>(this), L"BUTTON");

            MARGINS margins;
            wxUxThemeEngine::Get()->GetThemeMargins(theme, NULL,
                                                    BP_PUSHBUTTON,
                                                    PBS_NORMAL,
                                                    TMT_CONTENTMARGINS,
                                                    NULL,
                                                    &margins);

            // XP doesn't draw themed buttons correctly when the client
            // area is smaller than 8x8 - enforce this minimum size for
            // small bitmaps
            size.IncTo(wxSize(8, 8));

            marginH = margins.cxLeftWidth + margins.cxRightWidth
                        + 2*XP_BUTTON_EXTRA_MARGIN;
            marginV = margins.cyTopHeight + margins.cyBottomHeight
                        + 2*XP_BUTTON_EXTRA_MARGIN;
        }
        else
#endif // wxUSE_UXTHEME
        {
            marginH =
            marginV = OD_BUTTON_MARGIN;
        }

        size.IncBy(marginH, marginV);
    }
}

wxSize wxButton::DoGetBestSize() const
{
    wxButton * const self = const_cast<wxButton *>(this);

    wxSize size;

    // Account for the text part if we have it.
    if ( ShowsLabel() )
    {
        int flags = 0;
        if ( GetAuthNeeded() )
            flags |= wxMSWButton::Size_AuthNeeded;

#if wxUSE_MARKUP
        if ( m_markupText )
        {
            wxClientDC dc(self);
            size = wxMSWButton::GetFittingSize(self,
                                               m_markupText->Measure(dc),
                                               flags);
        }
        else // Normal plain text (but possibly multiline) label.
#endif // wxUSE_MARKUP
        {
            size = wxMSWButton::ComputeBestFittingSize(self, flags);
        }
    }

    if ( m_imageData )
        AdjustForBitmapSize(size);

    return wxMSWButton::IncreaseToStdSizeAndCache(self, size);
}

/* static */
wxSize wxButtonBase::GetDefaultSize()
{
    static wxSize s_sizeBtn;

    if ( s_sizeBtn.x == 0 )
    {
        wxScreenDC dc;
        dc.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));

        // The size of a standard button in the dialog units is 50x14,
        // translate this to pixels.
        //
        // Windows' computes dialog units using average character width over
        // upper- and lower-case ASCII alphabet and not using the average
        // character width metadata stored in the font; see
        // http://support.microsoft.com/default.aspx/kb/145994 for detailed
        // discussion.
        //
        // NB: wxMulDivInt32() is used, because it correctly rounds the result

        const wxSize base = wxPrivate::GetAverageASCIILetterSize(dc);
        s_sizeBtn.x = wxMulDivInt32(50, base.x, 4);
        s_sizeBtn.y = wxMulDivInt32(14, base.y, 8);
    }

    return s_sizeBtn;
}

// ----------------------------------------------------------------------------
// default button handling
// ----------------------------------------------------------------------------

/*
   The comment below and all this code is probably due to not using WM_NEXTDLGCTL
   message when changing focus (but just SetFocus() which is not enough), see
   http://blogs.msdn.com/oldnewthing/archive/2004/08/02/205624.aspx for the
   full explanation.

   TODO: Do use WM_NEXTDLGCTL and get rid of all this code.


   "Everything you ever wanted to know about the default buttons" or "Why do we
   have to do all this?"

   In MSW the default button should be activated when the user presses Enter
   and the current control doesn't process Enter itself somehow. This is
   handled by ::DefWindowProc() (or maybe ::DefDialogProc()) using DM_SETDEFID
   Another aspect of "defaultness" is that the default button has different
   appearance: this is due to BS_DEFPUSHBUTTON style which is completely
   separate from DM_SETDEFID stuff (!). Also note that BS_DEFPUSHBUTTON should
   be unset if our parent window is not active so it should be unset whenever
   we lose activation and set back when we regain it.

   Final complication is that when a button is active, it should be the default
   one, i.e. pressing Enter on a button always activates it and not another
   one.

   We handle this by maintaining a permanent and a temporary default items in
   wxControlContainer (both may be NULL). When a button becomes the current
   control (i.e. gets focus) it sets itself as the temporary default which
   ensures that it has the right appearance and that Enter will be redirected
   to it. When the button loses focus, it unsets the temporary default and so
   the default item will be the permanent default -- that is the default button
   if any had been set or none otherwise, which is just what we want.

   NB: all this is quite complicated by now and the worst is that normally
       it shouldn't be necessary at all as for the normal Windows programs
       DefWindowProc() and IsDialogMessage() take care of all this
       automatically -- however in wxWidgets programs this doesn't work for
       nested hierarchies (i.e. a notebook inside a notebook) for unknown
       reason and so we have to reproduce all this code ourselves. It would be
       very nice if we could avoid doing it.
 */

// set this button as the (permanently) default one in its panel
wxWindow *wxButton::SetDefault()
{
    // set this one as the default button both for wxWidgets ...
    wxWindow *winOldDefault = wxButtonBase::SetDefault();

    // ... and Windows
    SetDefaultStyle(wxDynamicCast(winOldDefault, wxButton), false);
    SetDefaultStyle(this, true);

    return winOldDefault;
}

// return the top level parent window if it's not being deleted yet, otherwise
// return NULL
static wxTopLevelWindow *GetTLWParentIfNotBeingDeleted(wxWindow *win)
{
    for ( ;; )
    {
        // IsTopLevel() will return false for a wxTLW being deleted, so we also
        // need the parent test for this case
        wxWindow * const parent = win->GetParent();
        if ( !parent || win->IsTopLevel() )
        {
            if ( win->IsBeingDeleted() )
                return NULL;

            break;
        }

        win = parent;
    }

    wxASSERT_MSG( win, wxT("button without top level parent?") );

    wxTopLevelWindow * const tlw = wxDynamicCast(win, wxTopLevelWindow);
    wxASSERT_MSG( tlw, wxT("logic error in GetTLWParentIfNotBeingDeleted()") );

    return tlw;
}

// set this button as being currently default
void wxButton::SetTmpDefault()
{
    wxTopLevelWindow * const tlw = GetTLWParentIfNotBeingDeleted(GetParent());
    if ( !tlw )
        return;

    wxWindow *winOldDefault = tlw->GetDefaultItem();
    tlw->SetTmpDefaultItem(this);

    SetDefaultStyle(wxDynamicCast(winOldDefault, wxButton), false);
    SetDefaultStyle(this, true);
}

// unset this button as currently default, it may still stay permanent default
void wxButton::UnsetTmpDefault()
{
    wxTopLevelWindow * const tlw = GetTLWParentIfNotBeingDeleted(GetParent());
    if ( !tlw )
        return;

    tlw->SetTmpDefaultItem(NULL);

    wxWindow *winOldDefault = tlw->GetDefaultItem();

    SetDefaultStyle(this, false);
    SetDefaultStyle(wxDynamicCast(winOldDefault, wxButton), true);
}

/* static */
void
wxButton::SetDefaultStyle(wxButton *btn, bool on)
{
    // we may be called with NULL pointer -- simpler to do the check here than
    // in the caller which does wxDynamicCast()
    if ( !btn )
        return;

    // first, let DefDlgProc() know about the new default button
    if ( on )
    {
        // we shouldn't set BS_DEFPUSHBUTTON for any button if we don't have
        // focus at all any more
        if ( !wxTheApp->IsActive() )
            return;

        wxWindow * const tlw = wxGetTopLevelParent(btn);
        wxCHECK_RET( tlw, wxT("button without top level window?") );

        ::SendMessage(GetHwndOf(tlw), DM_SETDEFID, btn->GetId(), 0L);

        // sending DM_SETDEFID also changes the button style to
        // BS_DEFPUSHBUTTON so there is nothing more to do
    }

    // then also change the style as needed
    long style = ::GetWindowLong(GetHwndOf(btn), GWL_STYLE);
    if ( !(style & BS_DEFPUSHBUTTON) == on )
    {
        // don't do it with the owner drawn buttons because it will
        // reset BS_OWNERDRAW style bit too (as BS_OWNERDRAW &
        // BS_DEFPUSHBUTTON != 0)!
        if ( (style & BS_OWNERDRAW) != BS_OWNERDRAW )
        {
            ::SendMessage(GetHwndOf(btn), BM_SETSTYLE,
                          on ? style | BS_DEFPUSHBUTTON
                             : style & ~BS_DEFPUSHBUTTON,
                          1L /* redraw */);
        }
        else // owner drawn
        {
            // redraw the button - it will notice itself that it's
            // [not] the default one [any longer]
            btn->Refresh();
        }
    }
    //else: already has correct style
}

// ----------------------------------------------------------------------------
// helpers
// ----------------------------------------------------------------------------

bool wxButton::SendClickEvent()
{
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, GetId());
    event.SetEventObject(this);

    return ProcessCommand(event);
}

void wxButton::Command(wxCommandEvent & event)
{
    ProcessCommand(event);
}

// ----------------------------------------------------------------------------
// event/message handlers
// ----------------------------------------------------------------------------

bool wxButton::MSWCommand(WXUINT param, WXWORD WXUNUSED(id))
{
    bool processed = false;
    switch ( param )
    {
        // NOTE: Apparently older versions (NT 4?) of the common controls send
        //       BN_DOUBLECLICKED but not a second BN_CLICKED for owner-drawn
        //       buttons, so in order to send two EVT_BUTTON events we should
        //       catch both types.  Currently (Feb 2003) up-to-date versions of
        //       win98, win2k and winXP all send two BN_CLICKED messages for
        //       all button types, so we don't catch BN_DOUBLECLICKED anymore
        //       in order to not get 3 EVT_BUTTON events.  If this is a problem
        //       then we need to figure out which version of the comctl32 changed
        //       this behaviour and test for it.

        case 1:                     // message came from an accelerator
        case BN_CLICKED:            // normal buttons send this
            processed = SendClickEvent();
            break;
    }

    return processed;
}

WXLRESULT wxButton::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    // when we receive focus, we want to temporarily become the default button in
    // our parent panel so that pressing "Enter" would activate us -- and when
    // losing it we should restore the previous default button as well
    if ( nMsg == WM_SETFOCUS )
    {
        SetTmpDefault();

        // let the default processing take place too
    }
    else if ( nMsg == WM_KILLFOCUS )
    {
        UnsetTmpDefault();
    }
    else if ( nMsg == WM_LBUTTONDBLCLK )
    {
        // emulate a click event to force an owner-drawn button to change its
        // appearance - without this, it won't do it
        (void)wxControl::MSWWindowProc(WM_LBUTTONDOWN, wParam, lParam);

        // and continue with processing the message normally as well
    }
#if wxUSE_UXTHEME
    else if ( nMsg == WM_THEMECHANGED )
    {
        // need to recalculate the best size here
        // as the theme size might have changed
        InvalidateBestSize();
    }
#endif // wxUSE_UXTHEME
    // must use m_mouseInWindow here instead of IsMouseInWindow()
    // since we need to know the first time the mouse enters the window
    // and IsMouseInWindow() would return true in this case
    else if ( (nMsg == WM_MOUSEMOVE && !m_mouseInWindow) ||
                nMsg == WM_MOUSELEAVE )
    {
        if (
                IsEnabled() &&
                (
#if wxUSE_UXTHEME
                wxUxThemeEngine::GetIfActive() ||
#endif // wxUSE_UXTHEME
                 (m_imageData && m_imageData->GetBitmap(State_Current).IsOk())
                )
           )
        {
            Refresh();
        }
    }

    // let the base class do all real processing
    return wxControl::MSWWindowProc(nMsg, wParam, lParam);
}

// ----------------------------------------------------------------------------
// authentication needed handling
// ----------------------------------------------------------------------------

bool wxButton::DoGetAuthNeeded() const
{
    return m_authNeeded;
}

void wxButton::DoSetAuthNeeded(bool show)
{
    // show/hide UAC symbol on Windows Vista and later
    if ( wxGetWinVersion() >= wxWinVersion_6 )
    {
        m_authNeeded = show;
        ::SendMessage(GetHwnd(), BCM_SETSHIELD, 0, show);
        InvalidateBestSize();
    }
}

// ----------------------------------------------------------------------------
// button images
// ----------------------------------------------------------------------------

wxBitmap wxButton::DoGetBitmap(State which) const
{
    return m_imageData ? m_imageData->GetBitmap(which) : wxBitmap();
}

void wxButton::DoSetBitmap(const wxBitmap& bitmap, State which)
{
#if wxUSE_UXTHEME
    wxXPButtonImageData *oldData = NULL;
#endif // wxUSE_UXTHEME

    // Check if we already had bitmaps of different size.
    if ( m_imageData &&
          bitmap.GetSize() != m_imageData->GetBitmap(State_Normal).GetSize() )
    {
        wxASSERT_MSG( which == State_Normal,
                      "Must set normal bitmap with the new size first" );

#if wxUSE_UXTHEME
        if ( ShowsLabel() && wxUxThemeEngine::GetIfActive() )
        {
            // We can't change the size of the images stored in wxImageList
            // in wxXPButtonImageData::m_iml so force recreating it below but
            // keep the current data to copy its values into the new one.
            oldData = static_cast<wxXPButtonImageData *>(m_imageData);
            m_imageData = NULL;
        }
#endif // wxUSE_UXTHEME
        //else: wxODButtonImageData doesn't require anything special
    }

    // allocate the image data when the first bitmap is set
    if ( !m_imageData )
    {
#if wxUSE_UXTHEME
        // using image list doesn't work correctly if we don't have any label
        // (even if we use BUTTON_IMAGELIST_ALIGN_CENTER alignment and
        // BS_BITMAP style), at least under Windows 2003 so use owner drawn
        // strategy for bitmap-only buttons
        if ( ShowsLabel() && wxUxThemeEngine::GetIfActive() )
        {
            m_imageData = new wxXPButtonImageData(this, bitmap);

            if ( oldData )
            {
                // Preserve the old values in case the user changed them.
                m_imageData->SetBitmapPosition(oldData->GetBitmapPosition());

                const wxSize oldMargins = oldData->GetBitmapMargins();
                m_imageData->SetBitmapMargins(oldMargins.x, oldMargins.y);

                // No need to preserve the bitmaps though as they were of wrong
                // size anyhow.

                delete oldData;
            }
        }
        else
#endif // wxUSE_UXTHEME
        {
            m_imageData = new wxODButtonImageData(this, bitmap);
            MakeOwnerDrawn();
        }
    }
    else
    {
        m_imageData->SetBitmap(bitmap, which);
    }

    // it should be enough to only invalidate the best size when the normal
    // bitmap changes as all bitmaps assigned to the button should be of the
    // same size anyhow
    if ( which == State_Normal )
        InvalidateBestSize();

    Refresh();
}

wxSize wxButton::DoGetBitmapMargins() const
{
    return m_imageData ? m_imageData->GetBitmapMargins() : wxSize(0, 0);
}

void wxButton::DoSetBitmapMargins(wxCoord x, wxCoord y)
{
    wxCHECK_RET( m_imageData, "SetBitmap() must be called first" );

    m_imageData->SetBitmapMargins(x, y);
    InvalidateBestSize();
}

void wxButton::DoSetBitmapPosition(wxDirection dir)
{
    wxCHECK_RET( m_imageData, "SetBitmap() must be called first" );

    m_imageData->SetBitmapPosition(dir);
    InvalidateBestSize();
}

// ----------------------------------------------------------------------------
// markup support
// ----------------------------------------------------------------------------

#if wxUSE_MARKUP

bool wxButton::DoSetLabelMarkup(const wxString& markup)
{
    if ( !wxButtonBase::DoSetLabelMarkup(markup) )
        return false;

    if ( !m_markupText )
    {
        m_markupText = new wxMarkupText(markup);
        MakeOwnerDrawn();
    }
    else
    {
        // We are already owner-drawn so just update the text.
        m_markupText->SetMarkup(markup);
    }

    Refresh();

    return true;
}

#endif // wxUSE_MARKUP

// ----------------------------------------------------------------------------
// owner-drawn buttons support
// ----------------------------------------------------------------------------

// drawing helpers
namespace
{

// return the button state using both the ODS_XXX flags specified in state
// parameter and the current button state
wxButton::State GetButtonState(wxButton *btn, UINT state)
{
    if ( state & ODS_DISABLED )
        return wxButton::State_Disabled;

    if ( state & ODS_SELECTED )
        return wxButton::State_Pressed;

    if ( btn->HasCapture() || btn->IsMouseInWindow() )
        return wxButton::State_Current;

    if ( state & ODS_FOCUS )
        return wxButton::State_Focused;

    return wxButton::State_Normal;
}

void DrawButtonText(HDC hdc,
                    RECT *pRect,
                    wxButton *btn,
                    int flags)
{
    const wxString text = btn->GetLabel();

    if ( text.find(wxT('\n')) != wxString::npos )
    {
        // draw multiline label

        // center text horizontally in any case
        flags |= DT_CENTER;

        // first we need to compute its bounding rect
        RECT rc;
        ::CopyRect(&rc, pRect);
        ::DrawText(hdc, text.wx_str(), text.length(), &rc,
                   DT_CENTER | DT_CALCRECT);

        // now center this rect inside the entire button area
        const LONG w = rc.right - rc.left;
        const LONG h = rc.bottom - rc.top;
        rc.left = (pRect->right - pRect->left)/2 - w/2;
        rc.right = rc.left+w;
        rc.top = (pRect->bottom - pRect->top)/2 - h/2;
        rc.bottom = rc.top+h;

        ::DrawText(hdc, text.wx_str(), text.length(), &rc, flags);
    }
    else // single line label
    {
        // translate wx button flags to alignment flags for DrawText()
        if ( btn->HasFlag(wxBU_RIGHT) )
        {
            flags |= DT_RIGHT;
        }
        else if ( !btn->HasFlag(wxBU_LEFT) )
        {
            flags |= DT_CENTER;
        }
        //else: DT_LEFT is the default anyhow (and its value is 0 too)

        if ( btn->HasFlag(wxBU_BOTTOM) )
        {
            flags |= DT_BOTTOM;
        }
        else if ( !btn->HasFlag(wxBU_TOP) )
        {
            flags |= DT_VCENTER;
        }
        //else: as above, DT_TOP is the default

        // notice that we must have DT_SINGLELINE for vertical alignment flags
        // to work
        ::DrawText(hdc, text.wx_str(), text.length(), pRect,
                   flags | DT_SINGLELINE );
    }
}

void DrawRect(HDC hdc, const RECT& r)
{
    wxDrawLine(hdc, r.left, r.top, r.right, r.top);
    wxDrawLine(hdc, r.right, r.top, r.right, r.bottom);
    wxDrawLine(hdc, r.right, r.bottom, r.left, r.bottom);
    wxDrawLine(hdc, r.left, r.bottom, r.left, r.top);
}

/*
   The button frame looks like this normally:

   WWWWWWWWWWWWWWWWWWB
   WHHHHHHHHHHHHHHHHGB  W = white       (HILIGHT)
   WH               GB  H = light grey  (LIGHT)
   WH               GB  G = dark grey   (SHADOW)
   WH               GB  B = black       (DKSHADOW)
   WH               GB
   WGGGGGGGGGGGGGGGGGB
   BBBBBBBBBBBBBBBBBBB

   When the button is selected, the button becomes like this (the total button
   size doesn't change):

   BBBBBBBBBBBBBBBBBBB
   BWWWWWWWWWWWWWWWWBB
   BWHHHHHHHHHHHHHHGBB
   BWH             GBB
   BWH             GBB
   BWGGGGGGGGGGGGGGGBB
   BBBBBBBBBBBBBBBBBBB
   BBBBBBBBBBBBBBBBBBB

   When the button is pushed (while selected) it is like:

   BBBBBBBBBBBBBBBBBBB
   BGGGGGGGGGGGGGGGGGB
   BG               GB
   BG               GB
   BG               GB
   BG               GB
   BGGGGGGGGGGGGGGGGGB
   BBBBBBBBBBBBBBBBBBB
*/
void DrawButtonFrame(HDC hdc, RECT& rectBtn,
                     bool selected, bool pushed)
{
    RECT r;
    CopyRect(&r, &rectBtn);

    AutoHPEN hpenBlack(GetSysColor(COLOR_3DDKSHADOW)),
             hpenGrey(GetSysColor(COLOR_3DSHADOW)),
             hpenLightGr(GetSysColor(COLOR_3DLIGHT)),
             hpenWhite(GetSysColor(COLOR_3DHILIGHT));

    SelectInHDC selectPen(hdc, hpenBlack);

    r.right--;
    r.bottom--;

    if ( pushed )
    {
        DrawRect(hdc, r);

        (void)SelectObject(hdc, hpenGrey);
        ::InflateRect(&r, -1, -1);

        DrawRect(hdc, r);
    }
    else // !pushed
    {
        if ( selected )
        {
            DrawRect(hdc, r);

            ::InflateRect(&r, -1, -1);
        }

        wxDrawLine(hdc, r.left, r.bottom, r.right, r.bottom);
        wxDrawLine(hdc, r.right, r.bottom, r.right, r.top - 1);

        (void)SelectObject(hdc, hpenWhite);
        wxDrawLine(hdc, r.left, r.bottom - 1, r.left, r.top);
        wxDrawLine(hdc, r.left, r.top, r.right, r.top);

        (void)SelectObject(hdc, hpenLightGr);
        wxDrawLine(hdc, r.left + 1, r.bottom - 2, r.left + 1, r.top + 1);
        wxDrawLine(hdc, r.left + 1, r.top + 1, r.right - 1, r.top + 1);

        (void)SelectObject(hdc, hpenGrey);
        wxDrawLine(hdc, r.left + 1, r.bottom - 1, r.right - 1, r.bottom - 1);
        wxDrawLine(hdc, r.right - 1, r.bottom - 1, r.right - 1, r.top);
    }

    InflateRect(&rectBtn, -OD_BUTTON_MARGIN, -OD_BUTTON_MARGIN);
}

#if wxUSE_UXTHEME
void DrawXPBackground(wxButton *button, HDC hdc, RECT& rectBtn, UINT state)
{
    wxUxThemeHandle theme(button, L"BUTTON");

    // this array is indexed by wxButton::State values and so must be kept in
    // sync with it
    static const int uxStates[] =
    {
        PBS_NORMAL, PBS_HOT, PBS_PRESSED, PBS_DISABLED, PBS_DEFAULTED
    };

    int iState = uxStates[GetButtonState(button, state)];

    wxUxThemeEngine * const engine = wxUxThemeEngine::Get();

    // draw parent background if needed
    if ( engine->IsThemeBackgroundPartiallyTransparent
                 (
                    theme,
                    BP_PUSHBUTTON,
                    iState
                 ) )
    {
        // Set this button as the one whose background is being erased: this
        // allows our WM_ERASEBKGND handler used by DrawThemeParentBackground()
        // to correctly align the background brush with this window instead of
        // the parent window to which WM_ERASEBKGND is sent. Notice that this
        // doesn't work with custom user-defined EVT_ERASE_BACKGROUND handlers
        // as they won't be aligned but unfortunately all the attempts to fix
        // it by shifting DC origin before calling DrawThemeParentBackground()
        // failed to work so we at least do this, even though this is far from
        // being the perfect solution.
        wxWindowBeingErased = button;

        engine->DrawThemeParentBackground(GetHwndOf(button), hdc, &rectBtn);

        wxWindowBeingErased = NULL;
    }

    // draw background
    engine->DrawThemeBackground(theme, hdc, BP_PUSHBUTTON, iState,
                                &rectBtn, NULL);

    // calculate content area margins
    MARGINS margins;
    engine->GetThemeMargins(theme, hdc, BP_PUSHBUTTON, iState,
                            TMT_CONTENTMARGINS, &rectBtn, &margins);
    ::InflateRect(&rectBtn, -margins.cxLeftWidth, -margins.cyTopHeight);
    ::InflateRect(&rectBtn, -XP_BUTTON_EXTRA_MARGIN, -XP_BUTTON_EXTRA_MARGIN);

    if ( button->UseBgCol() )
    {
        COLORREF colBg = wxColourToRGB(button->GetBackgroundColour());
        AutoHBRUSH hbrushBackground(colBg);

        // don't overwrite the focus rect
        RECT rectClient;
        ::CopyRect(&rectClient, &rectBtn);
        ::InflateRect(&rectClient, -1, -1);
        FillRect(hdc, &rectClient, hbrushBackground);
    }
}
#endif // wxUSE_UXTHEME

} // anonymous namespace

// ----------------------------------------------------------------------------
// owner drawn buttons support
// ----------------------------------------------------------------------------

void wxButton::MakeOwnerDrawn()
{
    long style = GetWindowLong(GetHwnd(), GWL_STYLE);
    if ( (style & BS_OWNERDRAW) != BS_OWNERDRAW )
    {
        // make it so
        style |= BS_OWNERDRAW;
        SetWindowLong(GetHwnd(), GWL_STYLE, style);
    }
}

bool wxButton::SetBackgroundColour(const wxColour &colour)
{
    if ( !wxControl::SetBackgroundColour(colour) )
    {
        // nothing to do
        return false;
    }

    MakeOwnerDrawn();

    Refresh();

    return true;
}

bool wxButton::SetForegroundColour(const wxColour &colour)
{
    if ( !wxControl::SetForegroundColour(colour) )
    {
        // nothing to do
        return false;
    }

    MakeOwnerDrawn();

    Refresh();

    return true;
}

bool wxButton::MSWOnDraw(WXDRAWITEMSTRUCT *wxdis)
{
    LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)wxdis;
    HDC hdc = lpDIS->hDC;

    UINT state = lpDIS->itemState;
    bool pushed = (SendMessage(GetHwnd(), BM_GETSTATE, 0, 0) & BST_PUSHED) != 0;

    RECT rectBtn;
    CopyRect(&rectBtn, &lpDIS->rcItem);

    // draw the button background
    if ( !HasFlag(wxBORDER_NONE) )
    {
#if wxUSE_UXTHEME
        if ( wxUxThemeEngine::GetIfActive() )
        {
            DrawXPBackground(this, hdc, rectBtn, state);
        }
        else
#endif // wxUSE_UXTHEME
        {
            COLORREF colBg = wxColourToRGB(GetBackgroundColour());

            // first, draw the background
            AutoHBRUSH hbrushBackground(colBg);
            FillRect(hdc, &rectBtn, hbrushBackground);

            // draw the border for the current state
            bool selected = (state & ODS_SELECTED) != 0;
            if ( !selected )
            {
                wxTopLevelWindow *
                    tlw = wxDynamicCast(wxGetTopLevelParent(this), wxTopLevelWindow);
                if ( tlw )
                {
                    selected = tlw->GetDefaultItem() == this;
                }
            }

            DrawButtonFrame(hdc, rectBtn, selected, pushed);
        }

        // draw the focus rectangle if we need it
        if ( (state & ODS_FOCUS) && !(state & ODS_NOFOCUSRECT) )
        {
            DrawFocusRect(hdc, &rectBtn);

#if wxUSE_UXTHEME
            if ( !wxUxThemeEngine::GetIfActive() )
#endif // wxUSE_UXTHEME
            {
                if ( pushed )
                {
                    // the label is shifted by 1 pixel to create "pushed" effect
                    OffsetRect(&rectBtn, 1, 1);
                }
            }
        }
    }


    // draw the image, if any
    if ( m_imageData )
    {
        wxBitmap bmp = m_imageData->GetBitmap(GetButtonState(this, state));
        if ( !bmp.IsOk() )
            bmp = m_imageData->GetBitmap(State_Normal);

        const wxSize sizeBmp = bmp.GetSize();
        const wxSize margin = m_imageData->GetBitmapMargins();
        const wxSize sizeBmpWithMargins(sizeBmp + 2*margin);
        wxRect rectButton(wxRectFromRECT(rectBtn));

        // for simplicity, we start with centred rectangle and then move it to
        // the appropriate edge
        wxRect rectBitmap = wxRect(sizeBmp).CentreIn(rectButton);

        // move bitmap only if we have a label, otherwise keep it centered
        if ( ShowsLabel() )
        {
            switch ( m_imageData->GetBitmapPosition() )
            {
                default:
                    wxFAIL_MSG( "invalid direction" );
                    // fall through

                case wxLEFT:
                    rectBitmap.x = rectButton.x + margin.x;
                    rectButton.x += sizeBmpWithMargins.x;
                    rectButton.width -= sizeBmpWithMargins.x;
                    break;

                case wxRIGHT:
                    rectBitmap.x = rectButton.GetRight() - sizeBmp.x - margin.x;
                    rectButton.width -= sizeBmpWithMargins.x;
                    break;

                case wxTOP:
                    rectBitmap.y = rectButton.y + margin.y;
                    rectButton.y += sizeBmpWithMargins.y;
                    rectButton.height -= sizeBmpWithMargins.y;
                    break;

                case wxBOTTOM:
                    rectBitmap.y = rectButton.GetBottom() - sizeBmp.y - margin.y;
                    rectButton.height -= sizeBmpWithMargins.y;
                    break;
            }
        }

        wxDCTemp dst((WXHDC)hdc);
        dst.DrawBitmap(bmp, rectBitmap.GetPosition(), true);

        wxCopyRectToRECT(rectButton, rectBtn);
    }


    // finally draw the label
    if ( ShowsLabel() )
    {
        COLORREF colFg = state & ODS_DISABLED
                            ? ::GetSysColor(COLOR_GRAYTEXT)
                            : wxColourToRGB(GetForegroundColour());

        wxTextColoursChanger changeFg(hdc, colFg, CLR_INVALID);
        wxBkModeChanger changeBkMode(hdc, wxBRUSHSTYLE_TRANSPARENT);

#if wxUSE_MARKUP
        if ( m_markupText )
        {
            wxDCTemp dc((WXHDC)hdc);
            dc.SetTextForeground(wxColour(colFg));
            dc.SetFont(GetFont());

            m_markupText->Render(dc, wxRectFromRECT(rectBtn),
                                 state & ODS_NOACCEL
                                    ? wxMarkupText::Render_Default
                                    : wxMarkupText::Render_ShowAccels);
        }
        else // Plain text label
#endif // wxUSE_MARKUP
        {
            // notice that DT_HIDEPREFIX doesn't work on old (pre-Windows 2000)
            // systems but by happy coincidence ODS_NOACCEL is not used under
            // them neither so DT_HIDEPREFIX should never be used there
            DrawButtonText(hdc, &rectBtn, this,
                           state & ODS_NOACCEL ? DT_HIDEPREFIX : 0);
        }
    }

    return true;
}

#endif // wxUSE_BUTTON

