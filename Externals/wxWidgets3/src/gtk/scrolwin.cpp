/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/scrolwin.cpp
// Purpose:     wxScrolledWindow implementation
// Author:      Robert Roebling
// Modified by: Ron Lee
//              Vadim Zeitlin: removed 90% of duplicated common code
// Created:     01/02/97
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/scrolwin.h"

#include <gtk/gtk.h>
#include "wx/gtk/private/gtk2-compat.h"

// ----------------------------------------------------------------------------
// wxScrollHelper implementation
// ----------------------------------------------------------------------------

void wxScrollHelper::SetScrollbars(int pixelsPerUnitX, int pixelsPerUnitY,
                                   int noUnitsX, int noUnitsY,
                                   int xPos, int yPos,
                                   bool noRefresh)
{
    // prevent programmatic position changes from causing scroll events
    m_win->SetScrollPos(wxHORIZONTAL, xPos);
    m_win->SetScrollPos(wxVERTICAL, yPos);

    base_type::SetScrollbars(
        pixelsPerUnitX, pixelsPerUnitY, noUnitsX, noUnitsY, xPos, yPos, noRefresh);
}

void wxScrollHelper::DoAdjustScrollbar(GtkRange* range,
                                       int pixelsPerLine,
                                       int winSize,
                                       int virtSize,
                                       int *pos,
                                       int *lines,
                                       int *linesPerPage)
{
    if (!range)
        return;

    int upper;
    int page_size;
    if (pixelsPerLine > 0 && winSize > 0 && winSize < virtSize)
    {
        upper = (virtSize + pixelsPerLine - 1) / pixelsPerLine;
        page_size = winSize / pixelsPerLine;
        *lines = upper;
        *linesPerPage = page_size;
    }
    else
    {
        // GtkRange won't allow upper == lower, so for disabled state use [0,1]
        //   with a page size of 1. This will also clamp position to 0.
        upper = 1;
        page_size = 1;
        *lines = 0;
        *linesPerPage = 0;
    }

    GtkAdjustment* adj = gtk_range_get_adjustment(range);
    const double adj_upper = gtk_adjustment_get_upper(adj);
    const double adj_page_size = gtk_adjustment_get_page_size(adj);
    if (adj_upper != upper || adj_page_size != page_size)
    {
        const bool wasVisible = adj_upper > adj_page_size;

        g_object_freeze_notify(G_OBJECT(adj));
        gtk_range_set_increments(range, 1, page_size);
        gtk_adjustment_set_page_size(adj, page_size);
        gtk_range_set_range(range, 0, upper);
        g_object_thaw_notify(G_OBJECT(adj));

        const bool isVisible = gtk_adjustment_get_upper(adj) > gtk_adjustment_get_page_size(adj);
        if (isVisible != wasVisible)
            m_win->m_useCachedClientSize = false;
    }

    // ensure that the scroll position is always in valid range
    if (*pos > *lines)
        *pos = *lines;
}

void wxScrollHelper::AdjustScrollbars()
{
    int vw, vh;
    m_targetWindow->GetVirtualSize(&vw, &vh);
#ifdef __WXGTK3__
    if (m_targetWindow != m_win)
    {
        // setting wxPizza preferred size keeps GtkScrolledWindow from causing
        // an infinite sizing loop
        gtk_widget_set_size_request(m_win->m_wxwindow, vw, vh);
    }
#endif

    int w, h;
    const wxSize availSize = GetSizeAvailableForScrollTarget(
        m_win->GetSize() - m_win->GetWindowBorderSize());
    if ( availSize.x >= vw && availSize.y >= vh )
    {
        w = availSize.x;
        h = availSize.y;

        // we know that the scrollbars will be removed
        DoAdjustHScrollbar(w, vw);
        DoAdjustVScrollbar(h, vh);

        return;
    }

    m_targetWindow->GetClientSize(&w, NULL);
    DoAdjustHScrollbar(w, vw);

    m_targetWindow->GetClientSize(NULL, &h);
    DoAdjustVScrollbar(h, vh);

    const int w_old = w;
    m_targetWindow->GetClientSize(&w, NULL);
    if ( w != w_old )
    {
        // It is necessary to repeat the calculations in this case to avoid an
        // observed infinite series of size events, involving alternating
        // changes in visibility of the scrollbars.
        // At this point, GTK+ has already queued a resize, which will cause
        // AdjustScrollbars() to be called again. If the scrollbar visibility
        // is not correct before then, yet another resize will occur, possibly
        // leading to an unending series if the sizes are just right.
        DoAdjustHScrollbar(w, vw);

        m_targetWindow->GetClientSize(NULL, &h);
        DoAdjustVScrollbar(h, vh);
    }
}

void wxScrollHelper::DoScrollOneDir(int orient,
                                    int pos,
                                    int pixelsPerLine,
                                    int *posOld)
{
    if ( pos != -1 && pos != *posOld && pixelsPerLine )
    {
        m_win->SetScrollPos(orient, pos);
        pos = m_win->GetScrollPos(orient);

        int diff = (*posOld - pos)*pixelsPerLine;
        m_targetWindow->ScrollWindow(orient == wxHORIZONTAL ? diff : 0,
                                     orient == wxHORIZONTAL ? 0 : diff);

        *posOld = pos;
    }
}

void wxScrollHelper::DoScroll( int x_pos, int y_pos )
{
    wxCHECK_RET( m_targetWindow != 0, wxT("No target window") );

    DoScrollOneDir(wxHORIZONTAL, x_pos, m_xScrollPixelsPerLine, &m_xScrollPosition);
    DoScrollOneDir(wxVERTICAL, y_pos, m_yScrollPixelsPerLine, &m_yScrollPosition);
}

// ----------------------------------------------------------------------------
// scrollbars visibility
// ----------------------------------------------------------------------------

namespace
{

GtkPolicyType GtkPolicyFromWX(wxScrollbarVisibility visibility)
{
    GtkPolicyType policy;
    switch ( visibility )
    {
        case wxSHOW_SB_NEVER:
            policy = GTK_POLICY_NEVER;
            break;

        case wxSHOW_SB_DEFAULT:
            policy = GTK_POLICY_AUTOMATIC;
            break;

        default:
            wxFAIL_MSG( wxS("unknown scrollbar visibility") );
            // fall through

        case wxSHOW_SB_ALWAYS:
            policy = GTK_POLICY_ALWAYS;
            break;
    }

    return policy;
}

} // anonymous namespace

bool wxScrollHelper::IsScrollbarShown(int orient) const
{
    GtkScrolledWindow * const scrolled = GTK_SCROLLED_WINDOW(m_win->m_widget);
    if ( !scrolled )
    {
        // By default, all windows are scrollable.
        return true;
    }

    GtkPolicyType hpolicy, vpolicy;
    gtk_scrolled_window_get_policy(scrolled, &hpolicy, &vpolicy);

    GtkPolicyType policy = orient == wxHORIZONTAL ? hpolicy : vpolicy;

    return policy != GTK_POLICY_NEVER;
}

void wxScrollHelper::DoShowScrollbars(wxScrollbarVisibility horz,
                                      wxScrollbarVisibility vert)
{
    GtkScrolledWindow * const scrolled = GTK_SCROLLED_WINDOW(m_win->m_widget);
    wxCHECK_RET( scrolled, "window must be created" );

    gtk_scrolled_window_set_policy(scrolled,
                                   GtkPolicyFromWX(horz),
                                   GtkPolicyFromWX(vert));
}
