///////////////////////////////////////////////////////////////////////////////
// Name:        src/aui/dockart.cpp
// Purpose:     wxaui: wx advanced user interface - docking window manager
// Author:      Benjamin I. Williams
// Modified by:
// Created:     2005-05-17
// RCS-ID:      $Id: dockart.cpp 66670 2011-01-12 13:39:36Z VZ $
// Copyright:   (C) Copyright 2005-2006, Kirix Corporation, All Rights Reserved
// Licence:     wxWindows Library Licence, Version 3.1
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_AUI

#include "wx/aui/framemanager.h"
#include "wx/aui/dockart.h"

#ifndef WX_PRECOMP
    #include "wx/settings.h"
    #include "wx/dcclient.h"
    #include "wx/image.h"
#endif

#ifdef __WXMAC__
#include "wx/osx/private.h"
#include "wx/graphics.h"
#include "wx/dcgraph.h"
#endif

#ifdef __WXGTK__
#include <gtk/gtk.h>
#include "wx/renderer.h"
#endif


// -- wxAuiDefaultDockArt class implementation --

// wxAuiDefaultDockArt is an art provider class which does all of the drawing for
// wxAuiManager.  This allows the library caller to customize the dock art
// (probably by deriving from this class), or to completely replace all drawing
// with custom dock art (probably by writing a new stand-alone class derived
// from the wxAuiDockArt base class). The active dock art class can be set via
// wxAuiManager::SetDockArt()
wxColor wxAuiLightContrastColour(const wxColour& c)
{
    int amount = 120;

    // if the color is especially dark, then
    // make the contrast even lighter
    if (c.Red() < 128 && c.Green() < 128 && c.Blue() < 128)
        amount = 160;

    return c.ChangeLightness(amount);
}

// wxAuiBitmapFromBits() is a utility function that creates a
// masked bitmap from raw bits (XBM format)
wxBitmap wxAuiBitmapFromBits(const unsigned char bits[], int w, int h,
                             const wxColour& color)
{
    wxImage img = wxBitmap((const char*)bits, w, h).ConvertToImage();
    img.Replace(0,0,0,123,123,123);
    img.Replace(255,255,255,color.Red(),color.Green(),color.Blue());
    img.SetMaskColour(123,123,123);
    return wxBitmap(img);
}


static void DrawGradientRectangle(wxDC& dc,
                                  const wxRect& rect,
                                  const wxColour& start_color,
                                  const wxColour& end_color,
                                  int direction)
{
    int rd, gd, bd, high = 0;
    rd = end_color.Red() - start_color.Red();
    gd = end_color.Green() - start_color.Green();
    bd = end_color.Blue() - start_color.Blue();

    if (direction == wxAUI_GRADIENT_VERTICAL)
        high = rect.GetHeight()-1;
    else
        high = rect.GetWidth()-1;

    for (int i = 0; i <= high; ++i)
    {
        int r,g,b;


        r = start_color.Red() + (high <= 0 ? 0 : (((i*rd*100)/high)/100));
        g = start_color.Green() + (high <= 0 ? 0 : (((i*gd*100)/high)/100));
        b = start_color.Blue() + (high <= 0 ? 0 : (((i*bd*100)/high)/100));

        wxPen p(wxColor((unsigned char)r,
                        (unsigned char)g,
                        (unsigned char)b));
        dc.SetPen(p);

        if (direction == wxAUI_GRADIENT_VERTICAL)
            dc.DrawLine(rect.x, rect.y+i, rect.x+rect.width, rect.y+i);
        else
            dc.DrawLine(rect.x+i, rect.y, rect.x+i, rect.y+rect.height);
    }
}

