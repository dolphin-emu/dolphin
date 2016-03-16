/////////////////////////////////////////////////////////////////////////////
// Name:        wx/aui/tabartmsw.h
// Purpose:     wxAuiMSWTabArt declaration
// Author:      Tobias Taschner
// Created:     2015-09-26
// Copyright:   (c) 2015 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/dc.h"
#endif

#include "wx/aui/tabart.h"
#include "wx/aui/auibook.h"
#include "wx/msw/uxtheme.h"
#include "wx/msw/private.h"
#include "wx/renderer.h"

#if wxUSE_AUI

#ifndef CP_DROPDOWNBUTTON

    #define TABP_TABITEM 1
    #define TABP_TABITEMLEFTEDGE 2
    #define TABP_TABITEMRIGHTEDGE 3
    #define TABP_TABITEMBOTHEDGE 4
    #define TABP_TOPTABITEM 5
    #define TABP_TOPTABITEMLEFTEDGE 6
    #define TABP_TOPTABITEMRIGHTEDGE 7
    #define TABP_TOPTABITEMBOTHEDGE 8
    #define TABP_PANE 9
    #define TABP_BODY 10
    #define TABP_AEROWIZARDBODY 11

    #define TIS_NORMAL 1
    #define TIS_HOT 2
    #define TIS_SELECTED 3
    #define TIS_DISABLED 4
    #define TIS_FOCUSED 5

    #define TTP_CLOSE 5

    #define TTCS_NORMAL 1
    #define TTCS_HOT 2
    #define TTCS_PRESSED 3

    #define SPNP_UPHORZ 3
    #define SPNP_DOWNHORZ 4

    #define CP_DROPDOWNBUTTON1  1

    #define WP_CLOSEBUTTON 18
#endif

wxAuiMSWTabArt::wxAuiMSWTabArt()
{
    m_closeBtnSize = wxDefaultSize;
    m_maxTabHeight = 0;

    wxUxThemeEngine* te = wxUxThemeEngine::GetIfActive();
    if ( te && te->IsAppThemed() )
    {
        m_themed = true;
    }
    else
        m_themed = false;
}

wxAuiMSWTabArt::~wxAuiMSWTabArt()
{
}

wxAuiTabArt* wxAuiMSWTabArt::Clone()
{
    return new wxAuiMSWTabArt(*this);
}

void wxAuiMSWTabArt::SetSizingInfo(const wxSize& tab_ctrl_size,
    size_t tab_count)
{
    wxAuiGenericTabArt::SetSizingInfo(tab_ctrl_size, tab_count);
}


void wxAuiMSWTabArt::DrawBorder(wxDC& dc, wxWindow* wnd, const wxRect& rect)
{
    if ( !IsThemed() )
    {
        wxAuiGenericTabArt::DrawBorder(dc, wnd, rect);
        return;
    }

    wxRect drawRect(rect);

    drawRect.y += m_maxTabHeight + wnd->FromDIP(1);
    drawRect.height -= m_maxTabHeight;

    // Mask border not covered by native theme
    wxRect topDrawRect(rect);
    topDrawRect.height = drawRect.height;
    dc.SetPen(wxPen(wnd->GetBackgroundColour(), GetBorderWidth(wnd)));
    dc.DrawRectangle(topDrawRect);

    RECT r;
    wxCopyRectToRECT(drawRect, r);

    wxUxThemeHandle hTheme(wnd, L"TAB");

    wxUxThemeEngine::Get()->DrawThemeBackground(
        hTheme,
        GetHdcOf(dc.GetTempHDC()),
        TABP_PANE,
        0,
        &r,
        NULL);
}

void wxAuiMSWTabArt::DrawBackground(wxDC& dc,
    wxWindow* wnd,
    const wxRect& rect)
{
    if ( !IsThemed() )
    {
        wxAuiGenericTabArt::DrawBackground(dc, wnd, rect);
        return;
    }

    int borderHeight = 2;

    wxRect drawRect = rect;
    drawRect.height -= borderHeight;

    // Draw background
    dc.SetBrush(wxBrush(wnd->GetBackgroundColour()));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(drawRect);

    // Draw top border
    drawRect.y = drawRect.height;
    drawRect.height = borderHeight + 2;

    drawRect.Inflate(1, 0);

    RECT r;
    wxCopyRectToRECT(drawRect, r);

    wxUxThemeHandle hTheme(wnd, L"TAB");

    wxUxThemeEngine::Get()->DrawThemeBackground(
        hTheme,
        GetHdcOf(dc.GetTempHDC()),
        TABP_PANE,
        0,
        &r,
        NULL);
}

