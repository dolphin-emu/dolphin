///////////////////////////////////////////////////////////////////////////////
// Name:        src/aui/tabartgtk.cpp
// Purpose:     implementation of the wxAuiGTKTabArt
// Author:      Jens Lody and Teodor Petrov
// Modified by:
// Created:     2012-03-23
// Copyright:   (c) 2012 Jens Lody <jens@codeblocks.org>
//                  and Teodor Petrov
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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

#if wxUSE_AUI

#ifndef WX_PRECOMP
    #include "wx/dc.h"
    #include "wx/dcclient.h"
    #include "wx/settings.h"
    #include "wx/image.h"
#endif

#include "wx/gtk/dc.h"
#include "wx/gtk/private.h"

#include <gtk/gtk.h>

#include "wx/aui/auibook.h"
#include "wx/aui/tabartgtk.h"
#include "wx/renderer.h"

namespace
{

static int s_CloseIconSize = 16; // default size

}

wxAuiGtkTabArt::wxAuiGtkTabArt()

{
}

wxAuiTabArt* wxAuiGtkTabArt::Clone()
{
    wxAuiGtkTabArt* clone = new wxAuiGtkTabArt();

    clone->SetNormalFont(m_normalFont);
    clone->SetSelectedFont(m_normalFont);
    clone->SetMeasuringFont(m_normalFont);

    return clone;
}

void wxAuiGtkTabArt::DrawBackground(wxDC& dc, wxWindow* WXUNUSED(wnd), const wxRect& rect)
{
    wxGTKDCImpl *impldc = (wxGTKDCImpl*) dc.GetImpl();
    GdkWindow* window = impldc->GetGDKWindow();

    gtk_style_apply_default_background(gtk_widget_get_style(wxGTKPrivate::GetNotebookWidget()),
                                       window,
                                       true,
                                       GTK_STATE_NORMAL,
                                       NULL,
                                       rect.x, rect.y, rect.width, rect.height);
}

void wxAuiGtkTabArt::DrawBorder(wxDC& WXUNUSED(dc), wxWindow* wnd, const wxRect& rect)
{
    int generic_border_width = wxAuiGenericTabArt::GetBorderWidth(wnd);

    if (!wnd) return;
    if (!wnd->m_wxwindow) return;
    if (!gtk_widget_is_drawable(wnd->m_wxwindow)) return;

    GtkStyle *style_notebook = gtk_widget_get_style(wxGTKPrivate::GetNotebookWidget());

    gtk_paint_box(style_notebook, wnd->GTKGetDrawingWindow(), GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                  NULL, wnd->m_wxwindow,
                  const_cast<char*>("notebook"),
                  rect.x + generic_border_width + 1, rect.y + generic_border_width + 1,
                  rect.width - (generic_border_width + 1), rect.height - (generic_border_width + 1));
}

void ButtonStateAndShadow(int button_state, GtkStateType &state, GtkShadowType &shadow)
{

    if (button_state & wxAUI_BUTTON_STATE_DISABLED)
    {
        state = GTK_STATE_INSENSITIVE;
        shadow = GTK_SHADOW_ETCHED_IN;
    }
    else if (button_state & wxAUI_BUTTON_STATE_HOVER)
    {
        state = GTK_STATE_PRELIGHT;
        shadow = GTK_SHADOW_OUT;
    }
    else if (button_state & wxAUI_BUTTON_STATE_PRESSED)
    {
        state = GTK_STATE_ACTIVE;
        shadow = GTK_SHADOW_IN;
    }
    else
    {
        state = GTK_STATE_NORMAL;
        shadow = GTK_SHADOW_OUT;
    }
}