wxString wxAuiChopText(wxDC& dc, const wxString& text, int max_size)
{
    wxCoord x,y;

    // first check if the text fits with no problems
    dc.GetTextExtent(text, &x, &y);
    if (x <= max_size)
        return text;

    size_t i, len = text.Length();
    size_t last_good_length = 0;
    for (i = 0; i < len; ++i)
    {
        wxString s = text.Left(i);
        s += wxT("...");

        dc.GetTextExtent(s, &x, &y);
        if (x > max_size)
            break;

        last_good_length = i;
    }

    wxString ret = text.Left(last_good_length);
    ret += wxT("...");
    return ret;
}

wxAuiDefaultDockArt::wxAuiDefaultDockArt()
{
#if defined( __WXMAC__ ) && wxOSX_USE_COCOA_OR_CARBON
    wxColor base_colour = wxColour( wxMacCreateCGColorFromHITheme(kThemeBrushToolbarBackground));
#else
    wxColor base_colour = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
#endif

    // the base_colour is too pale to use as our base colour,
    // so darken it a bit --
    if ((255-base_colour.Red()) +
        (255-base_colour.Green()) +
        (255-base_colour.Blue()) < 60)
    {
        base_colour = base_colour.ChangeLightness(92);
    }

    m_base_colour = base_colour;
    wxColor darker1_colour = base_colour.ChangeLightness(85);
    wxColor darker2_colour = base_colour.ChangeLightness(75);
    wxColor darker3_colour = base_colour.ChangeLightness(60);
    //wxColor darker4_colour = base_colour.ChangeLightness(50);
    wxColor darker5_colour = base_colour.ChangeLightness(40);

    m_active_caption_colour = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
    m_active_caption_gradient_colour = wxAuiLightContrastColour(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
    m_active_caption_text_colour = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
    m_inactive_caption_colour = darker1_colour;
    m_inactive_caption_gradient_colour = base_colour.ChangeLightness(97);
    m_inactive_caption_text_colour = *wxBLACK;

    m_sash_brush = wxBrush(base_colour);
    m_background_brush = wxBrush(base_colour);
    m_gripper_brush = wxBrush(base_colour);

    m_border_pen = wxPen(darker2_colour);
    m_gripper_pen1 = wxPen(darker5_colour);
    m_gripper_pen2 = wxPen(darker3_colour);
    m_gripper_pen3 = *wxWHITE_PEN;

#ifdef __WXMAC__
    m_caption_font = *wxSMALL_FONT;
#else
    m_caption_font = wxFont(8, wxDEFAULT, wxNORMAL, wxNORMAL, FALSE);
#endif

    // default metric values
#if defined( __WXMAC__ ) && wxOSX_USE_COCOA_OR_CARBON
    SInt32 height;
    GetThemeMetric( kThemeMetricSmallPaneSplitterHeight , &height );
    m_sash_size = height;
#elif defined(__WXGTK__)
    m_sash_size = wxRendererNative::Get().GetSplitterParams(NULL).widthSash;
#else
    m_sash_size = 4;
#endif
    m_caption_size = 17;
    m_border_size = 1;
    m_button_size = 14;
    m_gripper_size = 9;
    m_gradient_type = wxAUI_GRADIENT_VERTICAL;

    InitBitmaps();
}

void
wxAuiDefaultDockArt::InitBitmaps ()
{
    // some built in bitmaps
#if defined( __WXMAC__ )
     static const unsigned char close_bits[]={
         0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xFE, 0x03, 0xF8, 0x01, 0xF0, 0x19, 0xF3,
         0xB8, 0xE3, 0xF0, 0xE1, 0xE0, 0xE0, 0xF0, 0xE1, 0xB8, 0xE3, 0x19, 0xF3,
         0x01, 0xF0, 0x03, 0xF8, 0x0F, 0xFE, 0xFF, 0xFF };
#elif defined(__WXGTK__)
     static const unsigned char close_bits[]={
         0xff, 0xff, 0xff, 0xff, 0x07, 0xf0, 0xfb, 0xef, 0xdb, 0xed, 0x8b, 0xe8,
         0x1b, 0xec, 0x3b, 0xee, 0x1b, 0xec, 0x8b, 0xe8, 0xdb, 0xed, 0xfb, 0xef,
         0x07, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#else
    static const unsigned char close_bits[]={
         // reduced height, symmetric
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xf3, 0x9f, 0xf9,
         0x3f, 0xfc, 0x7f, 0xfe, 0x3f, 0xfc, 0x9f, 0xf9, 0xcf, 0xf3, 0xff, 0xff,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
      /*
         // same height as maximize/restore
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0xe7, 0xcf, 0xf3, 0x9f, 0xf9,
         0x3f, 0xfc, 0x7f, 0xfe, 0x3f, 0xfc, 0x9f, 0xf9, 0xcf, 0xf3, 0xe7, 0xe7,
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
      */
#endif

    static const unsigned char maximize_bits[] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0xf0, 0xf7, 0xf7, 0x07, 0xf0,
        0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0x07, 0xf0,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    static const unsigned char restore_bits[]={
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f, 0xf0, 0x1f, 0xf0, 0xdf, 0xf7,
        0x07, 0xf4, 0x07, 0xf4, 0xf7, 0xf5, 0xf7, 0xf1, 0xf7, 0xfd, 0xf7, 0xfd,
        0x07, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    static const unsigned char pin_bits[]={
        0xff,0xff,0xff,0xff,0xff,0xff,0x1f,0xfc,0xdf,0xfc,0xdf,0xfc,
        0xdf,0xfc,0xdf,0xfc,0xdf,0xfc,0x0f,0xf8,0x7f,0xff,0x7f,0xff,
        0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

#ifdef __WXMAC__
    m_inactive_close_bitmap = wxAuiBitmapFromBits(close_bits, 16, 16, *wxWHITE);
    m_active_close_bitmap = wxAuiBitmapFromBits(close_bits, 16, 16, *wxWHITE );
#else
    m_inactive_close_bitmap = wxAuiBitmapFromBits(close_bits, 16, 16, m_inactive_caption_text_colour);
    m_active_close_bitmap = wxAuiBitmapFromBits(close_bits, 16, 16, m_active_caption_text_colour);
#endif

#ifdef __WXMAC__
    m_inactive_maximize_bitmap = wxAuiBitmapFromBits(maximize_bits, 16, 16, *wxWHITE);
    m_active_maximize_bitmap = wxAuiBitmapFromBits(maximize_bits, 16, 16, *wxWHITE );
#else
    m_inactive_maximize_bitmap = wxAuiBitmapFromBits(maximize_bits, 16, 16, m_inactive_caption_text_colour);
    m_active_maximize_bitmap = wxAuiBitmapFromBits(maximize_bits, 16, 16, m_active_caption_text_colour);
#endif

#ifdef __WXMAC__
    m_inactive_restore_bitmap = wxAuiBitmapFromBits(restore_bits, 16, 16, *wxWHITE);
    m_active_restore_bitmap = wxAuiBitmapFromBits(restore_bits, 16, 16, *wxWHITE );
#else
    m_inactive_restore_bitmap = wxAuiBitmapFromBits(restore_bits, 16, 16, m_inactive_caption_text_colour);
    m_active_restore_bitmap = wxAuiBitmapFromBits(restore_bits, 16, 16, m_active_caption_text_colour);
#endif

    m_inactive_pin_bitmap = wxAuiBitmapFromBits(pin_bits, 16, 16, m_inactive_caption_text_colour);
    m_active_pin_bitmap = wxAuiBitmapFromBits(pin_bits, 16, 16, m_active_caption_text_colour);
}

int wxAuiDefaultDockArt::GetMetric(int id)
{
    switch (id)
    {
        case wxAUI_DOCKART_SASH_SIZE:          return m_sash_size;
        case wxAUI_DOCKART_CAPTION_SIZE:       return m_caption_size;
        case wxAUI_DOCKART_GRIPPER_SIZE:       return m_gripper_size;
        case wxAUI_DOCKART_PANE_BORDER_SIZE:   return m_border_size;
        case wxAUI_DOCKART_PANE_BUTTON_SIZE:   return m_button_size;
        case wxAUI_DOCKART_GRADIENT_TYPE:      return m_gradient_type;
        default: wxFAIL_MSG(wxT("Invalid Metric Ordinal")); break;
    }

    return 0;
}

void wxAuiDefaultDockArt::SetMetric(int id, int new_val)
{
    switch (id)
    {
        case wxAUI_DOCKART_SASH_SIZE:          m_sash_size = new_val; break;
        case wxAUI_DOCKART_CAPTION_SIZE:       m_caption_size = new_val; break;
        case wxAUI_DOCKART_GRIPPER_SIZE:       m_gripper_size = new_val; break;
        case wxAUI_DOCKART_PANE_BORDER_SIZE:   m_border_size = new_val; break;
        case wxAUI_DOCKART_PANE_BUTTON_SIZE:   m_button_size = new_val; break;
        case wxAUI_DOCKART_GRADIENT_TYPE:      m_gradient_type = new_val; break;
        default: wxFAIL_MSG(wxT("Invalid Metric Ordinal")); break;
    }
}

wxColour wxAuiDefaultDockArt::GetColour(int id)
{
    switch (id)
    {
        case wxAUI_DOCKART_BACKGROUND_COLOUR:                return m_background_brush.GetColour();
        case wxAUI_DOCKART_SASH_COLOUR:                      return m_sash_brush.GetColour();
        case wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR:          return m_inactive_caption_colour;
        case wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR: return m_inactive_caption_gradient_colour;
        case wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR:     return m_inactive_caption_text_colour;
        case wxAUI_DOCKART_ACTIVE_CAPTION_COLOUR:            return m_active_caption_colour;
        case wxAUI_DOCKART_ACTIVE_CAPTION_GRADIENT_COLOUR:   return m_active_caption_gradient_colour;
        case wxAUI_DOCKART_ACTIVE_CAPTION_TEXT_COLOUR:       return m_active_caption_text_colour;
        case wxAUI_DOCKART_BORDER_COLOUR:                    return m_border_pen.GetColour();
        case wxAUI_DOCKART_GRIPPER_COLOUR:                   return m_gripper_brush.GetColour();
        default: wxFAIL_MSG(wxT("Invalid Metric Ordinal")); break;
    }

    return wxColour();
}

void wxAuiDefaultDockArt::SetColour(int id, const wxColor& colour)
{
    switch (id)
    {
        case wxAUI_DOCKART_BACKGROUND_COLOUR:                m_background_brush.SetColour(colour); break;
        case wxAUI_DOCKART_SASH_COLOUR:                      m_sash_brush.SetColour(colour); break;
        case wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR:          m_inactive_caption_colour = colour; break;
        case wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR: m_inactive_caption_gradient_colour = colour; break;
        case wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR:     m_inactive_caption_text_colour = colour; break;
        case wxAUI_DOCKART_ACTIVE_CAPTION_COLOUR:            m_active_caption_colour = colour; break;
        case wxAUI_DOCKART_ACTIVE_CAPTION_GRADIENT_COLOUR:   m_active_caption_gradient_colour = colour; break;
        case wxAUI_DOCKART_ACTIVE_CAPTION_TEXT_COLOUR:       m_active_caption_text_colour = colour; break;
        case wxAUI_DOCKART_BORDER_COLOUR:                    m_border_pen.SetColour(colour); break;
        case wxAUI_DOCKART_GRIPPER_COLOUR:
            m_gripper_brush.SetColour(colour);
            m_gripper_pen1.SetColour(colour.ChangeLightness(40));
            m_gripper_pen2.SetColour(colour.ChangeLightness(60));
            break;
        default: wxFAIL_MSG(wxT("Invalid Metric Ordinal")); break;
    }

    InitBitmaps();
}

void wxAuiDefaultDockArt::SetFont(int id, const wxFont& font)
{
    if (id == wxAUI_DOCKART_CAPTION_FONT)
        m_caption_font = font;
}

wxFont wxAuiDefaultDockArt::GetFont(int id)
{
    if (id == wxAUI_DOCKART_CAPTION_FONT)
        return m_caption_font;
    return wxNullFont;
}

void wxAuiDefaultDockArt::DrawSash(wxDC& dc, wxWindow *window, int orientation, const wxRect& rect)
{
#if defined( __WXMAC__ ) && wxOSX_USE_COCOA_OR_CARBON
    wxUnusedVar(window);
    wxUnusedVar(orientation);

    HIRect splitterRect = CGRectMake( rect.x , rect.y , rect.width , rect.height );
    CGContextRef cgContext ;
    wxGCDCImpl *impl = (wxGCDCImpl*) dc.GetImpl();
    cgContext = (CGContextRef) impl->GetGraphicsContext()->GetNativeContext() ;

    HIThemeSplitterDrawInfo drawInfo ;
    drawInfo.version = 0 ;
    drawInfo.state = kThemeStateActive ;
    drawInfo.adornment = kHIThemeSplitterAdornmentNone ;
    HIThemeDrawPaneSplitter( &splitterRect , &drawInfo , cgContext , kHIThemeOrientationNormal ) ;

#elif defined(__WXGTK__)
    // clear out the rectangle first
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(m_sash_brush);
    dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);

#if 0
    GdkRectangle gdk_rect;
    if (orientation == wxVERTICAL )
    {
        gdk_rect.x = rect.x;
        gdk_rect.y = rect.y;
        gdk_rect.width = m_sash_size;
        gdk_rect.height = rect.height;
    }
    else
    {
        gdk_rect.x = rect.x;
        gdk_rect.y = rect.y;
        gdk_rect.width = rect.width;
        gdk_rect.height = m_sash_size;
    }
#endif

    if (!window) return;
    if (!window->m_wxwindow) return;
    if (!GTK_WIDGET_DRAWABLE(window->m_wxwindow)) return;

    gtk_paint_handle
    (
        window->m_wxwindow->style,
        window->GTKGetDrawingWindow(),
        // flags & wxCONTROL_CURRENT ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL,
        GTK_STATE_NORMAL,
        GTK_SHADOW_NONE,
        NULL /* no clipping */,
        window->m_wxwindow,
        "paned",
        rect.x,
        rect.y,
        rect.width,
        rect.height,
        (orientation == wxVERTICAL) ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL
    );

#else
    wxUnusedVar(window);
    wxUnusedVar(orientation);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(m_sash_brush);
    dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);
#endif
}


void wxAuiDefaultDockArt::DrawBackground(wxDC& dc, wxWindow *WXUNUSED(window), int, const wxRect& rect)
{
    dc.SetPen(*wxTRANSPARENT_PEN);
#ifdef __WXMAC__
    // we have to clear first, otherwise we are drawing a light striped pattern
    // over an already darker striped background
    dc.SetBrush(*wxWHITE_BRUSH) ;
    dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);
#endif
    dc.SetBrush(m_background_brush);
    dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);
}

void wxAuiDefaultDockArt::DrawBorder(wxDC& dc, wxWindow *WXUNUSED(window), const wxRect& _rect,
                                  wxAuiPaneInfo& pane)
{
    dc.SetPen(m_border_pen);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    wxRect rect = _rect;
    int i, border_width = GetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE);

    if (pane.IsToolbar())
    {
        for (i = 0; i < border_width; ++i)
        {
            dc.SetPen(*wxWHITE_PEN);
            dc.DrawLine(rect.x, rect.y, rect.x+rect.width, rect.y);
            dc.DrawLine(rect.x, rect.y, rect.x, rect.y+rect.height);
            dc.SetPen(m_border_pen);
            dc.DrawLine(rect.x, rect.y+rect.height-1,
                        rect.x+rect.width, rect.y+rect.height-1);
            dc.DrawLine(rect.x+rect.width-1, rect.y,
                        rect.x+rect.width-1, rect.y+rect.height);
            rect.Deflate(1);
        }
    }
    else
    {
        for (i = 0; i < border_width; ++i)
        {
            dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);
            rect.Deflate(1);
        }
    }
}