void wxAuiMSWTabArt::DrawTab(wxDC& dc,
    wxWindow* wnd,
    const wxAuiNotebookPage& page,
    const wxRect& in_rect,
    int close_button_state,
    wxRect* out_tab_rect,
    wxRect* out_button_rect,
    int* x_extent)
{
    if ( !IsThemed() )
    {
        wxAuiGenericTabArt::DrawTab(dc, wnd, page, in_rect, close_button_state, out_tab_rect, out_button_rect, x_extent);
        return;
    }

    if ( !m_closeBtnSize.IsFullySpecified() )
        InitSizes(wnd, dc);

    // figure out the size of the tab
    wxSize tabSize = GetTabSize(dc,
        wnd,
        page.caption,
        page.bitmap,
        page.active,
        close_button_state,
        x_extent);

    wxCoord tabHeight = tabSize.y;
    wxCoord tabWidth = tabSize.x;
    wxCoord tabX = in_rect.x;
    wxCoord tabY = 0;

    if (!page.active)
    {
        tabY += wnd->FromDIP(2);
        tabHeight -= wnd->FromDIP(2);
    }
    else
    {
        tabX -= wnd->FromDIP(2);
        tabWidth += wnd->FromDIP(4);
        tabHeight += 2;
    }

    int clipWidth = tabWidth;
    if ( tabX + clipWidth > in_rect.x + in_rect.width )
        clipWidth = (in_rect.x + in_rect.width) - tabX;
    dc.SetClippingRegion(tabX - wnd->FromDIP(2), tabY, clipWidth + wnd->FromDIP(4), tabHeight);


    // draw tab
    wxRect tabRect(tabX, tabY, tabWidth, tabHeight);

    int tabState;
    if ( page.active )
        tabState = TIS_SELECTED;
    else if ( page.hover )
        tabState = TIS_HOT;
    else
        tabState = TIS_NORMAL;

    wxUxThemeEngine* te = wxUxThemeEngine::Get();

    wxUxThemeHandle hTabTheme(wnd, L"Tab");
    RECT tabR;
    wxCopyRectToRECT(tabRect, tabR);
    te->DrawThemeBackground(hTabTheme, GetHdcOf(dc.GetTempHDC()), TABP_TABITEM,
        tabState,
        &tabR, NULL);

    wxRect textRect = tabRect;
    if ( !page.active )
        textRect.Offset(0, wnd->FromDIP(1));
    if ( close_button_state != wxAUI_BUTTON_STATE_HIDDEN )
        textRect.width -= m_closeBtnSize.x + wnd->FromDIP(3);

    dc.SetFont(wnd->GetFont());
    dc.DrawLabel(page.caption, page.bitmap, textRect, wxALIGN_CENTRE);

    // draw focus rectangle
    if ( page.active && (wnd->FindFocus() == wnd) )
    {
        wxRect focusRect = tabRect;
        focusRect.Deflate(wnd->FromDIP(2));

        wxRendererNative::Get().DrawFocusRect(wnd, dc, focusRect, 0);
    }

    // draw close button
    if ( close_button_state != wxAUI_BUTTON_STATE_HIDDEN )
    {
        wxUxThemeHandle hToolTipTheme(wnd, L"TOOLTIP");

        int btnState;
        if ( close_button_state == wxAUI_BUTTON_STATE_HOVER )
            btnState = TTCS_HOT;
        else if ( close_button_state == wxAUI_BUTTON_STATE_PRESSED )
            btnState = TTCS_PRESSED;
        else
            btnState = TTCS_NORMAL;

        int offsetY = tabY;
        if ( wxGetWinVersion() < wxWinVersion_Vista )
            offsetY++; // WinXP theme needs a little more padding

        wxRect rect(tabX + tabWidth - m_closeBtnSize.x - wnd->FromDIP(4),
            offsetY + (tabHeight / 2) - (m_closeBtnSize.y / 2),
            m_closeBtnSize.x,
            m_closeBtnSize.y);

        RECT btnR;
        wxCopyRectToRECT(rect, btnR);
        te->DrawThemeBackground(hToolTipTheme, GetHdcOf(dc.GetTempHDC()), TTP_CLOSE, btnState, &btnR, NULL);

        if ( out_button_rect )
            *out_button_rect = rect;
    }

    *out_tab_rect = wxRect(tabX, tabY, tabWidth, tabHeight);

    dc.DestroyClippingRegion();
}