wxRect DrawCloseButton(wxDC& dc,
                       GtkWidget *widget,
                       int button_state,
                       wxRect const &in_rect,
                       int orientation,
                       GdkRectangle* clipRect)
{
    GtkStyle *style_button = gtk_widget_get_style(wxGTKPrivate::GetButtonWidget());
    int xthickness = style_button->xthickness;
    int ythickness = style_button->ythickness;

    wxBitmap bmp(gtk_widget_render_icon(widget, GTK_STOCK_CLOSE, GTK_ICON_SIZE_SMALL_TOOLBAR, "tab"));

    if(bmp.GetWidth() != s_CloseIconSize || bmp.GetHeight() != s_CloseIconSize)
    {
        wxImage img = bmp.ConvertToImage();
        img.Rescale(s_CloseIconSize, s_CloseIconSize);
        bmp = img;
    }

    int button_size = s_CloseIconSize + 2 * xthickness;

    wxRect out_rect;

    if (orientation == wxLEFT)
        out_rect.x = in_rect.x - ythickness;
    else
        out_rect.x = in_rect.x + in_rect.width - button_size - ythickness;

    out_rect.y = in_rect.y + (in_rect.height - button_size) / 2;
    out_rect.width = button_size;
    out_rect.height = button_size;

    wxGTKDCImpl *impldc = (wxGTKDCImpl*) dc.GetImpl();
    GdkWindow* window = impldc->GetGDKWindow();

    if (button_state == wxAUI_BUTTON_STATE_HOVER)
    {
        gtk_paint_box(style_button, window,
                      GTK_STATE_PRELIGHT, GTK_SHADOW_OUT, clipRect, widget, "button",
                     out_rect.x, out_rect.y, out_rect.width, out_rect.height);
    }
    else if (button_state == wxAUI_BUTTON_STATE_PRESSED)
    {
        gtk_paint_box(style_button, window,
                      GTK_STATE_ACTIVE, GTK_SHADOW_IN, clipRect, widget, "button",
                      out_rect.x, out_rect.y, out_rect.width, out_rect.height);
    }


    dc.DrawBitmap(bmp, out_rect.x + xthickness, out_rect.y + ythickness, true);

    return out_rect;
}