void wxAuiDefaultDockArt::DrawCaptionBackground(wxDC& dc, const wxRect& rect, bool active)
{
    if (m_gradient_type == wxAUI_GRADIENT_NONE)
    {
        if (active)
            dc.SetBrush(wxBrush(m_active_caption_colour));
        else
            dc.SetBrush(wxBrush(m_inactive_caption_colour));

        dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);
    }
    else
    {
        if (active)
        {
            // on mac the gradients are expected to become darker from the top
#ifdef __WXMAC__
            DrawGradientRectangle(dc, rect,
                                 m_active_caption_colour,
                                 m_active_caption_gradient_colour,
                                 m_gradient_type);
#else
            // on other platforms, active gradients become lighter at the top
            DrawGradientRectangle(dc, rect,
                                 m_active_caption_gradient_colour,
                                 m_active_caption_colour,
                                 m_gradient_type);
#endif
        }
        else
        {
#ifdef __WXMAC__
            // on mac the gradients are expected to become darker from the top
            DrawGradientRectangle(dc, rect,
                                 m_inactive_caption_gradient_colour,
                                 m_inactive_caption_colour,
                                 m_gradient_type);
#else
            // on other platforms, inactive gradients become lighter at the bottom
            DrawGradientRectangle(dc, rect,
                                 m_inactive_caption_colour,
                                 m_inactive_caption_gradient_colour,
                                 m_gradient_type);
#endif
        }
    }
}


