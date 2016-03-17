/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/fontdlg.cpp
// Purpose:     wxFontDialog class for carbon 10.2+.
// Author:      Ryan Norton
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_FONTDLG

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/crt.h"
    #include "wx/dcclient.h"
    #include "wx/frame.h"
    #include "wx/textctrl.h"
    #include "wx/listbox.h"
    #include "wx/checkbox.h"
    #include "wx/choice.h"
    #include "wx/sizer.h"
    #include "wx/stattext.h"
    #include "wx/button.h"
#endif

#include "wx/fontdlg.h"
#include "wx/fontutil.h"
#include "wx/modalhook.h"

#if wxOSX_USE_EXPERIMENTAL_FONTDIALOG

wxIMPLEMENT_DYNAMIC_CLASS(wxFontDialog, wxDialog);

#include "wx/osx/private.h"

// ---------------------------------------------------------------------------
// wxFontDialog
// ---------------------------------------------------------------------------

wxFontDialog::wxFontDialog()
{
}

wxFontDialog::wxFontDialog(wxWindow *parent)
{
    Create(parent);
}

wxFontDialog::wxFontDialog(wxWindow *parent, const wxFontData&  data)
{
    Create(parent, data);
}

wxFontDialog::~wxFontDialog()
{
}

bool wxFontDialog::Create(wxWindow *WXUNUSED(parent))
{
    return true ;
}

bool wxFontDialog::Create(wxWindow *parent, const wxFontData& data)
{
    m_fontData = data;
    return Create(parent) ;
}

int wxFontDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    wxDialog::OSXBeginModalDialog();
    int retval = RunMixedFontDialog(this);
    wxDialog::OSXEndModalDialog();

    return retval ;
}

#else

#if !USE_NATIVE_FONT_DIALOG_FOR_MACOSX

#undef wxFontDialog

#include "wx/osx/fontdlg.h"

#include "wx/fontenum.h"
#include "wx/colordlg.h"
#include "wx/spinctrl.h"

// ---------------------------------------------------------------------------
// wxFontDialog stub for mac OS's without a native font dialog
// ---------------------------------------------------------------------------

static const wxChar *FontFamilyIntToString(wxFontFamily family);
static wxFontFamily FontFamilyStringToInt(const wxChar *family);


//-----------------------------------------------------------------------------
// helper class - wxFontPreviewCtrl
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxFontPreviewCtrl : public wxWindow
{
public:
    wxFontPreviewCtrl(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& sz = wxDefaultSize, long style = 0) :
        wxWindow(parent, id, pos, sz, style)
    {
        SetBackgroundColour(*wxWHITE);
    }

private:
    void OnPaint(wxPaintEvent& event);
    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(wxFontPreviewCtrl, wxWindow)
    EVT_PAINT(wxFontPreviewCtrl::OnPaint)
wxEND_EVENT_TABLE()

void wxFontPreviewCtrl::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);

    wxSize size = GetSize();
    wxFont font = GetFont();

    if ( font.IsOk() )
    {
        dc.SetFont(font);
        // Calculate vertical centre
        long w = 0, h = 0;
        dc.GetTextExtent( wxT("X"), &w, &h);
        dc.SetTextForeground(GetForegroundColour());
        dc.SetClippingRegion(2, 2, size.x-4, size.y-4);
        dc.DrawText(_("ABCDEFGabcdefg12345"),
                     10, size.y/2 - h/2);
        dc.DestroyClippingRegion();
    }
}

/*
 * A control for displaying a small preview of a colour or bitmap
 */

class wxFontColourSwatchCtrl: public wxControl
{
    wxDECLARE_CLASS(wxFontColourSwatchCtrl);
public:
    wxFontColourSwatchCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0);
    virtual ~wxFontColourSwatchCtrl();

    void OnPaint(wxPaintEvent& event);
    void OnMouseEvent(wxMouseEvent& event);

    void SetColour(const wxColour& colour) { m_colour = colour; SetBackgroundColour(m_colour); }

    wxColour& GetColour() { return m_colour; }

    virtual wxSize DoGetBestSize() const { return GetSize(); }

protected:
    wxColour    m_colour;

    wxDECLARE_EVENT_TABLE();
};

/*
 * A control for displaying a small preview of a colour or bitmap
 */

wxBEGIN_EVENT_TABLE(wxFontColourSwatchCtrl, wxControl)
    EVT_MOUSE_EVENTS(wxFontColourSwatchCtrl::OnMouseEvent)
wxEND_EVENT_TABLE()

wxIMPLEMENT_CLASS(wxFontColourSwatchCtrl, wxControl);

