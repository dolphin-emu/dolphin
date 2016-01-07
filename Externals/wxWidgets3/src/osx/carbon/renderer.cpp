///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/renderer.cpp
// Purpose:     implementation of wxRendererNative for Mac
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.07.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxOSX_USE_COCOA_OR_CARBON

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/dc.h"
    #include "wx/bitmap.h"
    #include "wx/settings.h"
    #include "wx/dcclient.h"
    #include "wx/dcmemory.h"
    #include "wx/toplevel.h"
#endif

#include "wx/renderer.h"
#include "wx/graphics.h"
#include "wx/dcgraph.h"
#include "wx/splitter.h"
#include "wx/time.h"
#include "wx/osx/private.h"

#ifdef wxHAS_DRAW_TITLE_BAR_BITMAP
    #include "wx/image.h"
    #include "wx/mstream.h"
#endif // wxHAS_DRAW_TITLE_BAR_BITMAP


// check if we're having a CGContext we can draw into
inline bool wxHasCGContext(wxWindow* WXUNUSED(win), wxDC& dc)
{
    wxGCDCImpl* gcdc = wxDynamicCast( dc.GetImpl() , wxGCDCImpl);
    
    if ( gcdc )
    {
        if ( gcdc->GetGraphicsContext()->GetNativeContext() )
            return true;
    }
    return false;
}



class WXDLLEXPORT wxRendererMac : public wxDelegateRendererNative
{
public:
    // draw the header control button (used by wxListCtrl)
    virtual int DrawHeaderButton( wxWindow *win,
        wxDC& dc,
        const wxRect& rect,
        int flags = 0,
        wxHeaderSortIconType sortArrow = wxHDR_SORT_ICON_NONE,
        wxHeaderButtonParams* params = NULL ) wxOVERRIDE;

    virtual int GetHeaderButtonHeight(wxWindow *win) wxOVERRIDE;

    virtual int GetHeaderButtonMargin(wxWindow *win) wxOVERRIDE;

    // draw the expanded/collapsed icon for a tree control item
    virtual void DrawTreeItemButton( wxWindow *win,
        wxDC& dc,
        const wxRect& rect,
        int flags = 0 ) wxOVERRIDE;

    // draw a (vertical) sash
    virtual void DrawSplitterSash( wxWindow *win,
        wxDC& dc,
        const wxSize& size,
        wxCoord position,
        wxOrientation orient,
        int flags = 0 ) wxOVERRIDE;

    virtual void DrawCheckBox(wxWindow *win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0) wxOVERRIDE;

    virtual wxSize GetCheckBoxSize(wxWindow* win) wxOVERRIDE;

    virtual void DrawComboBoxDropButton(wxWindow *win,
                                        wxDC& dc,
                                        const wxRect& rect,
                                        int flags = 0) wxOVERRIDE;

    virtual void DrawPushButton(wxWindow *win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags = 0) wxOVERRIDE;

    virtual void DrawCollapseButton(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    int flags = 0);

    virtual wxSize GetCollapseButtonSize(wxWindow *win, wxDC& dc);

    virtual void DrawItemSelectionRect(wxWindow *win,
                                       wxDC& dc,
                                       const wxRect& rect,
                                       int flags = 0) wxOVERRIDE;

    virtual void DrawFocusRect(wxWindow* win, wxDC& dc, const wxRect& rect, int flags = 0) wxOVERRIDE;

    virtual void DrawChoice(wxWindow* win, wxDC& dc, const wxRect& rect, int flags=0) wxOVERRIDE;

    virtual void DrawComboBox(wxWindow* win, wxDC& dc, const wxRect& rect, int flags=0) wxOVERRIDE;

    virtual void DrawTextCtrl(wxWindow* win, wxDC& dc, const wxRect& rect, int flags=0) wxOVERRIDE;

    virtual void DrawRadioBitmap(wxWindow* win, wxDC& dc, const wxRect& rect, int flags=0) wxOVERRIDE;

#ifdef wxHAS_DRAW_TITLE_BAR_BITMAP
    virtual void DrawTitleBarBitmap(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    wxTitleBarButton button,
                                    int flags = 0) wxOVERRIDE;
#endif // wxHAS_DRAW_TITLE_BAR_BITMAP

    virtual void DrawGauge(wxWindow* win,
                           wxDC& dc,
                           const wxRect& rect,
                           int value,
                           int max,
                           int flags = 0);

    virtual wxSplitterRenderParams GetSplitterParams(const wxWindow *win) wxOVERRIDE;

private:
    void DrawMacThemeButton(wxWindow *win,
                            wxDC& dc,
                            const wxRect& rect,
                            int flags,
                            int kind,
                            int adornment);

    // the tree buttons
    wxBitmap m_bmpTreeExpanded;
    wxBitmap m_bmpTreeCollapsed;
};

// ============================================================================
// implementation
// ============================================================================

// static
wxRendererNative& wxRendererNative::GetDefault()
{
    static wxRendererMac s_rendererMac;

    return s_rendererMac;
}