void wxAuiGtkTabArt::DrawTab(wxDC& dc, wxWindow* wnd, const wxAuiNotebookPage& page,
                             const wxRect& in_rect, int close_button_state, wxRect* out_tab_rect,
                             wxRect* out_button_rect, int* x_extent)
{
    GtkWidget *widget = wnd->GetHandle();
    GtkStyle *style_notebook = gtk_widget_get_style(wxGTKPrivate::GetNotebookWidget());

    wxRect const &window_rect = wnd->GetRect();

    int focus_width = 0;

    gtk_widget_style_get(wxGTKPrivate::GetNotebookWidget(),
                         "focus-line-width", &focus_width,
                         NULL);

    int tab_pos;
    if (m_flags &wxAUI_NB_BOTTOM)
        tab_pos = wxAUI_NB_BOTTOM;
    else //if (m_flags & wxAUI_NB_TOP) {}
        tab_pos = wxAUI_NB_TOP;

    // TODO: else if (m_flags &wxAUI_NB_LEFT) {}
    // TODO: else if (m_flags &wxAUI_NB_RIGHT) {}

    // figure out the size of the tab
    wxSize tab_size = GetTabSize(dc, wnd, page.caption, page.bitmap,
                                    page.active, close_button_state, x_extent);

    wxRect tab_rect = in_rect;
    tab_rect.width = tab_size.x;
    tab_rect.height = tab_size.y;
    tab_rect.y += 2 * GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_hborder;

    if (page.active)
        tab_rect.height += 2 * GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_hborder;

    int gap_rect_height = 10 * GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_hborder;
    int gap_rect_x = 1, gap_start = 0, gap_width = 0;
    int gap_rect_y = tab_rect.y - gap_rect_height;
    int gap_rect_width = window_rect.width;

    switch (tab_pos)
    {
        case wxAUI_NB_TOP:
            tab_rect.y -= 2 * GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_hborder;
            if (!page.active)
                tab_rect.y += 2 * GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_hborder;
            gap_rect_y = tab_rect.y + tab_rect.height - GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_hborder / 2;
            // fall through
        case wxAUI_NB_BOTTOM:
            gap_start = tab_rect.x - GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_vborder / 2;
            gap_width = tab_rect.width;
            break;
        // TODO: case wxAUI_NB_LEFT: break;
        // TODO: case wxAUI_NB_RIGHT: break;
    }
    tab_rect.y += GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_hborder / 2;
    gap_rect_y += GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_hborder / 2;

    int padding = focus_width + GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_hborder;

    int clip_width = tab_rect.width;
    if (tab_rect.x + tab_rect.width > in_rect.x + in_rect.width)
        clip_width = (in_rect.x + in_rect.width) - tab_rect.x;

    dc.SetClippingRegion(tab_rect.x, tab_rect.y - GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_vborder, clip_width, tab_rect.height + GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_vborder);

    GdkRectangle area;
    area.x = tab_rect.x - GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_vborder;
    area.y = tab_rect.y - 2 * GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_hborder;
    area.width = clip_width + GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_vborder;
    area.height = tab_rect.height + 2 * GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_hborder;

    wxGTKDCImpl *impldc = (wxGTKDCImpl*) dc.GetImpl();
    GdkWindow* window = impldc->GetGDKWindow();

    // Before drawing the active tab itself, draw a box without border, because some themes
    // have transparent gaps and a line would be visible at the bottom of the tab
    if (page.active)
        gtk_paint_box(style_notebook, window, GTK_STATE_NORMAL, GTK_SHADOW_NONE,
                      NULL, widget,
                      const_cast<char*>("notebook"),
                      gap_rect_x, gap_rect_y,
                      gap_rect_width, gap_rect_height);

    if (tab_pos == wxAUI_NB_BOTTOM)
    {
        if (page.active)
        {
            gtk_paint_box_gap(style_notebook, window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                              NULL, widget,
                              const_cast<char*>("notebook"),
                              gap_rect_x, gap_rect_y,
                              gap_rect_width, gap_rect_height,
                              GTK_POS_BOTTOM, gap_start , gap_width);
        }
        gtk_paint_extension(style_notebook, window,
                           page.active ? GTK_STATE_NORMAL : GTK_STATE_ACTIVE, GTK_SHADOW_OUT,
                           &area, widget,
                           const_cast<char*>("tab"),
                           tab_rect.x, tab_rect.y,
                           tab_rect.width, tab_rect.height,
                           GTK_POS_TOP);
    }
    else
    {
        if (page.active)
        {
            gtk_paint_box_gap(style_notebook, window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                              NULL, widget,
                              const_cast<char*>("notebook"),
                              gap_rect_x, gap_rect_y,
                              gap_rect_width, gap_rect_height,
                              GTK_POS_TOP, gap_start , gap_width);
        }
        gtk_paint_extension(style_notebook, window,
                           page.active ? GTK_STATE_NORMAL : GTK_STATE_ACTIVE, GTK_SHADOW_OUT,
                           &area, widget,
                           const_cast<char*>("tab"),
                           tab_rect.x, tab_rect.y,
                           tab_rect.width, tab_rect.height,
                           GTK_POS_BOTTOM);
    }

    // After drawing the inactive tab itself, draw a box with the same dimensions as the gap-box,
    // otherwise we don't get a gap-box, if the active tab is invisible
    if (!page.active)
        gtk_paint_box(style_notebook, window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                      NULL, widget,
                      const_cast<char*>("notebook"),
                      gap_rect_x, gap_rect_y,
                      gap_rect_width, gap_rect_height);

    wxCoord textX = tab_rect.x + padding + style_notebook->xthickness;

    int bitmap_offset = 0;
    if (page.bitmap.IsOk())
    {
        bitmap_offset = textX;

        // draw bitmap
        int bitmapY = tab_rect.y +(tab_rect.height - page.bitmap.GetHeight()) / 2;
        if(!page.active)
        {
            if (tab_pos == wxAUI_NB_TOP)
                bitmapY += style_notebook->ythickness / 2;
            else
                bitmapY -= style_notebook->ythickness / 2;
        }
        dc.DrawBitmap(page.bitmap,
                      bitmap_offset,
                      bitmapY,
                      true);

        textX += page.bitmap.GetWidth() + padding;
    }

    wxCoord textW, textH, textY;

    dc.SetFont(m_normalFont);
    dc.GetTextExtent(page.caption, &textW, &textH);
    textY = tab_rect.y + (tab_rect.height - textH) / 2;
    if(!page.active)
    {
        if (tab_pos == wxAUI_NB_TOP)
            textY += style_notebook->ythickness / 2;
        else
            textY -= style_notebook->ythickness / 2;
    }

    // draw tab text
    GdkColor text_colour = page.active ? style_notebook->fg[GTK_STATE_NORMAL] : style_notebook->fg[GTK_STATE_ACTIVE];
    dc.SetTextForeground(wxColor(text_colour));
    GdkRectangle focus_area;

    int padding_focus = padding - focus_width;
    focus_area.x = tab_rect.x + padding_focus;
    focus_area.y = textY - focus_width;
    focus_area.width = tab_rect.width - 2 * padding_focus;
    focus_area.height = textH + 2 * focus_width;

    if(page.active && (wnd->FindFocus() == wnd) && focus_area.x <= (area.x + area.width))
    {
        // clipping seems not to work here, so we we have to recalc the focus-area manually
        if((focus_area.x + focus_area.width) > (area.x + area.width))
            focus_area.width = area.x + area.width - focus_area.x + focus_width - GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_vborder;
        gtk_paint_focus (style_notebook, window,
                         GTK_STATE_ACTIVE, NULL, widget, "tab",
                         focus_area.x, focus_area.y, focus_area.width, focus_area.height);
    }

    dc.DrawText(page.caption, textX, textY);

    // draw close-button on tab (if enabled)
    if (close_button_state != wxAUI_BUTTON_STATE_HIDDEN)
    {
        wxRect rect(tab_rect.x, tab_rect.y, tab_rect.width - style_notebook->xthickness, tab_rect.height);
        if(!page.active)
        {
            if (tab_pos == wxAUI_NB_TOP)
                rect.y += style_notebook->ythickness / 2;
            else
                rect.y -= style_notebook->ythickness / 2;
        }
        *out_button_rect = DrawCloseButton(dc, widget, close_button_state, rect, wxRIGHT, &area);
    }

    if ( clip_width < tab_rect.width )
        tab_rect.width = clip_width;
    *out_tab_rect = tab_rect;

    dc.DestroyClippingRegion();
}

