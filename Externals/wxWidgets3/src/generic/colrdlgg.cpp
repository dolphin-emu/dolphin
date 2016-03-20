/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/colrdlgg.cpp
// Purpose:     Choice dialogs
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_COLOURDLG

#ifndef WX_PRECOMP
    #include "wx/settings.h"
    #include "wx/utils.h"
    #include "wx/intl.h"
    #include "wx/dialog.h"
    #include "wx/listbox.h"
    #include "wx/button.h"
    #include "wx/stattext.h"
    #include "wx/layout.h"
    #include "wx/dcclient.h"
    #include "wx/sizer.h"
    #include "wx/slider.h"
#endif

#if wxUSE_STATLINE
    #include "wx/statline.h"
#endif

#include "wx/colourdata.h"
#include "wx/generic/colrdlgg.h"

#if wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
#include "wx/statbmp.h"
#include "wx/dcmemory.h"
#include "wx/dcgraph.h"
#endif // wxCLRDLGG_USE_PREVIEW_WITH_ALPHA

#define wxID_ADD_CUSTOM     3000
#if wxUSE_SLIDER
    #define wxID_RED_SLIDER     3001
    #define wxID_GREEN_SLIDER   3002
    #define wxID_BLUE_SLIDER    3003
#endif // wxUSE_SLIDER

wxIMPLEMENT_DYNAMIC_CLASS(wxGenericColourDialog, wxDialog);

wxBEGIN_EVENT_TABLE(wxGenericColourDialog, wxDialog)
    EVT_BUTTON(wxID_ADD_CUSTOM, wxGenericColourDialog::OnAddCustom)
#if wxUSE_SLIDER
    EVT_SLIDER(wxID_RED_SLIDER, wxGenericColourDialog::OnRedSlider)
    EVT_SLIDER(wxID_GREEN_SLIDER, wxGenericColourDialog::OnGreenSlider)
    EVT_SLIDER(wxID_BLUE_SLIDER, wxGenericColourDialog::OnBlueSlider)
#endif
    EVT_PAINT(wxGenericColourDialog::OnPaint)
    EVT_MOUSE_EVENTS(wxGenericColourDialog::OnMouseEvent)
    EVT_CLOSE(wxGenericColourDialog::OnCloseWindow)
wxEND_EVENT_TABLE()


/*
 * Generic wxColourDialog
 */

// don't change the number of elements (48) in this array, the code below is
// hardcoded to use it
static const wxChar *const wxColourDialogNames[] =
{
    wxT("ORANGE"),
    wxT("GOLDENROD"),
    wxT("WHEAT"),
    wxT("SPRING GREEN"),
    wxT("SKY BLUE"),
    wxT("SLATE BLUE"),
    wxT("MEDIUM VIOLET RED"),
    wxT("PURPLE"),

    wxT("RED"),
    wxT("YELLOW"),
    wxT("MEDIUM SPRING GREEN"),
    wxT("PALE GREEN"),
    wxT("CYAN"),
    wxT("LIGHT STEEL BLUE"),
    wxT("ORCHID"),
    wxT("LIGHT MAGENTA"),

    wxT("BROWN"),
    wxT("YELLOW"),
    wxT("GREEN"),
    wxT("CADET BLUE"),
    wxT("MEDIUM BLUE"),
    wxT("MAGENTA"),
    wxT("MAROON"),
    wxT("ORANGE RED"),

    wxT("FIREBRICK"),
    wxT("CORAL"),
    wxT("FOREST GREEN"),
    wxT("AQUAMARINE"),
    wxT("BLUE"),
    wxT("NAVY"),
    wxT("THISTLE"),
    wxT("MEDIUM VIOLET RED"),

    wxT("INDIAN RED"),
    wxT("GOLD"),
    wxT("MEDIUM SEA GREEN"),
    wxT("MEDIUM BLUE"),
    wxT("MIDNIGHT BLUE"),
    wxT("GREY"),
    wxT("PURPLE"),
    wxT("KHAKI"),

    wxT("BLACK"),
    wxT("MEDIUM FOREST GREEN"),
    wxT("KHAKI"),
    wxT("DARK GREY"),
    wxT("SEA GREEN"),
    wxT("LIGHT GREY"),
    wxT("MEDIUM SLATE BLUE"),
    wxT("WHITE")
};