int wxRendererMac::DrawHeaderButton( wxWindow *win,
    wxDC& dc,
    const wxRect& rect,
    int flags,
    wxHeaderSortIconType sortArrow,
    wxHeaderButtonParams* params )
{
    const wxCoord x = rect.x;
    const wxCoord y = rect.y;
    const wxCoord w = rect.width;
    const wxCoord h = rect.height;

    dc.SetBrush( *wxTRANSPARENT_BRUSH );

    HIRect headerRect = CGRectMake( x, y, w, h );
    if ( !wxHasCGContext(win, dc) )
    {
        win->Refresh( &rect );
    }
    else
    {
        CGContextRef cgContext;
        wxGCDCImpl *impl = (wxGCDCImpl*) dc.GetImpl();

        cgContext = (CGContextRef) impl->GetGraphicsContext()->GetNativeContext();

        {
            HIThemeButtonDrawInfo drawInfo;
            HIRect labelRect;

            memset( &drawInfo, 0, sizeof(drawInfo) );
            drawInfo.version = 0;
            drawInfo.kind = kThemeListHeaderButton;
            drawInfo.state = (flags & wxCONTROL_DISABLED) ? kThemeStateInactive : kThemeStateActive;
            drawInfo.value = (flags & wxCONTROL_PRESSED) ? kThemeButtonOn : kThemeButtonOff;
            drawInfo.adornment = kThemeAdornmentNone;

            // The down arrow is drawn automatically, change it to an up arrow if needed.
            if ( sortArrow == wxHDR_SORT_ICON_UP )
                drawInfo.adornment = kThemeAdornmentHeaderButtonSortUp;

            HIThemeDrawButton( &headerRect, &drawInfo, cgContext, kHIThemeOrientationNormal, &labelRect );

            // If we don't want any arrows we need to draw over the one already there
            if ( (flags & wxCONTROL_PRESSED) && (sortArrow == wxHDR_SORT_ICON_NONE) )
            {
                // clip to the header rectangle
                CGContextSaveGState( cgContext );
                CGContextClipToRect( cgContext, headerRect );
                // but draw bigger than that so the arrow will get clipped off
                headerRect.size.width += 25;
                HIThemeDrawButton( &headerRect, &drawInfo, cgContext, kHIThemeOrientationNormal, &labelRect );
                CGContextRestoreGState( cgContext );
            }
        }
    }

    // Reserve room for the arrows before writing the label, and turn off the
    // flags we've already handled
    wxRect newRect(rect);
    if ( (flags & wxCONTROL_PRESSED) && (sortArrow != wxHDR_SORT_ICON_NONE) )
    {
        newRect.width -= 12;
        sortArrow = wxHDR_SORT_ICON_NONE;
    }
    flags &= ~wxCONTROL_PRESSED;

    return DrawHeaderButtonContents(win, dc, newRect, flags, sortArrow, params);
}


int wxRendererMac::GetHeaderButtonHeight(wxWindow* WXUNUSED(win))
{
    SInt32      standardHeight;
    OSStatus        errStatus;

    errStatus = GetThemeMetric( kThemeMetricListHeaderHeight, &standardHeight );
    if (errStatus == noErr)
    {
        return standardHeight;
    }
    return -1;
}

int wxRendererMac::GetHeaderButtonMargin(wxWindow *WXUNUSED(win))
{
    wxFAIL_MSG( "GetHeaderButtonMargin() not implemented" );
    return -1;
}

void wxRendererMac::DrawTreeItemButton( wxWindow *win,
    wxDC& dc,
    const wxRect& rect,
    int flags )
{
    // now the wxGCDC is using native transformations
    const wxCoord x = rect.x;
    const wxCoord y = rect.y;
    const wxCoord w = rect.width;
    const wxCoord h = rect.height;

    dc.SetBrush( *wxTRANSPARENT_BRUSH );

    HIRect headerRect = CGRectMake( x, y, w, h );
    if ( !wxHasCGContext(win, dc) )
    {
        win->Refresh( &rect );
    }
    else
    {
        CGContextRef cgContext;

        wxGCDCImpl *impl = (wxGCDCImpl*) dc.GetImpl();
        cgContext = (CGContextRef) impl->GetGraphicsContext()->GetNativeContext();

        HIThemeButtonDrawInfo drawInfo;
        HIRect labelRect;

        memset( &drawInfo, 0, sizeof(drawInfo) );
        drawInfo.version = 0;
        drawInfo.kind = kThemeDisclosureButton;
        drawInfo.state = (flags & wxCONTROL_DISABLED) ? kThemeStateInactive : kThemeStateActive;
        // Apple mailing list posts say to use the arrow adornment constants, but those don't work.
        // We need to set the value using the 'old' DrawThemeButton constants instead.
        drawInfo.value = (flags & wxCONTROL_EXPANDED) ? kThemeDisclosureDown : kThemeDisclosureRight;
        drawInfo.adornment = kThemeAdornmentNone;

        HIThemeDrawButton( &headerRect, &drawInfo, cgContext, kHIThemeOrientationNormal, &labelRect );
    }
}

wxSplitterRenderParams
wxRendererMac::GetSplitterParams(const wxWindow *win)
{
    // see below
    SInt32 sashWidth,
            border;
#if wxOSX_USE_COCOA
    if ( win->HasFlag(wxSP_3DSASH) )
        GetThemeMetric( kThemeMetricPaneSplitterHeight, &sashWidth ); // Cocoa == Carbon == 7
    else if ( win->HasFlag(wxSP_NOSASH) ) // actually Cocoa doesn't allow 0
        sashWidth = 0;
    else // no 3D effect - Cocoa [NSSplitView dividerThickNess] for NSSplitViewDividerStyleThin
        sashWidth = 1;
#else // Carbon
    if ( win->HasFlag(wxSP_3DSASH) )
        GetThemeMetric( kThemeMetricPaneSplitterHeight, &sashWidth );
    else if ( win->HasFlag(wxSP_NOSASH) )
        sashWidth = 0;
    else // no 3D effect
        GetThemeMetric( kThemeMetricSmallPaneSplitterHeight, &sashWidth );
#endif // Cocoa/Carbon

    if ( win->HasFlag(wxSP_3DBORDER) )
        border = 2;
    else // no 3D effect
        border = 0;

    return wxSplitterRenderParams(sashWidth, border, false);
}