wxFontColourSwatchCtrl::wxFontColourSwatchCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style):
    wxControl(parent, id, pos, size, style)
{
    SetColour(* wxWHITE);
    SetBackgroundStyle(wxBG_STYLE_COLOUR);
}

wxFontColourSwatchCtrl::~wxFontColourSwatchCtrl()
{
}

void wxFontColourSwatchCtrl::OnMouseEvent(wxMouseEvent& event)
{
    if (event.LeftDown())
    {
        wxWindow* parent = GetParent();
        while (parent != NULL && !parent->IsKindOf(CLASSINFO(wxDialog)) && !parent->IsKindOf(CLASSINFO(wxFrame)))
            parent = parent->GetParent();

        wxColourData data;
        data.SetChooseFull(true);
        data.SetColour(m_colour);
        wxColourDialog *dialog = new wxColourDialog(parent, &data);
        // Crashes on wxMac (no m_peer)
#ifndef __WXMAC__
        dialog->SetTitle(_("Background colour"));
#endif
        if (dialog->ShowModal() == wxID_OK)
        {
            wxColourData retData = dialog->GetColourData();
            m_colour = retData.GetColour();
            SetBackgroundColour(m_colour);
        }
        dialog->Destroy();
        Refresh();

        wxCommandEvent event(wxEVT_BUTTON, GetId());
        HandleWindowEvent(event);
    }
}

/*!
 * wxFontDialog type definition
 */

wxIMPLEMENT_DYNAMIC_CLASS(wxFontDialog, wxDialog);

/*!
 * wxFontDialog event table definition
 */

wxBEGIN_EVENT_TABLE( wxFontDialog, wxDialog )
    EVT_LISTBOX( wxID_FONTDIALOG_FACENAME, wxFontDialog::OnFontdialogFacenameSelected )
    EVT_SPINCTRL( wxID_FONTDIALOG_FONTSIZE, wxFontDialog::OnFontdialogFontsizeUpdated )
    EVT_TEXT( wxID_FONTDIALOG_FONTSIZE, wxFontDialog::OnFontdialogFontsizeTextUpdated )
    EVT_CHECKBOX( wxID_FONTDIALOG_BOLD, wxFontDialog::OnFontdialogBoldClick )
    EVT_CHECKBOX( wxID_FONTDIALOG_ITALIC, wxFontDialog::OnFontdialogItalicClick )
    EVT_CHECKBOX( wxID_FONTDIALOG_UNDERLINED, wxFontDialog::OnFontdialogUnderlinedClick )
    EVT_BUTTON( wxID_OK, wxFontDialog::OnOkClick )
    EVT_BUTTON(wxID_FONTDIALOG_COLOUR, wxFontDialog::OnColourChanged)
wxEND_EVENT_TABLE()

/*!
 * wxFontDialog constructors
 */

wxFontDialog::wxFontDialog( )
{
    m_dialogParent = NULL;
}

wxFontDialog::wxFontDialog(wxWindow* parent, const wxFontData& fontData)
{
    m_dialogParent = NULL;

    Create(parent, fontData);
}

wxFontDialog::~wxFontDialog()
{
    // empty
}

/*!
 * wxFontDialog creator
 */