wxGenericColourDialog::wxGenericColourDialog()
{
    m_whichKind = 1;
    m_colourSelection = -1;
}

wxGenericColourDialog::wxGenericColourDialog(wxWindow *parent,
                                             wxColourData *data)
{
    m_whichKind = 1;
    m_colourSelection = -1;
    Create(parent, data);
}

wxGenericColourDialog::~wxGenericColourDialog()
{
}

void wxGenericColourDialog::OnCloseWindow(wxCloseEvent& WXUNUSED(event))
{
    EndModal(wxID_CANCEL);
}

bool wxGenericColourDialog::Create(wxWindow *parent, wxColourData *data)
{
    if ( !wxDialog::Create(GetParentForModalDialog(parent, 0), wxID_ANY,
                           _("Choose colour"),
                           wxPoint(0, 0), wxSize(900, 900)) )
        return false;

    if (data)
        m_colourData = *data;

    InitializeColours();
    CalculateMeasurements();
    CreateWidgets();

    return true;
}

int wxGenericColourDialog::ShowModal()
{
    return wxDialog::ShowModal();
}


// Internal functions
void wxGenericColourDialog::OnMouseEvent(wxMouseEvent& event)
{
  if (event.ButtonDown(wxMOUSE_BTN_LEFT))
  {
    int x = (int)event.GetX();
    int y = (int)event.GetY();

    if ((x >= m_standardColoursRect.x && x <= (m_standardColoursRect.x + m_standardColoursRect.width)) &&
        (y >= m_standardColoursRect.y && y <= (m_standardColoursRect.y + m_standardColoursRect.height)))
    {
      int selX = (int)(x - m_standardColoursRect.x)/(m_smallRectangleSize.x + m_gridSpacing);
      int selY = (int)(y - m_standardColoursRect.y)/(m_smallRectangleSize.y + m_gridSpacing);
      int ptr = (int)(selX + selY*8);
      OnBasicColourClick(ptr);
    }
    // wxStaticBitmap (used to ARGB preview) events are handled in the dedicated handler.
#if !wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
    else if ((x >= m_customColoursRect.x && x <= (m_customColoursRect.x + m_customColoursRect.width)) &&
        (y >= m_customColoursRect.y && y <= (m_customColoursRect.y + m_customColoursRect.height)))
    {
      int selX = (int)(x - m_customColoursRect.x)/(m_smallRectangleSize.x + m_gridSpacing);
      int selY = (int)(y - m_customColoursRect.y)/(m_smallRectangleSize.y + m_gridSpacing);
      int ptr = (int)(selX + selY*8);
      OnCustomColourClick(ptr);
    }
#endif
    else
        event.Skip();
  }
  else
      event.Skip();
}

#if wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
void wxGenericColourDialog::OnCustomColourMouseClick(wxMouseEvent& event)
{
    // Find index of custom colour
    // and call the handler.
    for (unsigned i = 0; i < WXSIZEOF(m_customColoursBmp); i++)
    {
        if ( m_customColoursBmp[i]->GetId() == event.GetId() )
        {
              OnCustomColourClick(i);
              return;
        }
    }

    event.Skip();
}
#endif // wxCLRDLGG_USE_PREVIEW_WITH_ALPHA

void wxGenericColourDialog::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);

    PaintBasicColours(dc);
    // wxStaticBitmap controls are updated on their own.
#if !wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
    PaintCustomColours(dc, -1);
    PaintCustomColour(dc);
#endif // !wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
    PaintHighlight(dc, true);
}