void wxRendererMac::DrawSplitterSash( wxWindow *win,
    wxDC& dc,
    const wxSize& size,
    wxCoord position,
    wxOrientation orient,
    int WXUNUSED(flags) )
{
    bool hasMetal = win->MacGetTopLevelWindow()->GetExtraStyle() & wxFRAME_EX_METAL;
    SInt32 height;

    height = wxRendererNative::Get().GetSplitterParams(win).widthSash;

    HIRect splitterRect;
    if (orient == wxVERTICAL)
        splitterRect = CGRectMake( position, 0, height, size.y );
    else
        splitterRect = CGRectMake( 0, position, size.x, height );

    // under compositing we should only draw when called by the OS, otherwise just issue a redraw command
    // strange redraw errors occur if we don't do this

    if ( !wxHasCGContext(win, dc) )
    {
        wxRect rect( (int) splitterRect.origin.x, (int) splitterRect.origin.y, (int) splitterRect.size.width,
                     (int) splitterRect.size.height );
        win->RefreshRect( rect );
    }
    else
    {
        CGContextRef cgContext;
        wxGCDCImpl *impl = (wxGCDCImpl*) dc.GetImpl();
        cgContext = (CGContextRef) impl->GetGraphicsContext()->GetNativeContext();

        HIThemeBackgroundDrawInfo bgdrawInfo;
        bgdrawInfo.version = 0;
        bgdrawInfo.state = kThemeStateActive;
        bgdrawInfo.kind = hasMetal ? kThemeBackgroundMetal : kThemeBackgroundPlacard;

        if ( hasMetal )
            HIThemeDrawBackground(&splitterRect, &bgdrawInfo, cgContext, kHIThemeOrientationNormal);
        else
        {
            CGContextSetFillColorWithColor(cgContext,win->GetBackgroundColour().GetCGColor());
            CGContextFillRect(cgContext,splitterRect);
        }

        if ( win->HasFlag(wxSP_3DSASH) )
        {
            HIThemeSplitterDrawInfo drawInfo;
            drawInfo.version = 0;
            drawInfo.state = kThemeStateActive;
            drawInfo.adornment = hasMetal ? kHIThemeSplitterAdornmentMetal : kHIThemeSplitterAdornmentNone;
            HIThemeDrawPaneSplitter( &splitterRect, &drawInfo, cgContext, kHIThemeOrientationNormal );
        }
    }
}

void
wxRendererMac::DrawItemSelectionRect(wxWindow * WXUNUSED(win),
                                     wxDC& dc,
                                     const wxRect& rect,
                                     int flags)
{
    if ( !(flags & wxCONTROL_SELECTED) )
        return;

    wxColour col( wxMacCreateCGColorFromHITheme( (flags & wxCONTROL_FOCUSED) ?
                                                 kThemeBrushAlternatePrimaryHighlightColor
                                                                             : kThemeBrushSecondaryHighlightColor ) );
    wxBrush selBrush( col );

    dc.SetPen( *wxTRANSPARENT_PEN );
    dc.SetBrush( selBrush );
    dc.DrawRectangle( rect );
}


void
wxRendererMac::DrawMacThemeButton(wxWindow *win,
                                  wxDC& dc,
                                  const wxRect& rect,
                                  int flags,
                                  int kind,
                                  int adornment)
{
    // now the wxGCDC is using native transformations
    const wxCoord x = rect.x;
    const wxCoord y = rect.y;
    const wxCoord w = rect.width;
    const wxCoord h = rect.height;

    dc.SetBrush( *wxTRANSPARENT_BRUSH );

    HIRect headerRect = CGRectMake( x, y, w, h );
    if ( !wxHasCGContext(win, dc) )
    {
        win->Refresh( &rect );
    }
    else
    {
        wxGCDCImpl *impl = (wxGCDCImpl*) dc.GetImpl();
        CGContextRef cgContext;
        cgContext = (CGContextRef) impl->GetGraphicsContext()->GetNativeContext();

        HIThemeButtonDrawInfo drawInfo;
        HIRect labelRect;

        memset( &drawInfo, 0, sizeof(drawInfo) );
        drawInfo.version = 0;
        drawInfo.kind = kind;
        drawInfo.state = (flags & wxCONTROL_DISABLED) ? kThemeStateInactive : kThemeStateActive;
        drawInfo.value = (flags & wxCONTROL_PRESSED) ? kThemeButtonOn : kThemeButtonOff;
        if (flags & wxCONTROL_UNDETERMINED)
            drawInfo.value = kThemeButtonMixed;
        drawInfo.adornment = adornment;
        if (flags & wxCONTROL_FOCUSED)
            drawInfo.adornment |= kThemeAdornmentFocus;

        HIThemeDrawButton( &headerRect, &drawInfo, cgContext, kHIThemeOrientationNormal, &labelRect );
    }
}

