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
                                    int flags = 0) wxOVERRIDE;

    virtual wxSize GetCollapseButtonSize(wxWindow *win, wxDC& dc) wxOVERRIDE;

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
                           int flags = 0) wxOVERRIDE;

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
    // currently we only support the close bitmap here
    if ( button != wxTITLEBAR_BUTTON_CLOSE )
    {
        m_rendererNative.DrawTitleBarBitmap(win, dc, rect, button, flags);
        return;
    }

    wxColour glyphColor;

    // The following hard coded RGB values are based the close button in
    // XCode 6+ welcome screen
    bool drawCircle;
    if ( flags & wxCONTROL_PRESSED )
    {
        drawCircle = true;
        glyphColor = wxColour(104, 104, 104);
        dc.SetPen(wxPen(wxColour(70, 70, 71), 1));
        dc.SetBrush(wxColour(78, 78, 78));
    }
    else if ( flags & wxCONTROL_CURRENT )
    {
        drawCircle = true;
        glyphColor = *wxWHITE;
        dc.SetPen(wxPen(wxColour(163, 165, 166), 1));
        dc.SetBrush(wxColour(182, 184, 187));
    }
    else
    {
        drawCircle = false;
        glyphColor = wxColour(145, 147, 149);
    }

    if ( drawCircle )
    {
        wxRect circleRect(rect);
        circleRect.Deflate(2);

        dc.DrawEllipse(circleRect);
    }

    dc.SetPen(wxPen(glyphColor, 1));

    wxRect centerRect(rect);
    centerRect.Deflate(5);
    centerRect.height++;
    centerRect.width++;

    dc.DrawLine(centerRect.GetTopLeft(), centerRect.GetBottomRight());
    dc.DrawLine(centerRect.GetTopRight(), centerRect.GetBottomLeft());
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