void wxGenericColourDialog::CalculateMeasurements()
{
    // For single customizable colour
    const wxSize customRectangleSize(40, 40);

    m_smallRectangleSize.Set(18, 14);

    m_gridSpacing = 6;
    m_sectionSpacing = 15;

    m_standardColoursRect.x = 10;
    m_standardColoursRect.y = 15;
    m_standardColoursRect.width = (8*m_smallRectangleSize.x) + (7*m_gridSpacing);
    m_standardColoursRect.height = (6*m_smallRectangleSize.y) + (5*m_gridSpacing);

    m_customColoursRect.x = m_standardColoursRect.x;
    m_customColoursRect.y = m_standardColoursRect.y + m_standardColoursRect.height  + 20;
    m_customColoursRect.width = (8*m_smallRectangleSize.x) + (7*m_gridSpacing);
    m_customColoursRect.height = (2*m_smallRectangleSize.y) + (1*m_gridSpacing);

    m_singleCustomColourRect.x = m_customColoursRect.width + m_customColoursRect.x + m_sectionSpacing;
    m_singleCustomColourRect.y = 80;
    m_singleCustomColourRect.SetSize(customRectangleSize);

    m_okButtonX = 10;
    m_customButtonX = m_singleCustomColourRect.x ;
    m_buttonY = m_customColoursRect.y + m_customColoursRect.height + 10;
}

void wxGenericColourDialog::CreateWidgets()
{
    wxBeginBusyCursor();

#if wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
    // Bitmap to preview selected colour (with alpha channel)
    wxBitmap customColourBmp(m_singleCustomColourRect.GetSize(), 32);
    customColourBmp.UseAlpha();
    DoPreviewBitmap(customColourBmp, m_colourData.GetColour());
    m_customColourBmp = new wxStaticBitmap(this, wxID_ANY, customColourBmp,
                                           m_singleCustomColourRect.GetLeftTop(),
                                           m_singleCustomColourRect.GetSize(),
                                           wxBORDER_SUNKEN);

    // 16 bitmaps to preview custom colours (with alpha channel)
    for (unsigned i = 0; i < WXSIZEOF(m_customColoursBmp); i++)
    {
        int x = ((i % 8)*(m_smallRectangleSize.x+m_gridSpacing)) + m_customColoursRect.x;
        int y = ((i / 8)*(m_smallRectangleSize.y+m_gridSpacing)) + m_customColoursRect.y;

        wxBitmap bmp(m_smallRectangleSize, 32);
        bmp.UseAlpha();
        DoPreviewBitmap(bmp, m_customColours[i]);
        m_customColoursBmp[i] = new wxStaticBitmap(this, wxID_ANY, bmp,
                                                   wxPoint(x, y), m_smallRectangleSize);
        m_customColoursBmp[i]->Bind(wxEVT_LEFT_DOWN,
                                    &wxGenericColourDialog::OnCustomColourMouseClick, this);

    }
#endif // wxCLRDLGG_USE_PREVIEW_WITH_ALPHA

    wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );

    const int sliderHeight = 160;

    // first sliders