void
wxRendererMac::DrawCheckBox(wxWindow *win,
                            wxDC& dc,
                            const wxRect& rect,
                            int flags)
{
    if (flags & wxCONTROL_CHECKED)
        flags |= wxCONTROL_PRESSED;

    int kind;

    if (win->GetWindowVariant() == wxWINDOW_VARIANT_SMALL ||
        (win->GetParent() && win->GetParent()->GetWindowVariant() == wxWINDOW_VARIANT_SMALL))
        kind = kThemeCheckBoxSmall;
    else if (win->GetWindowVariant() == wxWINDOW_VARIANT_MINI ||
             (win->GetParent() && win->GetParent()->GetWindowVariant() == wxWINDOW_VARIANT_MINI))
        kind = kThemeCheckBoxMini;
    else
        kind = kThemeCheckBox;


    DrawMacThemeButton(win, dc, rect, flags,
                       kind, kThemeAdornmentNone);
}

wxSize wxRendererMac::GetCheckBoxSize(wxWindow* WXUNUSED(win))
{
    wxSize size;
    SInt32 width, height;
    OSStatus errStatus;

    errStatus = GetThemeMetric(kThemeMetricCheckBoxWidth, &width);
    if (errStatus == noErr)
    {
        size.SetWidth(width);
    }

    errStatus = GetThemeMetric(kThemeMetricCheckBoxHeight, &height);
    if (errStatus == noErr)
    {
        size.SetHeight(height);
    }

    return size;
}

void
wxRendererMac::DrawComboBoxDropButton(wxWindow *win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags)
{
    int kind;
    if (win->GetWindowVariant() == wxWINDOW_VARIANT_SMALL || (win->GetParent() && win->GetParent()->GetWindowVariant() == wxWINDOW_VARIANT_SMALL))
        kind = kThemeArrowButtonSmall;
    else if (win->GetWindowVariant() == wxWINDOW_VARIANT_MINI || (win->GetParent() && win->GetParent()->GetWindowVariant() == wxWINDOW_VARIANT_MINI))
        kind = kThemeArrowButtonMini;
    else
        kind = kThemeArrowButton;

    DrawMacThemeButton(win, dc, rect, flags,
                       kind, kThemeAdornmentArrowDownArrow);
}

void
wxRendererMac::DrawPushButton(wxWindow *win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags)
{
    int kind;
    if (win->GetWindowVariant() == wxWINDOW_VARIANT_SMALL || (win->GetParent() && win->GetParent()->GetWindowVariant() == wxWINDOW_VARIANT_SMALL))
        kind = kThemeBevelButtonSmall;
    // There is no kThemeBevelButtonMini, but in this case, use Small
    else if (win->GetWindowVariant() == wxWINDOW_VARIANT_MINI || (win->GetParent() && win->GetParent()->GetWindowVariant() == wxWINDOW_VARIANT_MINI))
        kind = kThemeBevelButtonSmall;
    else
        kind = kThemeBevelButton;

    DrawMacThemeButton(win, dc, rect, flags,
                       kind, kThemeAdornmentNone);
}

void wxRendererMac::DrawCollapseButton(wxWindow *win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags)
{
    int adornment = (flags & wxCONTROL_EXPANDED)
        ? kThemeAdornmentArrowUpArrow
        : kThemeAdornmentArrowDownArrow;

    DrawMacThemeButton(win, dc, rect, flags,
                       kThemeArrowButton, adornment);
}

wxSize wxRendererMac::GetCollapseButtonSize(wxWindow *WXUNUSED(win), wxDC& WXUNUSED(dc))
{
    wxSize size;
    SInt32 width, height;
    OSStatus errStatus;

    errStatus = GetThemeMetric(kThemeMetricDisclosureButtonWidth, &width);
    if (errStatus == noErr)
    {
        size.SetWidth(width);
    }

    errStatus = GetThemeMetric(kThemeMetricDisclosureButtonHeight, &height);
    if (errStatus == noErr)
    {
        size.SetHeight(height);
    }

    // strict metrics size cutoff the button, increase the size
    size.IncBy(1);

    return size;
}

void
wxRendererMac::DrawFocusRect(wxWindow* win, wxDC& dc, const wxRect& rect, int flags)
{
    if (!win)
    {
        wxDelegateRendererNative::DrawFocusRect(win, dc, rect, flags);
        return;
    }

    CGRect cgrect = CGRectMake( rect.x , rect.y , rect.width, rect.height ) ;

    HIThemeFrameDrawInfo info ;

    memset( &info, 0 , sizeof(info) ) ;

    info.version = 0 ;
    info.kind = 0 ;
    info.state = kThemeStateActive;
    info.isFocused = true ;

    CGContextRef cgContext = (CGContextRef) win->MacGetCGContextRef() ;
    wxASSERT( cgContext ) ;

    HIThemeDrawFocusRect( &cgrect , true , cgContext , kHIThemeOrientationNormal ) ;
}

void wxRendererMac::DrawChoice(wxWindow* win, wxDC& dc,
                           const wxRect& rect, int flags)
{
    int kind;

    if (win->GetWindowVariant() == wxWINDOW_VARIANT_SMALL ||
        (win->GetParent() && win->GetParent()->GetWindowVariant() == wxWINDOW_VARIANT_SMALL))
        kind = kThemePopupButtonSmall;
    else if (win->GetWindowVariant() == wxWINDOW_VARIANT_MINI ||
             (win->GetParent() && win->GetParent()->GetWindowVariant() == wxWINDOW_VARIANT_MINI))
        kind = kThemePopupButtonMini;
    else
        kind = kThemePopupButton;

    DrawMacThemeButton(win, dc, rect, flags, kind, kThemeAdornmentNone);
}