int wxAuiMSWTabArt::GetIndentSize()
{
    if ( IsThemed() )
        return 3; // This should be 1 but we can't draw into the border from DrawTab
    else
        return wxAuiGenericTabArt::GetIndentSize();
}

int wxAuiMSWTabArt::GetBorderWidth(wxWindow* wnd)
{
    return wxAuiGenericTabArt::GetBorderWidth(wnd);
}

int wxAuiMSWTabArt::GetAdditionalBorderSpace(wxWindow* wnd)
{
    if ( IsThemed() )
    {
        return 4;
    }
    else
        return wxAuiGenericTabArt::GetAdditionalBorderSpace(wnd);
}

wxSize wxAuiMSWTabArt::GetTabSize(wxDC& dc,
    wxWindow* wnd,
    const wxString& caption,
    const wxBitmap& bitmap,
    bool active,
    int close_button_state,
    int* x_extent)
{
    if ( !IsThemed() )
        return wxAuiGenericTabArt::GetTabSize(dc, wnd, caption, bitmap, active, close_button_state, x_extent);

    if ( !m_closeBtnSize.IsFullySpecified() )
        InitSizes(wnd, dc);

    wxCoord textWidth, textHeight, tmp;

    dc.SetFont(wnd->GetFont());
    dc.GetTextExtent(caption, &textWidth, &tmp);
    dc.GetTextExtent("ABCDEFXj", &tmp, &textHeight);

    wxCoord tabWidth = wxMax(m_tabSize.x, textWidth);
    wxCoord tabHeight = wxMax(m_tabSize.y, textHeight);

    // if the close button is showing, add space for it
    if ( close_button_state != wxAUI_BUTTON_STATE_HIDDEN )
    {
        tabWidth += m_closeBtnSize.x;
        tabHeight = wxMax(tabHeight, m_closeBtnSize.y);
    }

    // if there's a bitmap, add space for it
    if ( bitmap.IsOk() )
    {
        tabWidth += bitmap.GetWidth() + wnd->FromDIP(3); // bitmap padding
        tabHeight = wxMax(tabHeight, bitmap.GetHeight() + wnd->FromDIP(2));
    }

    // add padding
    tabWidth += wnd->FromDIP(12);
    tabHeight += wnd->FromDIP(3);

    if ( m_flags & wxAUI_NB_TAB_FIXED_WIDTH )
    {
        tabWidth = m_fixedTabWidth;
    }
    else
    {
        int minTabWidth = wnd->FromDIP(46);
        if (tabWidth < minTabWidth)
            tabWidth = minTabWidth;
    }

    *x_extent = tabWidth;

    if (tabHeight > m_maxTabHeight)
        m_maxTabHeight = tabHeight;

    return wxSize(tabWidth, tabHeight);
}