#if wxUSE_SLIDER
    const int sliderX = m_singleCustomColourRect.x + m_singleCustomColourRect.width + m_sectionSpacing;

    wxColour c = m_colourData.GetColour();
    m_redSlider = new wxSlider(this, wxID_RED_SLIDER, c.Red(), 0, 255,
        wxDefaultPosition, wxSize(wxDefaultCoord, sliderHeight), wxSL_VERTICAL|wxSL_LABELS|wxSL_INVERSE);
    m_greenSlider = new wxSlider(this, wxID_GREEN_SLIDER, c.Green(), 0, 255,
        wxDefaultPosition, wxSize(wxDefaultCoord, sliderHeight), wxSL_VERTICAL|wxSL_LABELS|wxSL_INVERSE);
    m_blueSlider = new wxSlider(this, wxID_BLUE_SLIDER, c.Blue(), 0, 255,
        wxDefaultPosition, wxSize(wxDefaultCoord, sliderHeight), wxSL_VERTICAL|wxSL_LABELS|wxSL_INVERSE);
    if ( m_colourData.GetChooseAlpha() )
    {
        m_alphaSlider = new wxSlider(this, wxID_ANY, c.Alpha(), 0, 255,
            wxDefaultPosition, wxSize(wxDefaultCoord, sliderHeight), wxSL_VERTICAL|wxSL_LABELS|wxSL_INVERSE);
        m_alphaSlider->Bind(wxEVT_SLIDER, &wxGenericColourDialog::OnAlphaSlider, this);
    }
    else
    {
        m_alphaSlider = NULL;
    }

    wxBoxSizer *sliderSizer = new wxBoxSizer( wxHORIZONTAL );

    sliderSizer->Add(sliderX, sliderHeight );

    const wxSizerFlags sliderLabelFlags = wxSizerFlags().Right().Border();
    const wxSizerFlags onesliderFlags = wxSizerFlags().CenterHorizontal();
    const wxSizerFlags sliderFlags
        = wxSizerFlags().CentreVertical().DoubleBorder();

    wxBoxSizer *redSliderSizer = new wxBoxSizer(wxVERTICAL);
    redSliderSizer->Add(new wxStaticText(this, wxID_ANY, _("Red:")), sliderLabelFlags);
    redSliderSizer->Add(m_redSlider, onesliderFlags);
    wxBoxSizer *greenSliderSizer = new wxBoxSizer(wxVERTICAL);
    greenSliderSizer->Add(new wxStaticText(this, wxID_ANY, _("Green:")), sliderLabelFlags);
    greenSliderSizer->Add(m_greenSlider, onesliderFlags);
    wxBoxSizer *blueSliderSizer = new wxBoxSizer(wxVERTICAL);
    blueSliderSizer->Add(new wxStaticText(this, wxID_ANY, _("Blue:")), sliderLabelFlags);
    blueSliderSizer->Add(m_blueSlider, onesliderFlags);

    sliderSizer->Add(redSliderSizer, sliderFlags);
    sliderSizer->Add(greenSliderSizer, sliderFlags);
    sliderSizer->Add(blueSliderSizer, sliderFlags);
    if ( m_colourData.GetChooseAlpha() )
    {
        wxBoxSizer *alphaSliderSizer = new wxBoxSizer(wxVERTICAL);
        alphaSliderSizer->Add(new wxStaticText(this, wxID_ANY, _("Opacity:")), sliderLabelFlags);
        alphaSliderSizer->Add(m_alphaSlider, onesliderFlags);
        sliderSizer->Add(alphaSliderSizer, sliderFlags);
    }

    topSizer->Add(sliderSizer, wxSizerFlags().Centre().DoubleBorder());
#else
    topSizer->Add(1, sliderHeight, wxSizerFlags(1).Centre().TripleBorder());
#endif // wxUSE_SLIDER

    // then the custom button
    topSizer->Add(new wxButton(this, wxID_ADD_CUSTOM,
                                  _("Add to custom colours") ),
                     wxSizerFlags().DoubleHorzBorder());

    // then the standard buttons
    wxSizer *buttonsizer = CreateSeparatedButtonSizer(wxOK | wxCANCEL);
    if ( buttonsizer )
    {
        topSizer->Add(buttonsizer, wxSizerFlags().Expand().DoubleBorder());
    }

    SetAutoLayout( true );
    SetSizer( topSizer );

    topSizer->SetSizeHints( this );
    topSizer->Fit( this );

    Centre( wxBOTH );

    wxEndBusyCursor();
}