void wxRendererMac::DrawComboBox(wxWindow* win, wxDC& dc,
                             const wxRect& rect, int flags)
{
    int kind;

    if (win->GetWindowVariant() == wxWINDOW_VARIANT_SMALL ||
        (win->GetParent() && win->GetParent()->GetWindowVariant() == wxWINDOW_VARIANT_SMALL))
        kind = kThemeComboBoxSmall;
    else if (win->GetWindowVariant() == wxWINDOW_VARIANT_MINI ||
             (win->GetParent() && win->GetParent()->GetWindowVariant() == wxWINDOW_VARIANT_MINI))
        kind = kThemeComboBoxMini;
    else
        kind = kThemeComboBox;

    DrawMacThemeButton(win, dc, rect, flags, kind, kThemeAdornmentNone);
}

void wxRendererMac::DrawRadioBitmap(wxWindow* win, wxDC& dc,
                                const wxRect& rect, int flags)
{
    int kind;

    if (win->GetWindowVariant() == wxWINDOW_VARIANT_SMALL ||
        (win->GetParent() && win->GetParent()->GetWindowVariant() == wxWINDOW_VARIANT_SMALL))
        kind = kThemeRadioButtonSmall;
    else if (win->GetWindowVariant() == wxWINDOW_VARIANT_MINI ||
             (win->GetParent() && win->GetParent()->GetWindowVariant() == wxWINDOW_VARIANT_MINI))
        kind = kThemeRadioButtonMini;
    else
        kind = kThemeRadioButton;

    if (flags & wxCONTROL_CHECKED)
        flags |= wxCONTROL_PRESSED;

    DrawMacThemeButton(win, dc, rect, flags,
                          kind, kThemeAdornmentNone);
}

void wxRendererMac::DrawTextCtrl(wxWindow* win, wxDC& dc,
                             const wxRect& rect, int flags)
{
    const wxCoord x = rect.x;
    const wxCoord y = rect.y;
    const wxCoord w = rect.width;
    const wxCoord h = rect.height;

    dc.SetBrush( *wxWHITE_BRUSH );
    dc.SetPen( *wxTRANSPARENT_PEN );
    dc.DrawRectangle(rect);

    dc.SetBrush( *wxTRANSPARENT_BRUSH );

    HIRect hiRect = CGRectMake( x, y, w, h );
    if ( !wxHasCGContext(win, dc) )
    {
        win->Refresh( &rect );
    }
    else
    {
        CGContextRef cgContext;

        cgContext = (CGContextRef) static_cast<wxGCDCImpl*>(dc.GetImpl())->GetGraphicsContext()->GetNativeContext();

        {
            HIThemeFrameDrawInfo drawInfo;

            memset( &drawInfo, 0, sizeof(drawInfo) );
            drawInfo.version = 0;
            drawInfo.kind = kHIThemeFrameTextFieldSquare;
            drawInfo.state = (flags & wxCONTROL_DISABLED) ? kThemeStateInactive : kThemeStateActive;
            if (flags & wxCONTROL_FOCUSED)
                drawInfo.isFocused = true;

            HIThemeDrawFrame( &hiRect, &drawInfo, cgContext, kHIThemeOrientationNormal);
        }
    }
}

#ifdef wxHAS_DRAW_TITLE_BAR_BITMAP