wxRect DrawSimpleArrow(wxDC& dc,
                       GtkWidget *widget,
                       int button_state,
                       wxRect const &in_rect,
                       int orientation,
                       GtkArrowType arrow_type)
{
    int scroll_arrow_hlength, scroll_arrow_vlength;
    gtk_widget_style_get(widget,
                         "scroll-arrow-hlength", &scroll_arrow_hlength,
                         "scroll-arrow-vlength", &scroll_arrow_vlength,
                         NULL);

    GtkStateType state;
    GtkShadowType shadow;
    ButtonStateAndShadow(button_state, state, shadow);

    wxRect out_rect;

    if (orientation == wxLEFT)
        out_rect.x = in_rect.x;
    else
        out_rect.x = in_rect.x + in_rect.width - scroll_arrow_hlength;
    out_rect.y = (in_rect.y + in_rect.height - 3 * gtk_widget_get_style(wxGTKPrivate::GetNotebookWidget())->ythickness - scroll_arrow_vlength) / 2;
    out_rect.width = scroll_arrow_hlength;
    out_rect.height = scroll_arrow_vlength;

    wxGTKDCImpl *impldc = (wxGTKDCImpl*) dc.GetImpl();
    GdkWindow* window = impldc->GetGDKWindow();
    gtk_paint_arrow (gtk_widget_get_style(wxGTKPrivate::GetButtonWidget()), window, state, shadow, NULL, widget, "notebook",
                     arrow_type, TRUE, out_rect.x, out_rect.y, out_rect.width, out_rect.height);

    return out_rect;
}