void wxGenericColourDialog::InitializeColours(void)
{
    size_t i;

    for (i = 0; i < WXSIZEOF(wxColourDialogNames); i++)
    {
        wxColour col = wxTheColourDatabase->Find(wxColourDialogNames[i]);
        if (col.IsOk())
            m_standardColours[i].Set(col.Red(), col.Green(), col.Blue());
        else
            m_standardColours[i].Set(0, 0, 0);
    }

    for (i = 0; i < WXSIZEOF(m_customColours); i++)
    {
        wxColour c = m_colourData.GetCustomColour(i);
        if (c.IsOk())
            m_customColours[i] = m_colourData.GetCustomColour(i);
        else
            m_customColours[i] = wxColour(255, 255, 255);
    }

    wxColour curr = m_colourData.GetColour();
    if ( curr.IsOk() )
    {
        bool m_initColourFound = false;

        for (i = 0; i < WXSIZEOF(wxColourDialogNames); i++)
        {
            if ( m_standardColours[i] == curr && !m_initColourFound )
            {
                m_whichKind = 1;
                m_colourSelection = i;
                m_initColourFound = true;
                break;
            }
        }
        if ( !m_initColourFound )
        {
            for ( i = 0; i < WXSIZEOF(m_customColours); i++ )
            {
                if ( m_customColours[i] == curr )
                {
                    m_whichKind = 2;
                    m_colourSelection = i;
                    break;
                }
            }
        }
        m_colourData.SetColour(curr);
    }
    else
    {
        m_whichKind = 1;
        m_colourSelection = 0;
        m_colourData.SetColour(wxColour(0, 0, 0));
    }
}

void wxGenericColourDialog::PaintBasicColours(wxDC& dc)
{
    int i;
    for (i = 0; i < 6; i++)
    {
        int j;
        for (j = 0; j < 8; j++)
        {
            int ptr = i*8 + j;

            int x = (j*(m_smallRectangleSize.x+m_gridSpacing) + m_standardColoursRect.x);
            int y = (i*(m_smallRectangleSize.y+m_gridSpacing) + m_standardColoursRect.y);

            dc.SetPen(*wxBLACK_PEN);
            wxBrush brush(m_standardColours[ptr]);
            dc.SetBrush(brush);

            dc.DrawRectangle( x, y, m_smallRectangleSize.x, m_smallRectangleSize.y);
        }
    }
}

#if !wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
void wxGenericColourDialog::PaintCustomColours(wxDC& dc, int clrIndex)
{
    int idxStart;
    int idxEnd;
    // For clrIndex == -1 redraw all custom colours
    if ( clrIndex < 0 || static_cast<unsigned>(clrIndex) >= WXSIZEOF(m_customColours) )
    {
        idxStart = 0;
        idxEnd = WXSIZEOF(m_customColours) - 1;
    }
    else
    {
        idxStart = clrIndex;
        idxEnd = clrIndex;
    }

    for (int i = idxStart; i <= idxEnd; i++)
    {
        int x = ((i % 8)*(m_smallRectangleSize.x+m_gridSpacing)) + m_customColoursRect.x;
        int y = ((i / 8)*(m_smallRectangleSize.y+m_gridSpacing)) + m_customColoursRect.y;

        dc.SetPen(*wxBLACK_PEN);

        wxBrush brush(m_customColours[i]);
        dc.SetBrush(brush);

        dc.DrawRectangle(x, y, m_smallRectangleSize.x, m_smallRectangleSize.y);
    }
}
#endif // !wxCLRDLGG_USE_PREVIEW_WITH_ALPHA

void wxGenericColourDialog::PaintHighlight(wxDC& dc, bool draw)
{
  if ( m_colourSelection < 0 )
      return;

  wxRect r(m_smallRectangleSize);
  if (m_whichKind == 1)
  {
    // Standard colours
    r.Offset(m_standardColoursRect.GetLeftTop());
  }
  else
  {
    // User-defined colours
    r.Offset(m_customColoursRect.GetLeftTop());
  }

  const int x = (m_colourSelection % 8) * (m_smallRectangleSize.x + m_gridSpacing);
  const int y = (m_colourSelection / 8) * (m_smallRectangleSize.y + m_gridSpacing);
  r.Offset(x, y);
  // Highlight is drawn with rectangle bigger than the item rectangle.
  r.Inflate(2, 2);

  // Highlighting frame is drawn with black colour.
  // Highlighting frame is removed by drawing using dialog background colour.
  wxPen pen(draw ? *wxBLACK_PEN : wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE)));

  dc.SetPen(pen);
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  dc.DrawRectangle(r);
}