void wxAuiDefaultDockArt::DrawCaption(wxDC& dc, wxWindow *WXUNUSED(window),
                                   const wxString& text,
                                   const wxRect& rect,
                                   wxAuiPaneInfo& pane)
{
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetFont(m_caption_font);

    DrawCaptionBackground(dc, rect,
                          (pane.state & wxAuiPaneInfo::optionActive)?true:false);

    int caption_offset = 0;
    if ( pane.icon.IsOk() )
    {
        DrawIcon(dc, rect, pane);

        caption_offset += pane.icon.GetWidth() + 3;
    }

    if (pane.state & wxAuiPaneInfo::optionActive)
        dc.SetTextForeground(m_active_caption_text_colour);
    else
        dc.SetTextForeground(m_inactive_caption_text_colour);


    wxCoord w,h;
    dc.GetTextExtent(wxT("ABCDEFHXfgkj"), &w, &h);

    wxRect clip_rect = rect;
    clip_rect.width -= 3; // text offset
    clip_rect.width -= 2; // button padding
    if (pane.HasCloseButton())
        clip_rect.width -= m_button_size;
    if (pane.HasPinButton())
        clip_rect.width -= m_button_size;
    if (pane.HasMaximizeButton())
        clip_rect.width -= m_button_size;

    wxString draw_text = wxAuiChopText(dc, text, clip_rect.width);

    dc.SetClippingRegion(clip_rect);
    dc.DrawText(draw_text, rect.x+3 + caption_offset, rect.y+(rect.height/2)-(h/2)-1);
    dc.DestroyClippingRegion();
}