void wxRendererMac::DrawTitleBarBitmap(wxWindow *win,
                                       wxDC& dc,
                                       const wxRect& rect,
                                       wxTitleBarButton button,
                                       int flags)
{
    // the files below were converted from the originals in art/osx/close*.png
    // using misc/scripts/png2c.py script -- we must use PNG and not XPM for
    // them because they use transparency and don't look right without it

    /* close.png - 400 bytes */
    static const unsigned char close_png[] = {
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
      0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
      0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0e,
      0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x48, 0x2d,
      0xd1, 0x00, 0x00, 0x00, 0x06, 0x62, 0x4b, 0x47,
      0x44, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0xa0,
      0xbd, 0xa7, 0x93, 0x00, 0x00, 0x00, 0x09, 0x70,
      0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x12, 0x00,
      0x00, 0x0b, 0x12, 0x01, 0xd2, 0xdd, 0x7e, 0xfc,
      0x00, 0x00, 0x00, 0x09, 0x76, 0x70, 0x41, 0x67,
      0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0e,
      0x00, 0xb1, 0x5b, 0xf1, 0xf7, 0x00, 0x00, 0x00,
      0xef, 0x49, 0x44, 0x41, 0x54, 0x28, 0xcf, 0xa5,
      0xd2, 0x31, 0x4a, 0x03, 0x41, 0x18, 0xc5, 0xf1,
      0xdf, 0xc6, 0x55, 0x02, 0xb2, 0x18, 0xb0, 0x33,
      0x8d, 0xd8, 0x6e, 0x8a, 0x14, 0xa6, 0xb2, 0xc9,
      0x21, 0x72, 0x0b, 0xdb, 0x5c, 0x45, 0x98, 0x3b,
      0x24, 0xc7, 0x99, 0x6d, 0x2d, 0x04, 0xd3, 0x09,
      0x42, 0x48, 0x65, 0x60, 0x6c, 0x76, 0x65, 0x1c,
      0x14, 0x45, 0x5f, 0x35, 0xfc, 0xe7, 0x3d, 0xe6,
      0x9b, 0x8f, 0x57, 0xf9, 0xac, 0x06, 0x97, 0x98,
      0xe0, 0xbc, 0x67, 0x07, 0xbc, 0xe2, 0x05, 0xfb,
      0xc1, 0x58, 0x65, 0xa1, 0x2b, 0x4c, 0xb3, 0x40,
      0xa9, 0x03, 0x9e, 0xb1, 0x83, 0x93, 0x2c, 0x74,
      0x83, 0xb1, 0xef, 0x75, 0x86, 0x0b, 0x1c, 0xb1,
      0x1f, 0xf5, 0xe3, 0x4d, 0x51, 0x43, 0x08, 0xe1,
      0xb6, 0x4c, 0x64, 0xac, 0xee, 0xbd, 0x0d, 0x5c,
      0x63, 0x89, 0x65, 0x08, 0x61, 0x9d, 0x52, 0x4a,
      0x31, 0xc6, 0xcd, 0xc0, 0x62, 0x8c, 0x9b, 0x94,
      0x52, 0x0a, 0x21, 0xac, 0x07, 0xd6, 0x67, 0xcc,
      0x33, 0xf0, 0x61, 0x8c, 0x31, 0x6e, 0xf2, 0x73,
      0xee, 0xc1, 0xbc, 0xc2, 0x1d, 0x4e, 0xf3, 0xd1,
      0x62, 0x8c, 0xf7, 0x6d, 0xdb, 0xae, 0xa0, 0xeb,
      0xba, 0xed, 0x6c, 0x36, 0x7b, 0x28, 0xa6, 0x7f,
      0x1b, 0xf9, 0xa3, 0xea, 0x7e, 0xcd, 0x93, 0xf2,
      0xb5, 0xae, 0xeb, 0xb6, 0xd0, 0xb6, 0xed, 0x2a,
      0xc6, 0xa8, 0x78, 0xf5, 0xf0, 0xaf, 0xe5, 0x34,
      0x58, 0xe4, 0xe1, 0x62, 0x11, 0x25, 0x5b, 0xa0,
      0x19, 0x9a, 0x33, 0x14, 0xa0, 0xfe, 0xe1, 0x6b,
      0x47, 0x3c, 0x62, 0x37, 0x34, 0x67, 0xdf, 0xc3,
      0x71, 0xdf, 0x90, 0xaf, 0x74, 0xc0, 0x93, 0xbe,
      0x72, 0x55, 0x71, 0xf9, 0xeb, 0x92, 0xbf, 0x03,
      0x70, 0x33, 0x76, 0x58, 0xe5, 0x41, 0xfb, 0x6d,
      0x00, 0x00, 0x00, 0x20, 0x7a, 0x54, 0x58, 0x74,
      0x74, 0x69, 0x66, 0x66, 0x3a, 0x72, 0x6f, 0x77,
      0x73, 0x2d, 0x70, 0x65, 0x72, 0x2d, 0x73, 0x74,
      0x72, 0x69, 0x70, 0x00, 0x00, 0x78, 0xda, 0x33,
      0xb5, 0x30, 0x05, 0x00, 0x01, 0x47, 0x00, 0xa3,
      0x38, 0xda, 0x77, 0x3b, 0x00, 0x00, 0x00, 0x00,
      0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
    };

    /* close_current.png - 421 bytes */
    static const unsigned char close_current_png[] = {
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
      0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
      0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0e,
      0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x48, 0x2d,
      0xd1, 0x00, 0x00, 0x00, 0x06, 0x62, 0x4b, 0x47,
      0x44, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0xa0,
      0xbd, 0xa7, 0x93, 0x00, 0x00, 0x00, 0x09, 0x70,
      0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x12, 0x00,
      0x00, 0x0b, 0x12, 0x01, 0xd2, 0xdd, 0x7e, 0xfc,
      0x00, 0x00, 0x00, 0x09, 0x76, 0x70, 0x41, 0x67,
      0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0e,
      0x00, 0xb1, 0x5b, 0xf1, 0xf7, 0x00, 0x00, 0x01,
      0x04, 0x49, 0x44, 0x41, 0x54, 0x28, 0xcf, 0x9d,
      0xd2, 0xbd, 0x4a, 0x43, 0x31, 0x00, 0xc5, 0xf1,
      0xdf, 0x8d, 0x56, 0x44, 0x04, 0x45, 0xd0, 0xc5,
      0x55, 0xdc, 0x0a, 0x8e, 0xed, 0xec, 0xe4, 0xec,
      0x24, 0xd4, 0x67, 0x29, 0x59, 0xfa, 0x16, 0x9d,
      0x7c, 0x07, 0x1d, 0x5c, 0xba, 0xa5, 0xa3, 0x28,
      0x22, 0x94, 0xae, 0x2e, 0x1d, 0x4a, 0x5d, 0x55,
      0xb8, 0x2e, 0x09, 0x5c, 0x4b, 0xfd, 0xa0, 0x67,
      0x0a, 0x27, 0x39, 0xc9, 0x49, 0xf2, 0xaf, 0x7c,
      0xd7, 0x01, 0x8e, 0x71, 0x84, 0xfd, 0xec, 0x2d,
      0x30, 0xc3, 0x2b, 0xe6, 0x65, 0x61, 0xd5, 0x08,
      0x9d, 0xe0, 0x14, 0x7b, 0x56, 0xeb, 0x0d, 0x13,
      0x4c, 0x61, 0xa3, 0x11, 0x3a, 0xc3, 0x8e, 0x9f,
      0xb5, 0x9d, 0x9b, 0xbc, 0x63, 0x1e, 0x72, 0xbd,
      0x53, 0xb4, 0x20, 0xc6, 0xd8, 0x5e, 0x4e, 0x34,
      0xbc, 0x56, 0x5e, 0x7b, 0x00, 0x6d, 0x5c, 0xe1,
      0x2a, 0xc6, 0x38, 0xa8, 0xeb, 0xba, 0x4e, 0x29,
      0xdd, 0x16, 0x2f, 0xa5, 0x74, 0x5b, 0xd7, 0x75,
      0x1d, 0x63, 0x1c, 0x14, 0x0f, 0xed, 0x0a, 0xe7,
      0xb9, 0x02, 0x48, 0x29, 0x5d, 0x77, 0x3a, 0x9d,
      0x8b, 0xf1, 0x78, 0x7c, 0x07, 0x65, 0xdc, 0xed,
      0x76, 0x6f, 0x1a, 0x25, 0x66, 0x15, 0x2e, 0xb1,
      0xd5, 0xac, 0x56, 0xc2, 0xb0, 0x22, 0x04, 0xef,
      0xc1, 0x9a, 0xda, 0xcc, 0xff, 0xf4, 0x6b, 0xd5,
      0x94, 0x92, 0xa5, 0x53, 0x17, 0x6b, 0x3f, 0xce,
      0x06, 0x3e, 0x71, 0x88, 0xed, 0xd1, 0x68, 0x34,
      0x0b, 0x21, 0x4c, 0x7a, 0xbd, 0xde, 0x7d, 0xd9,
      0x7a, 0x38, 0x1c, 0x3e, 0x86, 0x10, 0x26, 0xfd,
      0x7e, 0xff, 0xa9, 0x01, 0xc2, 0x4b, 0x21, 0xa7,
      0x00, 0xd0, 0xfa, 0xe3, 0x6a, 0x1f, 0x78, 0xc0,
      0xb4, 0x90, 0x33, 0xcf, 0x44, 0xec, 0x66, 0x42,
      0x56, 0xe9, 0x0d, 0xcf, 0x32, 0x72, 0xd5, 0xd2,
      0xe4, 0xbf, 0x21, 0xff, 0x02, 0x4d, 0xb5, 0x74,
      0x79, 0x60, 0x9f, 0x78, 0x78, 0x00, 0x00, 0x00,
      0x20, 0x7a, 0x54, 0x58, 0x74, 0x74, 0x69, 0x66,
      0x66, 0x3a, 0x72, 0x6f, 0x77, 0x73, 0x2d, 0x70,
      0x65, 0x72, 0x2d, 0x73, 0x74, 0x72, 0x69, 0x70,
      0x00, 0x00, 0x78, 0xda, 0x33, 0xb5, 0x30, 0x05,
      0x00, 0x01, 0x47, 0x00, 0xa3, 0x38, 0xda, 0x77,
      0x3b, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
      0x44, 0xae, 0x42, 0x60, 0x82};

    /* close_pressed.png - 458 bytes */
    static const unsigned char close_pressed_png[] = {
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
      0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
      0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0e,
      0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x48, 0x2d,
      0xd1, 0x00, 0x00, 0x00, 0x06, 0x62, 0x4b, 0x47,
      0x44, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0xa0,
      0xbd, 0xa7, 0x93, 0x00, 0x00, 0x00, 0x09, 0x70,
      0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x12, 0x00,
      0x00, 0x0b, 0x12, 0x01, 0xd2, 0xdd, 0x7e, 0xfc,
      0x00, 0x00, 0x00, 0x09, 0x76, 0x70, 0x41, 0x67,
      0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0e,
      0x00, 0xb1, 0x5b, 0xf1, 0xf7, 0x00, 0x00, 0x01,
      0x29, 0x49, 0x44, 0x41, 0x54, 0x28, 0xcf, 0x9d,
      0x92, 0x31, 0x6a, 0xc3, 0x40, 0x14, 0x44, 0x9f,
      0x2c, 0xe1, 0xc2, 0x10, 0x08, 0xc4, 0xe0, 0x22,
      0x76, 0x21, 0x04, 0x29, 0x13, 0xf0, 0x05, 0x54,
      0xa5, 0x8a, 0x0f, 0xb2, 0x8d, 0x0e, 0xa0, 0x23,
      0xa8, 0x56, 0xa3, 0x03, 0xe8, 0x08, 0xba, 0x80,
      0xba, 0xb0, 0x45, 0x0a, 0x97, 0x02, 0x23, 0x58,
      0x83, 0x40, 0x60, 0x61, 0x08, 0x11, 0x04, 0x57,
      0x69, 0xf6, 0xc3, 0x46, 0x38, 0x24, 0x64, 0xca,
      0xd9, 0x19, 0xe6, 0xff, 0xfd, 0xe3, 0xf1, 0x1d,
      0x2b, 0x20, 0x02, 0x36, 0xc0, 0xd2, 0x72, 0x27,
      0xe0, 0x08, 0x1c, 0x80, 0x5e, 0x84, 0x9e, 0x63,
      0x7a, 0x04, 0xb6, 0xc0, 0x1d, 0xd7, 0x31, 0x00,
      0x6f, 0xc0, 0x1e, 0xc0, 0x77, 0x4c, 0x31, 0x70,
      0xc3, 0xcf, 0x58, 0xd8, 0x49, 0x3e, 0x81, 0x7e,
      0x66, 0xc7, 0xdb, 0x02, 0x73, 0x80, 0xdd, 0x6e,
      0xb7, 0x9a, 0x3a, 0x1c, 0x6e, 0x6e, 0xb5, 0x2b,
      0x1f, 0x78, 0x02, 0x1e, 0x44, 0x90, 0x65, 0xd9,
      0x8b, 0xef, 0xfb, 0xef, 0x5a, 0xeb, 0x33, 0x40,
      0x92, 0x24, 0x51, 0x9a, 0xa6, 0xcf, 0xc6, 0x98,
      0xae, 0x69, 0x9a, 0xd1, 0x26, 0x8f, 0x81, 0x8d,
      0x07, 0xa0, 0xaa, 0xaa, 0x3e, 0x0c, 0xc3, 0x5a,
      0x29, 0x15, 0x0b, 0xa7, 0x94, 0x8a, 0x8b, 0xa2,
      0xa8, 0xab, 0xaa, 0xea, 0x9d, 0x21, 0x36, 0x81,
      0xf3, 0x7b, 0x00, 0xe4, 0x79, 0x7e, 0x10, 0x03,
      0x40, 0x51, 0x14, 0xb5, 0x70, 0x0e, 0x96, 0x33,
      0xfe, 0x89, 0xc0, 0xde, 0x69, 0x2d, 0x44, 0x92,
      0x24, 0x91, 0x8c, 0xe7, 0x26, 0x4f, 0x52, 0x4f,
      0x81, 0x3d, 0xee, 0x5a, 0x3e, 0x47, 0x4c, 0xae,
      0x50, 0x29, 0x15, 0xb7, 0x6d, 0xfb, 0xe1, 0xec,
      0x79, 0xf4, 0x81, 0x0b, 0x70, 0x0f, 0x2c, 0x9a,
      0xa6, 0x19, 0x8d, 0x31, 0x5d, 0x59, 0x96, 0x47,
      0x31, 0x69, 0xad, 0xcf, 0xc6, 0x98, 0xce, 0x31,
      0x0d, 0x80, 0x96, 0xe6, 0x48, 0x01, 0xe6, 0xbf,
      0xac, 0x76, 0x01, 0x6a, 0x60, 0x2f, 0xcd, 0xe9,
      0x6d, 0x23, 0x6e, 0xed, 0x9d, 0xae, 0x61, 0x00,
      0x5e, 0xb1, 0x95, 0xf3, 0x26, 0x8f, 0x7f, 0x2e,
      0xf9, 0x17, 0x50, 0x59, 0x74, 0x13, 0x34, 0x41,
      0x04, 0x5a, 0x00, 0x00, 0x00, 0x20, 0x7a, 0x54,
      0x58, 0x74, 0x74, 0x69, 0x66, 0x66, 0x3a, 0x72,
      0x6f, 0x77, 0x73, 0x2d, 0x70, 0x65, 0x72, 0x2d,
      0x73, 0x74, 0x72, 0x69, 0x70, 0x00, 0x00, 0x78,
      0xda, 0x33, 0xb5, 0x30, 0x05, 0x00, 0x01, 0x47,
      0x00, 0xa3, 0x38, 0xda, 0x77, 0x3b, 0x00, 0x00,
      0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
      0x60, 0x82};

    // currently we only support the close bitmap here
    if ( button != wxTITLEBAR_BUTTON_CLOSE )
    {
        m_rendererNative.DrawTitleBarBitmap(win, dc, rect, button, flags);
        return;
    }

    // choose the correct image depending on flags
    const void *data;
    size_t len;

    if ( flags & wxCONTROL_PRESSED )
    {
        data = close_pressed_png;
        len = WXSIZEOF(close_pressed_png);
    }
    else if ( flags & wxCONTROL_CURRENT )
    {
        data = close_current_png;
        len = WXSIZEOF(close_current_png);
    }
    else
    {
        data = close_png;
        len = WXSIZEOF(close_png);
    }

    // load it
    wxMemoryInputStream mis(data, len);
    wxImage image(mis, wxBITMAP_TYPE_PNG);
    wxBitmap bmp(image);
    wxASSERT_MSG( bmp.IsOk(), "failed to load embedded PNG image?" );

    // and draw it centering it in the provided rectangle: we don't scale the
    // image because this is really not going to look good for such a small
    // (14*14) bitmap
    dc.DrawBitmap(bmp, wxRect(bmp.GetSize()).CenterIn(rect).GetPosition());
}