void wxGenericColourDialog::PaintCustomColour(wxDC& dc)
{
#if wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
    wxUnusedVar(dc);

    wxBitmap bmp(m_singleCustomColourRect.GetSize(), 32);
    bmp.UseAlpha();
    DoPreviewBitmap(bmp, m_colourData.GetColour());
    m_customColourBmp->SetBitmap(bmp);
#else
    dc.SetPen(*wxBLACK_PEN);

    wxBrush *brush = new wxBrush(m_colourData.GetColour());
    dc.SetBrush(*brush);

    dc.DrawRectangle(m_singleCustomColourRect);

    dc.SetBrush(wxNullBrush);
    delete brush;
#endif // wxCLRDLGG_USE_PREVIEW_WITH_ALPHA/!wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
}

void wxGenericColourDialog::OnBasicColourClick(int which)
{
    wxClientDC dc(this);

    PaintHighlight(dc, false);
    m_whichKind = 1;
    m_colourSelection = which;

#if wxUSE_SLIDER
    m_redSlider->SetValue( m_standardColours[m_colourSelection].Red() );
    m_greenSlider->SetValue( m_standardColours[m_colourSelection].Green() );
    m_blueSlider->SetValue( m_standardColours[m_colourSelection].Blue() );
    if ( m_colourData.GetChooseAlpha() )
    {
        m_alphaSlider->SetValue( m_standardColours[m_colourSelection].Alpha() );
    }
#endif // wxUSE_SLIDER

    m_colourData.SetColour(m_standardColours[m_colourSelection]);

    PaintCustomColour(dc);
    PaintHighlight(dc, true);
}

void wxGenericColourDialog::OnCustomColourClick(int which)
{
    wxClientDC dc(this);
    PaintHighlight(dc, false);
    m_whichKind = 2;
    m_colourSelection = which;

#if wxUSE_SLIDER
    m_redSlider->SetValue( m_customColours[m_colourSelection].Red() );
    m_greenSlider->SetValue( m_customColours[m_colourSelection].Green() );
    m_blueSlider->SetValue( m_customColours[m_colourSelection].Blue() );
    if ( m_colourData.GetChooseAlpha() )
    {
        m_alphaSlider->SetValue( m_customColours[m_colourSelection].Alpha() );
    }
#endif // wxUSE_SLIDER

    m_colourData.SetColour(m_customColours[m_colourSelection]);

    PaintCustomColour(dc);
    PaintHighlight(dc, true);
}

/*
void wxGenericColourDialog::OnOk(void)
{
  Show(false);
}

void wxGenericColourDialog::OnCancel(void)
{
  colourDialogCancelled = true;
  Show(false);
}
*/

void wxGenericColourDialog::OnAddCustom(wxCommandEvent& WXUNUSED(event))
{
  wxClientDC dc(this);
  if (m_whichKind != 2)
  {
    PaintHighlight(dc, false);
    m_whichKind = 2;
    m_colourSelection = 0;
    PaintHighlight(dc, true);
  }

  m_customColours[m_colourSelection] = m_colourData.GetColour();

  m_colourData.SetCustomColour(m_colourSelection, m_customColours[m_colourSelection]);

#if wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
    wxBitmap bmp(m_smallRectangleSize, 32);
    bmp.UseAlpha();
    DoPreviewBitmap(bmp, m_customColours[m_colourSelection]);
    m_customColoursBmp[m_colourSelection]->SetBitmap(bmp);
#else
    PaintCustomColours(dc, m_colourSelection);
#endif // wxCLRDLGG_USE_PREVIEW_WITH_ALPHA/!wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
}

#if wxUSE_SLIDER