void
wxAuiDefaultDockArt::DrawIcon(wxDC& dc, const wxRect& rect, wxAuiPaneInfo& pane)
{
   // Draw the icon centered vertically
   dc.DrawBitmap(pane.icon,
                 rect.x+2, rect.y+(rect.height-pane.icon.GetHeight())/2,
                 true);
}

void wxAuiDefaultDockArt::DrawGripper(wxDC& dc, wxWindow *WXUNUSED(window),
                                   const wxRect& rect,
                                   wxAuiPaneInfo& pane)
{
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(m_gripper_brush);

    dc.DrawRectangle(rect.x, rect.y, rect.width,rect.height);

    if (!pane.HasGripperTop())
    {
        int y = 5;
        while (1)
        {
            dc.SetPen(m_gripper_pen1);
            dc.DrawPoint(rect.x+3, rect.y+y);
            dc.SetPen(m_gripper_pen2);
            dc.DrawPoint(rect.x+3, rect.y+y+1);
            dc.DrawPoint(rect.x+4, rect.y+y);
            dc.SetPen(m_gripper_pen3);
            dc.DrawPoint(rect.x+5, rect.y+y+1);
            dc.DrawPoint(rect.x+5, rect.y+y+2);
            dc.DrawPoint(rect.x+4, rect.y+y+2);

            y += 4;
            if (y > rect.GetHeight()-5)
                break;
        }
    }
    else
    {
        int x = 5;
        while (1)
        {
            dc.SetPen(m_gripper_pen1);
            dc.DrawPoint(rect.x+x, rect.y+3);
            dc.SetPen(m_gripper_pen2);
            dc.DrawPoint(rect.x+x+1, rect.y+3);
            dc.DrawPoint(rect.x+x, rect.y+4);
            dc.SetPen(m_gripper_pen3);
            dc.DrawPoint(rect.x+x+1, rect.y+5);
            dc.DrawPoint(rect.x+x+2, rect.y+5);
            dc.DrawPoint(rect.x+x+2, rect.y+4);

            x += 4;
            if (x > rect.GetWidth()-5)
                break;
        }
    }
}