void wxAuiGtkTabArt::DrawButton(wxDC& dc, wxWindow* wnd,
                            const wxRect& in_rect,
                            int bitmap_id,
                            int button_state,
                            int orientation,
                            wxRect* out_rect)
{
    GtkWidget *widget = wnd->GetHandle();
    wxRect rect = in_rect;
    if (m_flags &wxAUI_NB_BOTTOM)
        rect.y += 2 * gtk_widget_get_style(wxGTKPrivate::GetButtonWidget())->ythickness;

    switch (bitmap_id)
    {
        case wxAUI_BUTTON_CLOSE:
            rect.y -= 2 * gtk_widget_get_style(wxGTKPrivate::GetButtonWidget())->ythickness;
            rect = DrawCloseButton(dc, widget, button_state, rect, orientation, NULL);
            break;

        case wxAUI_BUTTON_LEFT:
            rect = DrawSimpleArrow(dc, widget, button_state, rect, orientation, GTK_ARROW_LEFT);
            break;

        case wxAUI_BUTTON_RIGHT:
            rect = DrawSimpleArrow(dc, widget, button_state, rect, orientation, GTK_ARROW_RIGHT);
            break;

        case wxAUI_BUTTON_WINDOWLIST:
            {
                rect.height -= 4 * gtk_widget_get_style(wxGTKPrivate::GetButtonWidget())->ythickness;
                rect.width = rect.height;
                rect.x = in_rect.x + in_rect.width - rect.width;

                if (button_state == wxAUI_BUTTON_STATE_HOVER)
                    wxRendererNative::Get().DrawComboBoxDropButton(wnd, dc, rect, wxCONTROL_CURRENT);
                else if (button_state == wxAUI_BUTTON_STATE_PRESSED)
                    wxRendererNative::Get().DrawComboBoxDropButton(wnd, dc, rect, wxCONTROL_PRESSED);
                else
                    wxRendererNative::Get().DrawDropArrow(wnd, dc, rect);
            }
            break;
    }

    *out_rect = rect;
}


int wxAuiGtkTabArt::GetBestTabCtrlSize(wxWindow* wnd,
                                   const wxAuiNotebookPageArray& pages,
                                   const wxSize& required_bmp_size)
{
    SetMeasuringFont(m_normalFont);
    SetSelectedFont(m_normalFont);
    int tab_height = 3 * gtk_widget_get_style(wxGTKPrivate::GetNotebookWidget())->ythickness + wxAuiGenericTabArt::GetBestTabCtrlSize(wnd, pages, required_bmp_size);
    return tab_height;
}

int wxAuiGtkTabArt::GetBorderWidth(wxWindow* wnd)
{
    return wxAuiGenericTabArt::GetBorderWidth(wnd) + wxMax(GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_hborder,
                                                           GTK_NOTEBOOK (wxGTKPrivate::GetNotebookWidget())->tab_vborder);
}

int wxAuiGtkTabArt::GetAdditionalBorderSpace(wxWindow* wnd)
{
    return 2 * GetBorderWidth(wnd);
}

wxSize wxAuiGtkTabArt::GetTabSize(wxDC& dc,
                              wxWindow* wnd,
                              const wxString& caption,
                              const wxBitmap& bitmap,
                              bool active,
                              int close_button_state,
                              int* x_extent)
{
    wxSize s = wxAuiGenericTabArt::GetTabSize(dc, wnd, caption, bitmap, active, close_button_state, x_extent);

    int overlap = 0;
    gtk_widget_style_get (wnd->GetHandle(),
        "focus-line-width", &overlap,
        NULL);
    *x_extent -= overlap;
    return s;
}
#endif  // wxUSE_AUI
