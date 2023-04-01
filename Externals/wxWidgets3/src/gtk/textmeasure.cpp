///////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/textmeasure.cpp
// Purpose:     wxTextMeasure implementation for wxGTK
// Author:      Manuel Martin
// Created:     2012-10-05
// Copyright:   (c) 1997-2012 wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/window.h"
    #include "wx/log.h"
#endif //WX_PRECOMP

#include "wx/private/textmeasure.h"

#include "wx/fontutil.h"
#include "wx/gtk/private.h"
#include "wx/gtk/dc.h"

#ifndef __WXGTK3__
    #include "wx/gtk/dcclient.h"
#endif

// ============================================================================
// wxTextMeasure implementation
// ============================================================================

void wxTextMeasure::Init()
{
    m_context = NULL;
    m_layout = NULL;

#ifndef __WXGTK3__
    m_wdc = NULL;

    if ( m_dc )
    {
        wxClassInfo* const ci = m_dc->GetImpl()->GetClassInfo();

        // Currently the code here only works with wxWindowDCImpl and only in
        // wxGTK2 as wxGTK3 uses Cairo and not Pango for all its DCs.
        if ( ci->IsKindOf(wxCLASSINFO(wxWindowDCImpl)))
        {
            m_useDCImpl = false;
        }
    }
#endif // GTK+ < 3
}

// Get Gtk needed elements, if we have not them yet.
void wxTextMeasure::BeginMeasuring()
{
    if ( m_dc )
    {
#ifndef __WXGTK3__
        m_wdc = wxDynamicCast(m_dc->GetImpl(), wxWindowDCImpl);
        if ( m_wdc )
        {
            m_context = m_wdc->m_context;
            m_layout = m_wdc->m_layout;
        }
#endif // GTK+ < 3
    }
    else if ( m_win )
    {
        m_context = gtk_widget_get_pango_context( m_win->GetHandle() );
        if ( m_context )
            m_layout = pango_layout_new(m_context);
    }

    // set the font to use
    if ( m_layout )
    {
        pango_layout_set_font_description(m_layout,
                                          GetFont().GetNativeFontInfo()->description);
    }
}

void wxTextMeasure::EndMeasuring()
{
    if ( !m_layout )
        return;

#ifndef __WXGTK3__
    if ( m_wdc )
    {
        // Reset dc own font description
        pango_layout_set_font_description( m_wdc->m_layout, m_wdc->m_fontdesc );
    }
    else
#endif // GTK+ < 3
    {
        g_object_unref (m_layout);
    }
}

// Notice we don't check here the font. It is supposed to be OK before the call.
void wxTextMeasure::DoGetTextExtent(const wxString& string,
                                    wxCoord *width,
                                    wxCoord *height,
                                    wxCoord *descent,
                                    wxCoord *externalLeading)
{
    if ( !m_context )
    {
        if ( width )
            *width = 0;

        if ( height )
            *height = 0;
        return;
    }

    // Set layout's text
    const wxCharBuffer dataUTF8 = wxGTK_CONV_FONT(string, GetFont());
    if ( !dataUTF8 && !string.empty() )
    {
        // hardly ideal, but what else can we do if conversion failed?
        wxLogLastError(wxT("GetTextExtent"));
        return;
    }
    pango_layout_set_text(m_layout, dataUTF8, -1);

    if ( m_dc )
    {
        // in device units
        pango_layout_get_pixel_size(m_layout, width, height);
    }
    else // win
    {
        // the logical rect bounds the ink rect
        PangoRectangle rect;
        pango_layout_get_extents(m_layout, NULL, &rect);
        *width = PANGO_PIXELS(rect.width);
        *height = PANGO_PIXELS(rect.height);
    }

    if (descent)
    {
        PangoLayoutIter *iter = pango_layout_get_iter(m_layout);
        int baseline = pango_layout_iter_get_baseline(iter);
        pango_layout_iter_free(iter);
        *descent = *height - PANGO_PIXELS(baseline);
    }

    if (externalLeading)
    {
        // No support for MSW-like "external leading" in Pango.
        *externalLeading = 0;
    }
}

bool wxTextMeasure::DoGetPartialTextExtents(const wxString& text,
                                            wxArrayInt& widths,
                                            double scaleX)
{
    if ( !m_layout )
        return wxTextMeasureBase::DoGetPartialTextExtents(text, widths, scaleX);

    // Set layout's text
    const wxCharBuffer dataUTF8 = wxGTK_CONV_FONT(text, GetFont());
    if ( !dataUTF8 )
    {
        // hardly ideal, but what else can we do if conversion failed?
        wxLogLastError(wxT("GetPartialTextExtents"));
        return false;
    }

    pango_layout_set_text(m_layout, dataUTF8, -1);

    // Calculate the position of each character based on the widths of
    // the previous characters

    // Code borrowed from Scintilla's PlatGTK
    PangoLayoutIter *iter = pango_layout_get_iter(m_layout);
    PangoRectangle pos;
    pango_layout_iter_get_cluster_extents(iter, NULL, &pos);
    size_t i = 0;
    while (pango_layout_iter_next_cluster(iter))
    {
        pango_layout_iter_get_cluster_extents(iter, NULL, &pos);
        int position = PANGO_PIXELS(pos.x);
        widths[i++] = position;
    }

    const size_t len = text.length();
    while (i < len)
        widths[i++] = PANGO_PIXELS(pos.x + pos.width);
    pango_layout_iter_free(iter);

    return true;
}
