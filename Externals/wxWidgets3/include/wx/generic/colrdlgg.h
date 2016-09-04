/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/colrdlgg.h
// Purpose:     wxGenericColourDialog
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COLORDLGG_H_
#define _WX_COLORDLGG_H_

#include "wx/gdicmn.h"
#include "wx/dialog.h"

#if wxUSE_SLIDER
    class WXDLLIMPEXP_FWD_CORE wxSlider;
#endif // wxUSE_SLIDER

// Preview with opacity is possible only if wxGCDC and wxStaticBitmap are
// available and currently it only works in wxOSX and wxMSW as it uses wxBitmap
// UseAlpha() and HasAlpha() methods which only these ports provide.
#define wxCLRDLGG_USE_PREVIEW_WITH_ALPHA \
    ((wxUSE_GRAPHICS_CONTEXT && wxUSE_STATBMP) && \
     (defined(__WXMSW__) || defined(__WXOSX__)))

#if wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
class wxStaticBitmap;
#endif // wxCLRDLGG_USE_PREVIEW_WITH_ALPHA

class WXDLLIMPEXP_CORE wxGenericColourDialog : public wxDialog
{
public:
    wxGenericColourDialog();
    wxGenericColourDialog(wxWindow *parent,
                          wxColourData *data = NULL);
    virtual ~wxGenericColourDialog();

    bool Create(wxWindow *parent, wxColourData *data = NULL);

    wxColourData &GetColourData() { return m_colourData; }

    virtual int ShowModal() wxOVERRIDE;

    // Internal functions
    void OnMouseEvent(wxMouseEvent& event);
    void OnPaint(wxPaintEvent& event);
#if wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
    void OnCustomColourMouseClick(wxMouseEvent& event);
#endif // wxCLRDLGG_USE_PREVIEW_WITH_ALPHA

    virtual void CalculateMeasurements();
    virtual void CreateWidgets();
    virtual void InitializeColours();

    virtual void PaintBasicColours(wxDC& dc);
#if !wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
    virtual void PaintCustomColours(wxDC& dc, int clrIndex = -1);
#endif // !wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
    virtual void PaintCustomColour(wxDC& dc);
    virtual void PaintHighlight(wxDC& dc, bool draw);

    virtual void OnBasicColourClick(int which);
    virtual void OnCustomColourClick(int which);

    void OnAddCustom(wxCommandEvent& event);

#if wxUSE_SLIDER
    void OnRedSlider(wxCommandEvent& event);
    void OnGreenSlider(wxCommandEvent& event);
    void OnBlueSlider(wxCommandEvent& event);
    void OnAlphaSlider(wxCommandEvent& event);
#endif // wxUSE_SLIDER

    void OnCloseWindow(wxCloseEvent& event);

#if wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
    void DoPreviewBitmap(wxBitmap& bmp, const wxColour& colour);
#endif // wxCLRDLGG_USE_PREVIEW_WITH_ALPHA

protected:
    wxColourData m_colourData;

    // Area reserved for grids of colours
    wxRect m_standardColoursRect;
    wxRect m_customColoursRect;
    wxRect m_singleCustomColourRect;

    // Size of each colour rectangle
    wxSize m_smallRectangleSize;

    // Grid spacing (between rectangles)
    int m_gridSpacing;

    // Section spacing (between left and right halves of dialog box)
    int m_sectionSpacing;

    // 48 'standard' colours
    wxColour m_standardColours[48];

    // 16 'custom' colours
    wxColour m_customColours[16];

    // Which colour is selected? An index into one of the two areas.
    int m_colourSelection;
    int m_whichKind; // 1 for standard colours, 2 for custom colours,

#if wxUSE_SLIDER
    wxSlider *m_redSlider;
    wxSlider *m_greenSlider;
    wxSlider *m_blueSlider;
    wxSlider *m_alphaSlider;
#endif // wxUSE_SLIDER
#if wxCLRDLGG_USE_PREVIEW_WITH_ALPHA
    // Bitmap to preview selected colour (with alpha channel)
    wxStaticBitmap *m_customColourBmp;
    // Bitmaps to preview custom colours (with alpha channel)
    wxStaticBitmap *m_customColoursBmp[16];
#endif // wxCLRDLGG_USE_PREVIEW_WITH_ALPHA

    int m_buttonY;

    int m_okButtonX;
    int m_customButtonX;

    //  static bool colourDialogCancelled;

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS(wxGenericColourDialog);
};

#endif // _WX_COLORDLGG_H_