bool wxFontDialog::Create(wxWindow* parent, const wxFontData& fontData)
{
    m_fontData = fontData;
    m_suppressUpdates = false;

    wxString caption = _("Font");

    m_facenameCtrl = NULL;
    m_sizeCtrl = NULL;
    m_boldCtrl = NULL;
    m_italicCtrl = NULL;
    m_underlinedCtrl = NULL;
    m_colourCtrl = NULL;
    m_previewCtrl = NULL;

    InitializeFont();

    SetExtraStyle(GetExtraStyle()|wxWS_EX_BLOCK_EVENTS);
    wxDialog::Create( parent, wxID_ANY, caption, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

    CreateControls();
    GetSizer()->Fit(this);
    GetSizer()->SetSizeHints(this);
    Centre();

    return true;
}

/*!
 * Control creation for wxFontDialog
 */

void wxFontDialog::CreateControls()
{
    wxFontDialog* itemDialog1 = this;

    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
    itemDialog1->SetSizer(itemBoxSizer2);

    wxBoxSizer* itemBoxSizer3 = new wxBoxSizer(wxVERTICAL);
    itemBoxSizer2->Add(itemBoxSizer3, 1, wxGROW|wxALL, 5);

    wxFlexGridSizer* itemFlexGridSizer4 = new wxFlexGridSizer(6, 2, 10, 0);
    itemFlexGridSizer4->AddGrowableRow(4);
    itemFlexGridSizer4->AddGrowableCol(1);
    itemBoxSizer3->Add(itemFlexGridSizer4, 1, wxGROW|wxALL, 5);

    wxStaticText* itemStaticText5 = new wxStaticText( itemDialog1, wxID_STATIC, _("Font:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer4->Add(itemStaticText5, 0, wxALIGN_RIGHT|wxALIGN_TOP|wxALL, 5);

    wxBoxSizer* itemBoxSizer6 = new wxBoxSizer(wxVERTICAL);
    itemFlexGridSizer4->Add(itemBoxSizer6, 0, wxGROW, 5);

    wxString* m_facenameCtrlStrings = NULL;
    m_facenameCtrl = new wxListBox( itemDialog1, wxID_FONTDIALOG_FACENAME, wxDefaultPosition, wxSize(320, 100), 0, m_facenameCtrlStrings, wxLB_SINGLE );
    itemBoxSizer6->Add(m_facenameCtrl, 0, wxGROW|wxALL, 5);

    wxStaticText* itemStaticText8 = new wxStaticText( itemDialog1, wxID_STATIC, _("Size:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer4->Add(itemStaticText8, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    m_sizeCtrl = new wxSpinCtrl( itemDialog1, wxID_FONTDIALOG_FONTSIZE, wxT("12"), wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 1, 300, 12 );
    m_sizeCtrl->SetHelpText(_("The font size in points."));
    if (ShowToolTips())
        m_sizeCtrl->SetToolTip(_("The font size in points."));
    itemFlexGridSizer4->Add(m_sizeCtrl, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText10 = new wxStaticText( itemDialog1, wxID_STATIC, _("Style:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer4->Add(itemStaticText10, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer11 = new wxBoxSizer(wxHORIZONTAL);
    itemFlexGridSizer4->Add(itemBoxSizer11, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);

    m_boldCtrl = new wxCheckBox( itemDialog1, wxID_FONTDIALOG_BOLD, _("Bold"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
    m_boldCtrl->SetValue(false);
    m_boldCtrl->SetHelpText(_("Check to make the font bold."));
    if (ShowToolTips())
        m_boldCtrl->SetToolTip(_("Check to make the font bold."));
    itemBoxSizer11->Add(m_boldCtrl, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    m_italicCtrl = new wxCheckBox( itemDialog1, wxID_FONTDIALOG_ITALIC, _("Italic"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
    m_italicCtrl->SetValue(false);
    m_italicCtrl->SetHelpText(_("Check to make the font italic."));
    if (ShowToolTips())
        m_italicCtrl->SetToolTip(_("Check to make the font italic."));
    itemBoxSizer11->Add(m_italicCtrl, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    if (m_fontData.GetEnableEffects())
    {
        m_underlinedCtrl = new wxCheckBox( itemDialog1, wxID_FONTDIALOG_UNDERLINED, _("Underlined"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
        m_underlinedCtrl->SetValue(false);
        m_underlinedCtrl->SetHelpText(_("Check to make the font underlined."));
        if (ShowToolTips())
            m_underlinedCtrl->SetToolTip(_("Check to make the font underlined."));
        itemBoxSizer11->Add(m_underlinedCtrl, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    }

    if (m_fontData.GetEnableEffects())
    {
        wxStaticText* itemStaticText15 = new wxStaticText( itemDialog1, wxID_STATIC, _("Colour:"), wxDefaultPosition, wxDefaultSize, 0 );
        itemFlexGridSizer4->Add(itemStaticText15, 0, wxALIGN_RIGHT|wxALIGN_TOP|wxALL, 5);

        m_colourCtrl = new wxFontColourSwatchCtrl( itemDialog1, wxID_FONTDIALOG_COLOUR, wxDefaultPosition, wxSize(-1, 30), wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
        m_colourCtrl->SetHelpText(_("Click to change the font colour."));
        if (ShowToolTips())
            m_colourCtrl->SetToolTip(_("Click to change the font colour."));
        itemFlexGridSizer4->Add(m_colourCtrl, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    }

    wxStaticText* itemStaticText17 = new wxStaticText( itemDialog1, wxID_STATIC, _("Preview:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer4->Add(itemStaticText17, 0, wxALIGN_RIGHT|wxALIGN_TOP|wxALL, 5);

    m_previewCtrl = new wxFontPreviewCtrl( itemDialog1, wxID_FONTDIALOG_PREVIEW, wxDefaultPosition, wxSize(-1, 70), wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
    m_previewCtrl->SetHelpText(_("Shows a preview of the font."));
    if (ShowToolTips())
        m_previewCtrl->SetToolTip(_("Shows a preview of the font."));
    itemFlexGridSizer4->Add(m_previewCtrl, 1, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer19 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer3->Add(itemBoxSizer19, 0, wxALIGN_RIGHT|wxALL, 5);

    wxButton* itemButton20 = new wxButton( itemDialog1, wxID_CANCEL, _("&Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    itemButton20->SetHelpText(_("Click to cancel changes to the font."));
    if (ShowToolTips())
        itemButton20->SetToolTip(_("Click to cancel changes to the font."));
    itemBoxSizer19->Add(itemButton20, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxButton* itemButton21 = new wxButton( itemDialog1, wxID_OK, _("&OK"), wxDefaultPosition, wxDefaultSize, 0 );
    itemButton21->SetDefault();
    itemButton21->SetHelpText(_("Click to confirm changes to the font."));
    if (ShowToolTips())
        itemButton21->SetToolTip(_("Click to confirm changes to the font."));
    itemBoxSizer19->Add(itemButton21, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxFontEnumerator enumerator;
    enumerator.EnumerateFacenames();
    wxArrayString facenames = enumerator.GetFacenames();
    if (!facenames.empty())
    {
        facenames.Add(_("<Any>"));
        facenames.Add(_("<Any Roman>"));
        facenames.Add(_("<Any Decorative>"));
        facenames.Add(_("<Any Modern>"));
        facenames.Add(_("<Any Script>"));
        facenames.Add(_("<Any Swiss>"));
        facenames.Add(_("<Any Teletype>"));
        facenames.Sort();
        m_facenameCtrl->Append(facenames);
    }

    InitializeControls();
    m_previewCtrl->SetFont(m_dialogFont);
    if (m_fontData.GetColour().IsOk())
    {
        m_previewCtrl->SetForegroundColour(m_fontData.GetColour());
    }
    m_previewCtrl->Refresh();
}

/*!
 * wxEVT_SPINCTRL event handler for wxID_FONTDIALOG_FONTSIZE
 */

void wxFontDialog::OnFontdialogFontsizeUpdated( wxSpinEvent& WXUNUSED(event) )
{
    ChangeFont();
}

/*!
 * wxEVT_TEXT event handler for wxID_FONTDIALOG_FONTSIZE
 */

void wxFontDialog::OnFontdialogFontsizeTextUpdated( wxCommandEvent& WXUNUSED(event) )
{
    ChangeFont();
}

/*!
 * wxEVT_CHECKBOX event handler for wxID_FONTDIALOG_BOLD
 */

void wxFontDialog::OnFontdialogBoldClick( wxCommandEvent& WXUNUSED(event) )
{
    ChangeFont();
}

/*!
 * wxEVT_CHECKBOX event handler for wxID_FONTDIALOG_ITALIC
 */

void wxFontDialog::OnFontdialogItalicClick( wxCommandEvent& WXUNUSED(event) )
{
    ChangeFont();
}

/*!
 * wxEVT_CHECKBOX event handler for wxID_FONTDIALOG_UNDERLINED
 */

void wxFontDialog::OnFontdialogUnderlinedClick( wxCommandEvent& WXUNUSED(event) )
{
    ChangeFont();
}

/*!
 * wxEVT_BUTTON event handler for wxID_OK
 */

void wxFontDialog::OnOkClick( wxCommandEvent& event )
{
    event.Skip();
}


/*!
 * wxEVT_LISTBOX event handler for wxID_FONTDIALOG_FACENAME
 */

void wxFontDialog::OnFontdialogFacenameSelected( wxCommandEvent& WXUNUSED(event) )
{
    ChangeFont();
}

void wxFontDialog::OnColourChanged(wxCommandEvent & WXUNUSED(event))
{
    m_fontData.SetColour(m_colourCtrl->GetColour());
    m_previewCtrl->SetForegroundColour(m_colourCtrl->GetColour());
    m_previewCtrl->Refresh();
}

/*!
 * Should we show tooltips?
 */

bool wxFontDialog::ShowToolTips()
{
    return true;
}

void wxFontDialog::InitializeFont()
{
    wxFontFamily fontFamily = wxFONTFAMILY_SWISS;
    wxFontWeight fontWeight = wxFONTWEIGHT_NORMAL;
    wxFontStyle fontStyle = wxFONTSTYLE_NORMAL;
    int fontSize = 12;
    bool fontUnderline = false;
    wxString fontName;

    if (m_fontData.m_initialFont.IsOk())
    {
        fontFamily = m_fontData.m_initialFont.GetFamily();
        fontWeight = m_fontData.m_initialFont.GetWeight();
        fontStyle = m_fontData.m_initialFont.GetStyle();
        fontSize = m_fontData.m_initialFont.GetPointSize();
        fontUnderline = m_fontData.m_initialFont.GetUnderlined();
        fontName = m_fontData.m_initialFont.GetFaceName();
    }

    m_dialogFont = wxFont(fontSize, fontFamily, fontStyle,
                          fontWeight, fontUnderline, fontName);

    if (m_previewCtrl)
        m_previewCtrl->SetFont(m_dialogFont);

    m_fontData.SetChosenFont(m_dialogFont);
}

/// Set controls according to current font
void wxFontDialog::InitializeControls()
{
    m_suppressUpdates = true;

    if (m_underlinedCtrl)
        m_underlinedCtrl->SetValue(m_dialogFont.GetUnderlined());

    m_boldCtrl->SetValue(m_dialogFont.GetWeight() == wxFONTWEIGHT_BOLD);
    m_italicCtrl->SetValue(m_dialogFont.GetStyle() == wxFONTSTYLE_ITALIC);
    m_sizeCtrl->SetValue(m_dialogFont.GetPointSize());

    wxString facename = m_dialogFont.GetFaceName();
    if (facename.empty() || m_facenameCtrl->FindString(facename) == wxNOT_FOUND)
    {
        facename = FontFamilyIntToString(m_dialogFont.GetFamily());
    }
    m_facenameCtrl->SetStringSelection(facename);

    if (m_colourCtrl && m_fontData.GetColour().IsOk())
    {
        m_colourCtrl->SetColour(m_fontData.GetColour());
        m_colourCtrl->Refresh();
    }

    m_suppressUpdates = false;
}

/// Respond to font change
void wxFontDialog::ChangeFont()
{
    if (m_suppressUpdates)
        return;

    bool underlined = m_underlinedCtrl ? m_underlinedCtrl->GetValue() : false;
    bool italic = m_italicCtrl->GetValue();
    bool bold = m_boldCtrl->GetValue();
    int size = m_sizeCtrl->GetValue();
    wxString facename = m_facenameCtrl->GetStringSelection();

    wxFontFamily family = FontFamilyStringToInt(facename);
    if (family != wxFONTFAMILY_DEFAULT)
        facename = wxEmptyString;

    m_dialogFont = wxFontInfo(size)
                    .Family(family).FaceName(facename)
                    .Italic(italic).Bold(bold).Underlined(underlined);

    m_fontData.SetChosenFont(m_dialogFont);

    m_previewCtrl->SetFont(m_dialogFont);
    m_previewCtrl->Refresh();
}

void wxFontDialog::SetData(const wxFontData& fontdata)
{
    m_fontData = fontdata;
}

bool wxFontDialog::IsShown() const
{
    return false;
}

int wxFontDialog::ShowModal()
{
    return wxDialog::ShowModal();
}

void wxFontDialog::OnPanelClose()
{
}

const wxChar *FontFamilyIntToString(wxFontFamily family)
{
    switch (family)
    {
        case wxFONTFAMILY_ROMAN:
            return _("<Any Roman>");
        case wxFONTFAMILY_DECORATIVE:
            return _("<Any Decorative>");
        case wxFONTFAMILY_MODERN:
            return _("<Any Modern>");
        case wxFONTFAMILY_SCRIPT:
            return _("<Any Script>");
        case wxFONTFAMILY_TELETYPE:
            return _("<Any Teletype>");
        case wxFONTFAMILY_SWISS:
        default:
            return _("<Any Swiss>");
    }
}

wxFontFamily FontFamilyStringToInt(const wxChar *family)
{
    if (!family)
        return wxFONTFAMILY_SWISS;

    if (wxStrcmp(family, _("<Any Roman>")) == 0)
        return wxFONTFAMILY_ROMAN;
    else if (wxStrcmp(family, _("<Any Decorative>")) == 0)
        return wxFONTFAMILY_DECORATIVE;
    else if (wxStrcmp(family, _("<Any Modern>")) == 0)
        return wxFONTFAMILY_MODERN;
    else if (wxStrcmp(family, _("<Any Script>")) == 0)
        return wxFONTFAMILY_SCRIPT;
    else if (wxStrcmp(family, _("<Any Teletype>")) == 0)
        return wxFONTFAMILY_TELETYPE;
    else if (wxStrcmp(family, _("<Any Swiss>")) == 0)
        return wxFONTFAMILY_SWISS;
    else return wxFONTFAMILY_DEFAULT;
}

#endif // !USE_NATIVE_FONT_DIALOG_FOR_MACOSX

#endif // wxOSX_USE_EXPERIMENTAL_FONTDIALOG

#endif // wxUSE_FONTDLG