#endif // wxHAS_DRAW_TITLE_BAR_BITMAP

void wxRendererMac::DrawGauge(wxWindow* WXUNUSED(win),
                              wxDC& dc,
                              const wxRect& rect,
                              int value,
                              int max,
                              int WXUNUSED(flags))
{
    const wxCoord x = rect.x;
    const wxCoord y = rect.y;
    const wxCoord w = rect.width;
    const wxCoord h = rect.height;

    HIThemeTrackDrawInfo tdi;
    tdi.version = 0;
    tdi.min = 0;
    tdi.value = value;
    tdi.max = max;
    tdi.bounds = CGRectMake(x, y, w, h);
    tdi.attributes = kThemeTrackHorizontal;
    tdi.enableState = kThemeTrackActive;
    tdi.kind = kThemeLargeProgressBar;

    int milliSecondsPerStep = 1000 / 60;
    wxLongLongNative localTime = wxGetLocalTimeMillis();
    tdi.trackInfo.progress.phase = localTime.GetValue() / milliSecondsPerStep % 32;

    CGContextRef cgContext;
    wxGCDCImpl *impl = (wxGCDCImpl*) dc.GetImpl();

    cgContext = (CGContextRef) impl->GetGraphicsContext()->GetNativeContext();

    HIThemeDrawTrack(&tdi, NULL, cgContext, kHIThemeOrientationNormal);
}

#endif