void wxAuiDefaultDockArt::DrawPaneButton(wxDC& dc, wxWindow *WXUNUSED(window),
                                      int button,
                                      int button_state,
                                      const wxRect& _rect,
                                      wxAuiPaneInfo& pane)
{
    wxBitmap bmp;
    if (!(&pane))
        return;
    switch (button)
    {
        default:
        case wxAUI_BUTTON_CLOSE:
            if (pane.state & wxAuiPaneInfo::optionActive)
                bmp = m_active_close_bitmap;
            else
                bmp = m_inactive_close_bitmap;
            break;
        case wxAUI_BUTTON_PIN:
            if (pane.state & wxAuiPaneInfo::optionActive)
                bmp = m_active_pin_bitmap;
            else
                bmp = m_inactive_pin_bitmap;
            break;
        case wxAUI_BUTTON_MAXIMIZE_RESTORE:
            if (pane.IsMaximized())
            {
                if (pane.state & wxAuiPaneInfo::optionActive)
                    bmp = m_active_restore_bitmap;
                else
                    bmp = m_inactive_restore_bitmap;
            }
            else
            {
                if (pane.state & wxAuiPaneInfo::optionActive)
                    bmp = m_active_maximize_bitmap;
                else
                    bmp = m_inactive_maximize_bitmap;
            }
            break;
    }


    wxRect rect = _rect;

    int old_y = rect.y;
    rect.y = rect.y + (rect.height/2) - (bmp.GetHeight()/2);
    rect.height = old_y + rect.height - rect.y - 1;


    if (button_state == wxAUI_BUTTON_STATE_PRESSED)
    {
        rect.x++;
        rect.y++;
    }

    if (button_state == wxAUI_BUTTON_STATE_HOVER ||
        button_state == wxAUI_BUTTON_STATE_PRESSED)
    {
        if (pane.state & wxAuiPaneInfo::optionActive)
        {
            dc.SetBrush(wxBrush(m_active_caption_colour.ChangeLightness(120)));
            dc.SetPen(wxPen(m_active_caption_colour.ChangeLightness(70)));
        }
        else
        {
            dc.SetBrush(wxBrush(m_inactive_caption_colour.ChangeLightness(120)));
            dc.SetPen(wxPen(m_inactive_caption_colour.ChangeLightness(70)));
        }

        // draw the background behind the button
        dc.DrawRectangle(rect.x, rect.y, 15, 15);
    }


    // draw the button itself
    dc.DrawBitmap(bmp, rect.x, rect.y, true);
}


#endif // wxUSE_AUI