void wxAuiMSWTabArt::DrawButton(wxDC& dc,
    wxWindow* wnd,
    const wxRect& in_rect,
    int bitmap_id,
    int button_state,
    int orientation,
    wxRect* out_rect)
{
    if ( !IsThemed() )
    {
        wxAuiGenericTabArt::DrawButton(dc, wnd, in_rect, bitmap_id, button_state, orientation, out_rect);
        return;
    }

    wxUxThemeEngine* te = wxUxThemeEngine::Get();

    const wchar_t* themeId = NULL;
    int part = 0;

    switch (bitmap_id)
    {
    case wxAUI_BUTTON_CLOSE:
        themeId = L"Window";
        part = WP_CLOSEBUTTON;
        break;
    case wxAUI_BUTTON_LEFT:
        themeId = L"Spin";
        part = SPNP_DOWNHORZ;
        break;
    case wxAUI_BUTTON_RIGHT:
        themeId = L"Spin";
        part = SPNP_UPHORZ;
        break;
    case wxAUI_BUTTON_WINDOWLIST:
        themeId = L"Combobox";
        part = CP_DROPDOWNBUTTON1;
        break;
    }

    wxRect rect = in_rect;

    if ( orientation == wxLEFT )
    {
        rect.SetX(in_rect.x);
        rect.SetY(((in_rect.y + in_rect.height) / 2) - (m_closeBtnSize.GetHeight() / 2));
        rect.SetWidth(m_closeBtnSize.GetWidth());
        rect.SetHeight(m_closeBtnSize.GetHeight());
    }
    else
    {
        rect = wxRect(in_rect.x + in_rect.width - m_closeBtnSize.GetWidth(),
            ((in_rect.y + in_rect.height) / 2) - (m_closeBtnSize.GetHeight() / 2),
            m_closeBtnSize.GetWidth(), m_closeBtnSize.GetHeight());
    }

    if ( bitmap_id == wxAUI_BUTTON_LEFT ||
        bitmap_id == wxAUI_BUTTON_RIGHT )
    {
        rect.y = in_rect.y;
        rect.height = in_rect.height - 7;
    }

    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(m_baseColour));
    dc.DrawRectangle(rect);

    int btnState;
    if ( button_state == wxAUI_BUTTON_STATE_DISABLED )
        btnState = TTCS_PRESSED + 1;
    else if ( button_state == wxAUI_BUTTON_STATE_HOVER )
        btnState = TTCS_HOT;
    else if ( button_state == wxAUI_BUTTON_STATE_PRESSED )
        btnState = TTCS_PRESSED;
    else
        btnState = TTCS_NORMAL;

    wxUxThemeHandle hTheme(wnd, themeId);

    wxRect btnRect(rect);
    btnRect.width -= wnd->FromDIP(1);

    RECT btnR;
    wxCopyRectToRECT(btnRect, btnR);
    te->DrawThemeBackground(hTheme, GetHdcOf(dc.GetTempHDC()), part, btnState, &btnR, NULL);

    if ( out_rect )
        *out_rect = rect;
}

int wxAuiMSWTabArt::ShowDropDown(wxWindow* wnd,
    const wxAuiNotebookPageArray& pages,
    int active_idx)
{
    return wxAuiGenericTabArt::ShowDropDown(wnd, pages, active_idx);
}

int wxAuiMSWTabArt::GetBestTabCtrlSize(wxWindow* wnd,
    const wxAuiNotebookPageArray& pages,
    const wxSize& requiredBmp_size)
{
    return wxAuiGenericTabArt::GetBestTabCtrlSize(wnd, pages, requiredBmp_size);
}

void wxAuiMSWTabArt::InitSizes(wxWindow* wnd, wxDC& dc)
{
    wxUxThemeEngine* te = wxUxThemeEngine::Get();
    SIZE uxSize;

    // Borrow close button from tooltip (best fit on various backgrounds)
    wxUxThemeHandle hTooltipTheme(wnd, L"Tooltip");

    te->GetThemePartSize(hTooltipTheme, GetHdcOf(dc.GetTempHDC()),
        TTP_CLOSE, 0, NULL, TS_TRUE, &uxSize);
    m_closeBtnSize.Set(uxSize.cx, uxSize.cy);

    wxUxThemeHandle hTabTheme(wnd, L"Tab");
    te->GetThemePartSize(hTabTheme, GetHdcOf(dc.GetTempHDC()),
        TABP_TABITEM, 0, NULL, TS_TRUE, &uxSize);
    m_tabSize.Set(uxSize.cx, uxSize.cy);
}

bool wxAuiMSWTabArt::IsThemed() const
{
    return
        m_themed &&
        !(m_flags & wxAUI_NB_BOTTOM); // Native theme does not support bottom tabs
}


#endif // wxUSE_AUI