void wxGenericColourDialog::OnRedSlider(wxCommandEvent& WXUNUSED(event))
{
  if (!m_redSlider)
    return;

  wxClientDC dc(this);
  wxColour c = m_colourData.GetColour();
  m_colourData.SetColour(wxColour((unsigned char)m_redSlider->GetValue(), c.Green(), c.Blue(), c.Alpha()));
  PaintCustomColour(dc);
}

void wxGenericColourDialog::OnGreenSlider(wxCommandEvent& WXUNUSED(event))
{
  if (!m_greenSlider)
    return;

  wxClientDC dc(this);
  wxColour c = m_colourData.GetColour();
  m_colourData.SetColour(wxColour(c.Red(), (unsigned char)m_greenSlider->GetValue(), c.Blue(), c.Alpha()));
  PaintCustomColour(dc);
}

void wxGenericColourDialog::OnBlueSlider(wxCommandEvent& WXUNUSED(event))
{
  if (!m_blueSlider)
    return;

  wxClientDC dc(this);
  wxColour c = m_colourData.GetColour();
  m_colourData.SetColour(wxColour(c.Red(), c.Green(), (unsigned char)m_blueSlider->GetValue(), c.Alpha()));
  PaintCustomColour(dc);
}

void wxGenericColourDialog::OnAlphaSlider(wxCommandEvent& WXUNUSED(event))
{
    wxColour c = m_colourData.GetColour();
    m_colourData.SetColour(wxColour(c.Red(), c.Green(), c.Blue(), (unsigned char)m_alphaSlider->GetValue()));

    wxClientDC dc(this);
    PaintCustomColour(dc);
}

#endif // wxUSE_SLIDER

#if wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
void wxGenericColourDialog::DoPreviewBitmap(wxBitmap& bmp, const wxColour& colour)
{
    if ( bmp.HasAlpha() && colour.Alpha() != wxALPHA_OPAQUE )
    {
        // For real ARGB draw a chessboard grid
        // with actual ARGB fields and reference RGB fields.
        const int w = bmp.GetWidth();
        const int h = bmp.GetHeight();

        // Calculate field size: 4 fields per row/column,
        // with size in range [2..10]
        int dx = wxMax(wxMin(w / 4, 10), 2);
        int dy = wxMax(wxMin(h / 4, 10), 2);
        // We want a square field
        dx = wxMax(dx, dy);
        dy = dx;

        // Prepare opaque colour
        wxColour colourRGB(colour.Red(), colour.Green(), colour.Blue(), wxALPHA_OPAQUE);

        {
            wxBrush brushARGB(colour);
            wxBrush brushRGB(colourRGB);

            wxMemoryDC mdc(bmp);
            {
                wxGCDC gdc(mdc);

                gdc.SetPen(*wxTRANSPARENT_PEN);

                for (int x = 0, ix = 0; x < w; x += dx, ix++)
                {
                    for (int y = 0, iy = 0; y < h; y += dy, iy++)
                    {
                        if ( (ix+iy) % 2 == 0 )
                        {
                            gdc.SetBrush(brushARGB);
                        }
                        else
                        {
                            gdc.SetBrush(brushRGB);
                        }
                        gdc.DrawRectangle(x, y, dx, dy);
                    }
                }

                // Draw a frame
                gdc.SetPen(*wxBLACK_PEN);
                gdc.SetBrush(*wxTRANSPARENT_BRUSH);
                gdc.DrawRectangle(0, 0, w, h);
            }
        }
    }
    else
    {
        wxMemoryDC mdc(bmp);
        // Fill with custom colour
        wxBrush brush(colour);
        mdc.SetPen(*wxBLACK_PEN);
        mdc.SetBrush(brush);
        mdc.DrawRectangle(wxPoint(0, 0), bmp.GetSize());
    }
}
#endif // wxCLRDLGG_USE_PREVIEW_WITH_ALPHA

#endif // wxUSE_COLOURDLG
